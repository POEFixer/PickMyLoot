// LootWorker.h - Pick My Loot's scan worker: every game read of the pickup loop
// runs here, off the render thread. While picking is active it requests and
// reads the host's inventory listing for the item-panel gate, takes the item
// snapshot (and the monster snapshot for the enemy gate), resolves names,
// looks up prices and publishes an immutable LootFrame. The frame tick on the
// render thread only decides and sends the cursor move and the click.
//
// Inputs from the render thread: a heartbeat every frame tick (picking wanted
// or not) and a copy of the settings on every change. Without a recent
// heartbeat the worker idles and reads nothing.
#pragma once

#include "../../../../POEFixer/plugin_sdk/PluginSDK.h"

#include "../common/GameGates.h"
#include "../common/PanelGate.h"
#include "../config/Settings.h"
#include "LootScanner.h"
#include "LootTypes.h"
#include "PriceBook.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace PickMyLoot {

// One published pass. Never modified after publication.
struct LootFrame {
    uint64_t seq = 0;
    unsigned long long timeMs = 0;  // GetTickCount64() when the snapshot was taken
    uint64_t areaCounter = 0;
    bool inGame = false;            // in game with a valid player
    bool paused = false;
    bool dead = false;
    bool townOrHideout = false;
    bool enemyNear = false;         // a hostile monster within the enemy range (enemy gate on)
    bool itemsScanned = false;      // false when a gate above already stops picking
    float playerWX = 0.f, playerWY = 0.f, playerWZ = 0.f;
    std::vector<GroundItem> items;  // in pickup range, nearest first
};

class LootWorker {
public:
    explicit LootWorker(const PluginSDK::Context* ctx) : m_ctx(ctx) {}
    ~LootWorker() { Stop(); }
    LootWorker(const LootWorker&) = delete;
    LootWorker& operator=(const LootWorker&) = delete;

    void Start() {
        if (m_thread.joinable()) return;
        {
            std::lock_guard<std::mutex> lk(m_wakeMutex);
            m_stop = false;
        }
        ResetState();
        m_thread = std::thread(&LootWorker::Loop, this);
    }

    // Joins the thread: after Stop() returns the worker makes no host call.
    void Stop() {
        {
            std::lock_guard<std::mutex> lk(m_wakeMutex);
            m_stop = true;
        }
        m_wakeCv.notify_all();
        if (m_thread.joinable()) m_thread.join();
        ResetState();
    }

    // Render thread: after loading and on every settings change.
    void SetSettings(std::shared_ptr<const PickMyLootConfig::Settings> s) {
        std::lock_guard<std::mutex> lk(m_inMutex);
        m_settings = std::move(s);
    }

    // Render thread, every frame tick: true while picking is on, the game is in
    // front and in game, and the POEFixer menu is closed.
    void Heartbeat(bool active) {
        m_beatMs.store(::GetTickCount64(), std::memory_order_relaxed);
        const bool was = m_active.exchange(active, std::memory_order_acq_rel);
        if (active && !was) m_wakeCv.notify_all();
    }

    std::shared_ptr<const LootFrame> Latest() const {
        std::lock_guard<std::mutex> lk(m_outMutex);
        return m_frame;
    }

    // Fail-safe item-panel verdict: a listing never read or older than
    // PanelGate::kStaleMs counts as open.
    bool PanelOpen(unsigned long long nowMs) const {
        return PfGates::PanelGate::Verdict(m_panelOpen.load(std::memory_order_acquire),
                                           m_panelReadAt.load(std::memory_order_acquire), nowMs);
    }
    bool PanelLastListing() const { return m_panelOpen.load(std::memory_order_relaxed); }
    int PricesRemembered() const { return m_pricesKnown.load(std::memory_order_relaxed); }
    uint64_t Faults() const { return m_faults.load(std::memory_order_relaxed); }

private:
    static constexpr unsigned long long kHeartbeatTimeoutMs = 400;
    static constexpr unsigned long long kEnemyCheckMs = 150;
    static constexpr int kPriceLookupsPerPass = 32;

    void ResetState() {
        m_scanner.ClearCache();
        m_prices.Clear();
        m_gate.Reset();
        m_panelOpen.store(false, std::memory_order_relaxed);
        m_panelReadAt.store(0, std::memory_order_release);
        m_lastPass = 0;
        m_lastEnemyCheck = 0;
        m_enemyNear = false;
        m_area = ~0ull;
        m_wasActive = false;
        std::lock_guard<std::mutex> lk(m_outMutex);
        m_frame.reset();
    }

    void Loop() {
        for (;;) {
            {
                std::lock_guard<std::mutex> lk(m_wakeMutex);
                if (m_stop) return;
            }
            // This is the plugin's own thread: the host's SEH bridge guards only
            // host -> plugin calls, so a structured fault or a C++ exception here
            // would end the whole program. Every tick is guarded twice and a
            // faulting tick costs only that pass (and a one-second pause).
            if (TickSEH(this) == 0) {
                m_faults.fetch_add(1, std::memory_order_relaxed);
                m_scanner.ClearCache();
                m_prices.Clear();
                m_gate.Reset();
                m_panelReadAt.store(0, std::memory_order_release);
                m_backoffUntil = ::GetTickCount64() + 1000;
            }
            const bool active = IsActive(::GetTickCount64());
            std::unique_lock<std::mutex> lk(m_wakeMutex);
            if (m_stop) return;
            m_wakeCv.wait_for(lk, std::chrono::milliseconds(active ? 10 : 50));
        }
    }

    // SEH layer: no locals that need unwinding (C2712), it only forwards.
    static int TickSEH(LootWorker* self) {
        __try {
            return self->TickGuarded();
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            return 0;
        }
    }

    int TickGuarded() {
        try {
            Tick();
            return 1;
        } catch (...) {
            return 0;
        }
    }

    bool IsActive(unsigned long long now) const {
        return m_active.load(std::memory_order_acquire) &&
               now - m_beatMs.load(std::memory_order_relaxed) < kHeartbeatTimeoutMs;
    }

    void Tick() {
        const unsigned long long now = ::GetTickCount64();
        if (now < m_backoffUntil || !m_ctx) return;
        if (!IsActive(now)) {
            if (m_wasActive) {
                // A pending scan request would be read as fresh on return:
                // forget it (the next active pass starts with a new request).
                m_gate.Reset();
                m_panelReadAt.store(0, std::memory_order_release);
                m_wasActive = false;
            }
            return;
        }
        m_wasActive = true;

        std::shared_ptr<const PickMyLootConfig::Settings> cfg;
        {
            std::lock_guard<std::mutex> lk(m_inMutex);
            cfg = m_settings;
        }
        if (!cfg) return;

        if (cfg->gatePanelOpen) {
            StepPanelGate(now);
        } else if (m_gate.LastReadAt() != 0 || m_gate.Pending()) {
            // Gate switched off: drop its state, so switching it on again
            // starts from "not read yet" (counts as open) with a new request.
            m_gate.Reset();
            m_panelReadAt.store(0, std::memory_order_release);
        }

        const unsigned long long interval =
            static_cast<unsigned long long>(std::max(cfg->actionIntervalMs, PickMyLootConfig::kActionIntervalMinMs));
        if (m_lastPass != 0 && now - m_lastPass < interval) return;
        m_lastPass = now;
        Pass(*cfg, now);
    }

    void StepPanelGate(unsigned long long now) {
        switch (m_gate.Next(now, true)) {
            case PfGates::PanelGate::Step::RequestScan:
                m_ctx->Inventory.Scan(-1);  // only flags the host worker; returns at once
                m_gate.OnRequested(now);
                break;
            case PfGates::PanelGate::Step::ReadListing: {
                const bool open = PfGates::AnyItemPanelOpen(m_ctx);
                m_gate.OnRead(now, open);
                m_panelOpen.store(open, std::memory_order_relaxed);
                m_panelReadAt.store(now, std::memory_order_release);
                break;
            }
            case PfGates::PanelGate::Step::Wait:
                break;
        }
    }

    void Pass(const PickMyLootConfig::Settings& cfg, unsigned long long now) {
        auto frame = std::make_shared<LootFrame>();
        frame->timeMs = now;
        // Player + world items only: the host drops every other entity before
        // copying its strings, which is most of the cost of a full snapshot.
        const PluginSDK::Snapshot snap = m_ctx->Game.GetSnapshot(PluginSDK::EntityType::Item);
        frame->areaCounter = snap.AreaChangeCounter;
        frame->inGame = snap.State == PluginSDK::GameState::InGame && snap.Player.IsValid;
        if (frame->inGame) {
            frame->playerWX = snap.Player.WorldX;
            frame->playerWY = snap.Player.WorldY;
            frame->playerWZ = snap.Player.WorldZ;
            if (snap.AreaChangeCounter != m_area) {
                m_area = snap.AreaChangeCounter;
                m_scanner.ClearCache();  // item entities are re-created in a new area
                m_lastEnemyCheck = 0;
                m_enemyNear = false;
            }
            frame->paused = snap.IsPaused;
            frame->dead = PfGates::PlayerDead(snap);
            frame->townOrHideout = snap.IsTown || snap.IsHideout;
            const bool stopped = frame->paused || frame->dead || (cfg.gateTownHideout && frame->townOrHideout);
            if (!stopped && cfg.gateEnemyNear) frame->enemyNear = EnemyNear(cfg, snap, now);
            if (!stopped && !frame->enemyNear) ScanItems(cfg, snap, now, *frame);
        }
        Publish(std::move(frame));
    }

    bool EnemyNear(const PickMyLootConfig::Settings& cfg, const PluginSDK::Snapshot& itemSnap,
                   unsigned long long now) {
        if (m_lastEnemyCheck != 0 && now - m_lastEnemyCheck < kEnemyCheckMs) return m_enemyNear;
        m_lastEnemyCheck = now;
        m_enemyNear = false;
        // Monsters only (filtered by the host before any string is copied).
        const PluginSDK::Snapshot mons = m_ctx->Game.GetSnapshot(PluginSDK::EntityType::Monster);
        const float px = itemSnap.Player.GridPositionX;
        const float py = itemSnap.Player.GridPositionY;
        const float r = static_cast<float>(cfg.enemyRange);
        for (const auto& e : mons.Entities) {
            if (!PfGates::IsHostileMonster(e)) continue;
            const float dx = e.GridPositionX - px, dy = e.GridPositionY - py;
            if (dx * dx + dy * dy <= r * r) {
                m_enemyNear = true;
                break;
            }
        }
        return m_enemyNear;
    }

    void ScanItems(const PickMyLootConfig::Settings& cfg, const PluginSDK::Snapshot& snap, unsigned long long now,
                   LootFrame& frame) {
        const PluginSDK::ScreenSize disp = m_ctx->Game.GetScreenSize();
        if (disp.Width < 16.f || disp.Height < 16.f) return;
        m_prices.BeginFrame(m_ctx, kPriceLookupsPerPass, now);
        m_scanner.Scan(m_ctx, snap, cfg.pickupRange, disp.Width, disp.Height, now, frame.items);
        const double threshold = cfg.EffectiveThreshold();
        for (GroundItem& g : frame.items) {
            const PriceEntry* p = m_prices.Get(m_ctx, g.baseName);
            if (p && p->known) g.priceCategory = p->category;
            if (cfg.NameEnabled(g.baseName)) {
                g.wanted = true;
            } else if (cfg.valueFilterEnabled && p && p->known) {
                const double v = p->In(cfg.referenceCurrency);
                g.wanted = v >= 0.0 && v >= threshold;
            }
        }
        frame.itemsScanned = true;
        m_pricesKnown.store(static_cast<int>(m_prices.Size()), std::memory_order_relaxed);
    }

    void Publish(std::shared_ptr<LootFrame> frame) {
        frame->seq = ++m_seq;
        std::lock_guard<std::mutex> lk(m_outMutex);
        m_frame = std::move(frame);
    }

    const PluginSDK::Context* m_ctx = nullptr;
    std::thread m_thread;
    std::mutex m_wakeMutex;
    std::condition_variable m_wakeCv;
    bool m_stop = false;

    // Inputs.
    mutable std::mutex m_inMutex;
    std::shared_ptr<const PickMyLootConfig::Settings> m_settings;
    std::atomic<bool> m_active{false};
    std::atomic<unsigned long long> m_beatMs{0};

    // Outputs.
    mutable std::mutex m_outMutex;
    std::shared_ptr<const LootFrame> m_frame;
    std::atomic<bool> m_panelOpen{false};
    std::atomic<unsigned long long> m_panelReadAt{0};
    std::atomic<int> m_pricesKnown{0};
    std::atomic<uint64_t> m_faults{0};

    // Worker-thread state.
    LootScanner m_scanner;
    PriceBook m_prices;
    PfGates::PanelGate m_gate;
    uint64_t m_seq = 0;
    uint64_t m_area = ~0ull;
    unsigned long long m_lastPass = 0;
    unsigned long long m_lastEnemyCheck = 0;
    unsigned long long m_backoffUntil = 0;
    bool m_enemyNear = false;
    bool m_wasActive = false;
};

}  // namespace PickMyLoot
