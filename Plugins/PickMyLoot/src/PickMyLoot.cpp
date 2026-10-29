// PickMyLoot.cpp - lifecycle and the per-frame pickup tick.
//
// Pick My Loot, originally created by Omer Faruk ARPA
// (https://github.com/omrfarukarpa/poefixer-pickmyloot), maintained by POEFixer.
// Licensed under the GNU General Public License v3.0 (see LICENSE).
//
// Texts follow the program language (PluginI18n.h): lang/<code>.json next to the DLL, English
// compiled in through lang_en.generated.h (python tools/plugin_i18n/gen.py Plugins/PickMyLoot
// after editing lang/en.json). The game's item names stay as the game writes them.
#include "../../../POEFixer/plugin_sdk/PluginSDK.h"
#include "lang_en.generated.h"   // the embedded English: once per DLL, after PluginSDK.h

#include "PickMyLootPlugin.h"

#include "common/GameGates.h"
#include "input/Win32Input.h"

#include <imgui.h>

#include <algorithm>
#include <cmath>

namespace I18n = PluginSDK::I18n;

namespace {
unsigned long long NowMs() { return ::GetTickCount64(); }
}  // namespace

const char* PickMyLootPlugin::GetName() const { return PluginSDK_GetInfo()->name; }

bool PickMyLootPlugin::WantsOverlay() const {
    return m_settings.showOverlay || m_settings.showRangeCircle || m_settings.showEnemyCircle;
}

const char* PickMyLootPlugin::StateText(State s) {
    switch (s) {
        case State::Off:          return I18n::Tr("state_off");
        case State::Binding:      return I18n::Tr("state_binding");
        case State::NotInGame:    return I18n::Tr("state_waiting_game");
        case State::MenuOpen:     return I18n::Tr("state_paused_menu");
        case State::PanelOpen:    return I18n::Tr("state_paused_panel");
        case State::GamePaused:   return I18n::Tr("state_paused_game");
        case State::Dead:         return I18n::Tr("state_paused_dead");
        case State::TownHideout:  return I18n::Tr("state_paused_town");
        case State::EnemyNear:    return I18n::Tr("state_paused_enemy");
        case State::UserClicking: return I18n::Tr("state_paused_clicking");
        case State::NoTarget:     return I18n::Tr("state_nothing");
        case State::Cooldown:     return I18n::Tr("state_picking");
        case State::Aiming:       return I18n::Tr("state_picking");
        case State::Clicked:      return I18n::Tr("state_picking");
    }
    return "";
}

void PickMyLootPlugin::EnsureLoaded() {
    if (m_loaded) return;
    const std::filesystem::path dir = DirectoryPath();
    if (dir.empty()) return;
    const PfIo::LoadStatus st = m_settings.Load(dir);
    m_saveGuard.OnLoad(st);
    if (st == PfIo::LoadStatus::Corrupt)
        ctx()->Log.Warn("Pick My Loot: config/settings.json is not valid JSON - kept as settings.json.corrupt; "
                        "running on defaults, nothing is saved until a setting is changed");
    else if (st == PfIo::LoadStatus::Unreadable)
        ctx()->Log.Warn("Pick My Loot: config/settings.json exists but could not be read - running on defaults "
                        "without overwriting it; it is read again later");
    m_catalog.Load(dir);
    m_loaded = true;
}

void PickMyLootPlugin::MarkDirty() {
    m_dirty = true;
    m_dirtyAt = NowMs();
    m_saveGuard.OnUserChange();  // the user's own change may replace an unreadable file
    PushSettings();
}

void PickMyLootPlugin::PushSettings() {
    m_worker.SetSettings(std::make_shared<const PickMyLootConfig::Settings>(m_settings));
}

void PickMyLootPlugin::RetrySettingsLoad(const std::filesystem::path& dir) {
    PickMyLootConfig::Settings fresh;
    const PfIo::LoadStatus st = fresh.Load(dir);
    if (st == PfIo::LoadStatus::Unreadable) return;  // still locked: try again at the next save
    if (st == PfIo::LoadStatus::Loaded) {
        m_settings = std::move(fresh);
        PushSettings();
        ctx()->Log.Info("Pick My Loot: config/settings.json could be read now - its settings are in use");
    }
    m_saveGuard.OnLoad(st);
}

void PickMyLootPlugin::OnKeyBound(int vk) {
    m_waitReleaseVk = vk;
    MarkDirty();
}

void PickMyLootPlugin::OnEnable(bool) {
    if (!HostCompatible()) {
        ctx()->Log.Error("Pick My Loot: this POEFixer version is not compatible with the plugin - disabled");
        return;
    }
    if (ctx()->ImGuiContext) ImGui::SetCurrentContext(static_cast<ImGuiContext*>(ctx()->ImGuiContext));
    EnsureLoaded();
    m_active = false;
    m_togglePrev = true;  // a key still held at enable time must not start picking
    m_state = State::Off;
    m_areaCounter = ctx()->Game.GetAreaChangeCounter();
    ResetSession();
    m_frame.reset();
    m_learnedSeq = 0;
    m_havePlayer = false;
    m_prices.Clear();
    PushSettings();
    m_worker.Start();

    auto& events = const_cast<PluginSDK::EventsService&>(ctx()->Events);
    if (!m_frameTok.Valid()) m_frameTok = events.OnFrame([this] {
        try {
            FrameTick();
        } catch (...) {
            m_active = false;
            StopPicking(State::Off, NowMs());
        }
    });
    m_enabled = true;
    ctx()->Log.Info("Pick My Loot enabled");
}

void PickMyLootPlugin::OnDisable() {
    auto& events = const_cast<PluginSDK::EventsService&>(ctx()->Events);
    if (m_frameTok.Valid()) events.Unsubscribe(m_frameTok);
    m_frameTok = {};
    m_worker.Stop();  // joined: no host call from the worker after this
    m_active = false;
    // Disabling from the POEFixer menu: put the cursor back only if the game is
    // in front, and release the aim plugin at once.
    if (m_haveSaved && ctx()->Game.IsForeground()) PfInput::MoveCursorDesktop(m_saved);
    ForgetCursor();
    m_pickBusy.MarkIdle();
    m_busyMarked = false;
    m_phase = Phase::Idle;
    m_state = State::Off;
    m_enabled = false;
    m_frame.reset();
    m_havePlayer = false;
    m_prices.Clear();
    SaveSettings();
    ctx()->Log.Info("Pick My Loot disabled");
}

void PickMyLootPlugin::SaveSettings() {
    if (!m_loaded) return;
    const std::filesystem::path dir = DirectoryPath();
    if (m_saveGuard.ShouldRetryLoad()) RetrySettingsLoad(dir);
    if (m_saveGuard.MayWrite() && m_settings.Save(dir)) m_dirty = false;
    if (m_catalog.Dirty()) m_catalog.Save(dir);
}

void PickMyLootPlugin::ForgetCursor() {
    m_haveSaved = false;
    m_restorePending = false;
}

void PickMyLootPlugin::ResetSession() {
    m_phase = Phase::Idle;
    m_targetId = 0;
    m_lastAction = 0;
    m_lastClick = 0;
    m_lastClickedId = 0;
    m_lastClickedAt = 0;
    m_attempts.clear();
    m_skipped.clear();
    m_wantedVisible = 0;
}

void PickMyLootPlugin::StopPicking(State s, unsigned long long now) {
    m_state = s;
    m_phase = Phase::Idle;
    m_targetId = 0;
    if (m_busyMarked) {
        m_pickBusy.MarkIdle();
        m_busyMarked = false;
    }
    if (!m_haveSaved) return;
    // Give the game time to take the last click at the item before the cursor
    // goes back; the next tick finishes the restore.
    if (m_lastClick != 0 && now - m_lastClick < kRestoreDelayMs) {
        m_restorePending = true;
        return;
    }
    PfInput::MoveCursorDesktop(m_saved);
    ForgetCursor();
}

void PickMyLootPlugin::LearnFromFrame(const PickMyLoot::LootFrame& frame, unsigned long long now) {
    if (frame.seq == m_learnedSeq) return;
    m_learnedSeq = frame.seq;
    // New league items: a base the catalog does not list joins the category
    // the price service files it under, so it shows up in the lists. Both go by
    // the English identity name; the name the game showed feeds the lists' display.
    for (const auto& g : frame.items) {
        if (g.englishName.empty()) continue;
        m_seenNames.Note(g.englishName, g.baseName);
        if (g.priceCategory == PickMyLoot::Category::Count || m_catalog.Knows(g.englishName)) continue;
        m_catalog.Learn(g.englishName, g.priceCategory);
    }
    if (m_catalog.Dirty() && now - m_catalogSavedAt > 30000) {
        m_catalogSavedAt = now;
        m_catalog.Save(DirectoryPath());
    }
}

// Render thread, every frame. Every game read (inventory listing, entity
// snapshots, names, prices) happens on the worker (game/LootWorker.h); this
// only reads keys and cheap host scalars, picks the target from the latest
// worker frame and sends the cursor move and the click.
void PickMyLootPlugin::FrameTick() {
    if (!HostCompatible()) return;
    const unsigned long long now = NowMs();
    if (m_waitReleaseVk != 0 && !PfInput::IsKeyDown(m_waitReleaseVk)) m_waitReleaseVk = 0;
    const bool keyJustBound = m_waitReleaseVk != 0;

    const bool foreground = ctx()->Game.IsForeground();
    const bool menu = ctx()->Game.IsMenuVisible();

    if (m_restorePending && m_phase == Phase::Idle && now - m_lastClick >= kRestoreDelayMs) {
        if (foreground) PfInput::MoveCursorDesktop(m_saved);
        ForgetCursor();
    }

    const bool toggleDown = foreground && !menu && !keyJustBound && PfInput::IsKeyDown(m_settings.toggleKey);
    if (toggleDown && !m_togglePrev) {
        m_active = !m_active;
        m_attempts.clear();
        m_skipped.clear();
        if (!m_active) StopPicking(State::Off, now);
    }
    m_togglePrev = toggleDown;

    const bool inGame = foreground && ctx()->Game.IsInGame();
    m_worker.Heartbeat(m_active && !keyJustBound && inGame && !menu);

    if (keyJustBound) { StopPicking(State::Binding, now); return; }
    if (!m_active) { StopPicking(State::Off, now); return; }
    if (!inGame) {
        // Never move the cursor of another window: forget the saved position.
        ForgetCursor();
        StopPicking(State::NotInGame, now);
        return;
    }
    if (menu) { StopPicking(State::MenuOpen, now); return; }

    const unsigned long long interval = static_cast<unsigned long long>(m_settings.actionIntervalMs);
    if (now - m_lastAction < interval) return;
    m_lastAction = now;

    if (m_lastClick != 0 && now - m_lastClick < static_cast<unsigned long long>(m_settings.pauseBetweenClicksMs)) {
        m_state = State::Cooldown;
        return;
    }
    // Fail safe: until the worker has read a fresh inventory listing (picking
    // just started, the host has not scanned yet), a panel counts as open.
    if (m_settings.gatePanelOpen && m_worker.PanelOpen(now)) { StopPicking(State::PanelOpen, now); return; }

    const std::shared_ptr<const PickMyLoot::LootFrame> frame = m_worker.Latest();
    const unsigned long long maxAge = std::max(kFrameMaxAgeMs, interval * 3);
    if (!frame || now < frame->timeMs || now - frame->timeMs > maxAge) {
        // No fresh scan yet (picking just started or the worker is behind).
        StopPicking(State::NoTarget, now);
        return;
    }
    m_frame = frame;
    if (!frame->inGame) {
        m_havePlayer = false;
        StopPicking(State::NotInGame, now);
        return;
    }
    m_havePlayer = true;
    m_playerWX = frame->playerWX;
    m_playerWY = frame->playerWY;
    m_playerWZ = frame->playerWZ;

    if (frame->areaCounter != m_areaCounter) {
        m_areaCounter = frame->areaCounter;
        ForgetCursor();
        StopPicking(State::NoTarget, now);
        ResetSession();
    }
    // A frame from the area the player just left: its items and positions
    // belong to another map. Wait for the worker's next pass.
    if (ctx()->Game.GetAreaChangeCounter() != frame->areaCounter) { StopPicking(State::NoTarget, now); return; }
    if (frame->paused) { StopPicking(State::GamePaused, now); return; }
    if (frame->dead) { StopPicking(State::Dead, now); return; }
    if (m_settings.gateTownHideout && frame->townOrHideout) { StopPicking(State::TownHideout, now); return; }
    if (m_settings.gateEnemyNear && frame->enemyNear) { StopPicking(State::EnemyNear, now); return; }
    // Our own click is a left-button press too: ignore the button right after it.
    if (m_settings.pauseWhileUserClicks && PfInput::IsKeyDown(VK_LBUTTON) &&
        (m_lastClick == 0 || now - m_lastClick > 150)) {
        StopPicking(State::UserClicking, now);
        return;
    }
    if (!frame->itemsScanned) { StopPicking(State::NoTarget, now); return; }

    const PluginSDK::ScreenSize disp = ctx()->Game.GetScreenSize();
    if (disp.Width < 16.f || disp.Height < 16.f) { StopPicking(State::NotInGame, now); return; }

    LearnFromFrame(*frame, now);

    // The worker's screen positions are as old as its snapshot; the camera
    // follows the player, so the chosen label is projected again now (one
    // matrix multiply per candidate, no entity read).
    const PickMyLoot::GroundItem* target = nullptr;
    float targetX = 0.f, targetY = 0.f;
    m_wantedVisible = 0;
    for (const auto& g : frame->items) {
        if (!g.onScreen || !g.wanted) continue;
        if (m_skipped.count(g.id)) continue;
        ++m_wantedVisible;
        if (target) continue;
        if (g.id == m_lastClickedId && now - m_lastClickedAt < kSameItemRetryDelayMs) continue;
        float sx = 0.f, sy = 0.f;
        if (!ctx()->Render.WorldToScreen(g.worldX, g.worldY, g.worldZ, sx, sy)) continue;
        if (sx < 0.f || sy < 0.f || sx >= disp.Width || sy >= disp.Height) continue;
        target = &g;
        targetX = sx;
        targetY = sy;
    }
    if (!target) { StopPicking(State::NoTarget, now); return; }

    const int clickX = static_cast<int>(std::lround(targetX));
    const int clickY = std::max(0, static_cast<int>(std::lround(targetY)) - m_settings.labelClickOffsetY);
    const HWND wnd = ctx()->Game.GetGameWindow();

    if (m_phase == Phase::Idle || target->id != m_targetId) {
        m_restorePending = false;
        if (m_settings.restoreCursor && !m_haveSaved) m_haveSaved = PfInput::GetCursorDesktop(m_saved);
        PfInput::MoveCursorClient(wnd, clickX, clickY);
        m_pickBusy.MarkBusy(750);
        m_busyMarked = true;
        m_targetId = target->id;
        m_armTime = now;
        m_phase = Phase::Armed;
        m_state = State::Aiming;
        if (m_settings.hoverSettleMs > 0) return;
    }

    if (now - m_armTime >= static_cast<unsigned long long>(m_settings.hoverSettleMs)) {
        PfInput::LeftClickClient(wnd, clickX, clickY);
        m_pickBusy.MarkBusy(750);
        m_busyMarked = true;
        m_lastClick = now;
        m_lastClickedId = target->id;
        m_lastClickedAt = now;
        // An item that is still there after several clicks cannot be picked
        // (inventory full, label hidden by the loot filter, out of reach):
        // stop clicking it until the area changes or picking is restarted.
        if (++m_attempts[target->id] >= kMaxAttemptsPerItem) m_skipped.insert(target->id);
        m_phase = Phase::Idle;
        m_targetId = 0;
        m_state = State::Clicked;
    }
}

extern "C" PLUGIN_API PluginSDK::Plugin* CreatePlugin() { return new PickMyLootPlugin(); }
extern "C" PLUGIN_API void DestroyPlugin(PluginSDK::Plugin* p) { delete p; }

// Plugin info: the host checks it before CreatePlugin; the publish tool reads it.
POEFIXER_PLUGIN_INFO(
    "PickMyLoot",
    "Pick My Loot",
    "1.2.5",
    "Omer Faruk ARPA, POEFixer",
    "Picks up the ground items you tick, or anything above a price you set, nearest first, using POEFixer prices; pauses on panels, enemies, town or menu.")
