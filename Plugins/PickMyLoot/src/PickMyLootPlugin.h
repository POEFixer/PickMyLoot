// PickMyLootPlugin.h - the Pick My Loot plugin: after the start key it picks
// up the ground items you chose (and optionally anything worth more than a
// value), nearest first, by moving the cursor onto the item label and clicking.
#pragma once

#include "../../../POEFixer/plugin_sdk/PluginSDK.h"

#include "config/Settings.h"
#include "game/LootCatalog.h"
#include "game/LootTypes.h"
#include "game/LootWorker.h"
#include "game/PriceBook.h"
#include "ipc/Coordination.h"

#include <imgui.h>

#include <array>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class PickMyLootPlugin : public PluginSDK::Plugin {
public:
    const char* GetName() const override;
    bool WantsOverlay() const override;
    void OnEnable(bool gameAttached) override;
    void OnDisable() override;
    void SaveSettings() override;
    void DrawUI() override;
    void DrawSettings() override;

private:
    enum class State {
        Off,           // not started
        Binding,       // the start key was just bound and is still held
        NotInGame,     // loading, not foreground, or not in game
        MenuOpen,      // the POEFixer menu is shown
        PanelOpen,
        GamePaused,
        Dead,
        TownHideout,
        EnemyNear,
        UserClicking,
        NoTarget,
        Cooldown,
        Aiming,        // cursor on the label, waiting for the hover to settle
        Clicked,
    };
    static const char* StateText(State s);
    static bool StateIsActive(State s) { return s != State::Off && s != State::Binding; }

    enum class Phase { Idle, Armed };

    void EnsureLoaded();
    void MarkDirty();
    // Hands the worker a copy of the current settings.
    void PushSettings();
    // The settings file was locked at load: read it again, adopt it if it reads.
    void RetrySettingsLoad(const std::filesystem::path& dir);
    // The start key changed: save it, and ignore it until it is released
    // (binding it must not also start picking).
    void OnKeyBound(int vk);
    void FrameTick();
    void SetState(State s) { m_state = s; }
    // Ends the current pick: releases the busy channel and puts the cursor back
    // (when that option is on), never sooner than kRestoreDelayMs after a click.
    void StopPicking(State s, unsigned long long now);
    void ForgetCursor();
    void ResetSession();
    // Teaches the catalog the league items a new frame priced (once per frame).
    void LearnFromFrame(const PickMyLoot::LootFrame& frame, unsigned long long now);

    // Overlay (ui/Overlay.cpp).
    void DrawGroundRing(ImDrawList* dl, float wx, float wy, float wz, float gridRadius, const float color[4],
                        float scale);

    // Settings sections (ui/SettingsUi.cpp).
    void DrawHeader();
    void DrawSectionWhatToPick();
    void DrawCategory(int index);
    void DrawSectionValue();
    void DrawSectionSpeed();
    void DrawSectionSafety();
    void DrawSectionDisplay();
    void DrawSectionDiagnostics();

    static constexpr unsigned long long kSameItemRetryDelayMs = 500;
    static constexpr unsigned long long kRestoreDelayMs = 60;
    static constexpr int kMaxAttemptsPerItem = 4;
    // A worker frame older than this (or than three scan intervals) is not acted on.
    static constexpr unsigned long long kFrameMaxAgeMs = 500;

    PickMyLootConfig::Settings      m_settings;
    PfIo::SaveGuard                 m_saveGuard;
    PickMyLoot::LootCatalog         m_catalog;  // render thread only
    PickMyLoot::PriceBook           m_prices;   // settings lists only (the worker has its own)
    PoeFixerIpc::PickBusyChannel    m_pickBusy;
    PluginSDK::EventsService::Token m_frameTok;

    bool  m_loaded = false;
    bool  m_enabled = false;
    bool  m_dirty = false;
    unsigned long long m_dirtyAt = 0;
    unsigned long long m_catalogSavedAt = 0;

    bool  m_active = false;
    bool  m_togglePrev = false;
    int   m_waitReleaseVk = 0;
    unsigned long long m_keyRefusedAt = 0;  // a mouse button was offered as the start key
    State m_state = State::Off;

    Phase    m_phase = Phase::Idle;
    uint32_t m_targetId = 0;
    unsigned long long m_lastAction = 0;
    unsigned long long m_lastClick = 0;
    unsigned long long m_armTime = 0;
    uint32_t m_lastClickedId = 0;
    unsigned long long m_lastClickedAt = 0;
    bool     m_busyMarked = false;

    POINT m_saved{};
    bool  m_haveSaved = false;
    bool  m_restorePending = false;

    uint64_t m_areaCounter = 0;
    std::unordered_map<uint32_t, int> m_attempts;
    std::unordered_set<uint32_t>      m_skipped;

    // The last worker frame the tick used (overlay and diagnostics read it).
    std::shared_ptr<const PickMyLoot::LootFrame> m_frame;
    uint64_t m_learnedSeq = 0;
    int   m_wantedVisible = 0;
    float m_playerWX = 0.f, m_playerWY = 0.f, m_playerWZ = 0.f;
    bool  m_havePlayer = false;

    // Settings UI state.
    std::array<std::string, PickMyLoot::kCategoryCount> m_filter;
    std::array<int, PickMyLoot::kCategoryCount>         m_tickedCount{};
    unsigned long long m_tickedCountAt = 0;
    PluginSDK::PriceStatus m_priceStatus;
    unsigned long long m_priceStatusAt = 0;

    // Last member: destroyed (joined) first, before anything it could touch.
    PickMyLoot::LootWorker          m_worker{ctx()};
};
