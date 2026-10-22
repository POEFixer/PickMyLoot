// Overlay.cpp - Pick My Loot in-game drawing: the status chip and the pickup /
// enemy range rings.
#include "../PickMyLootPlugin.h"

#include "../common/GameGates.h"
#include "../common/OverlayDraw.h"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

void PickMyLootPlugin::DrawUI() {
    if (!m_enabled || !ctx()->ImGuiContext) return;
    if (!m_settings.showOverlay && !m_settings.showRangeCircle && !m_settings.showEnemyCircle) return;
    ImGui::SetCurrentContext(static_cast<ImGuiContext*>(ctx()->ImGuiContext));
    if (!ctx()->Game.IsInGame() || !ctx()->Game.IsForeground()) return;

    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    const float scale = m_settings.overlayScale;

    if (m_settings.showRangeCircle || m_settings.showEnemyCircle) {
        // The player position from the last tick while picking; otherwise a
        // cheap player read (no entity enumeration).
        float wx = m_playerWX, wy = m_playerWY, wz = m_playerWZ;
        bool ok = m_havePlayer && StateIsActive(m_state);
        if (!ok) {
            const PluginSDK::Entity p = ctx()->Entities.GetPlayer();
            ok = p.IsValid;
            wx = p.WorldX;
            wy = p.WorldY;
            wz = p.WorldZ;
        }
        if (ok) {
            if (m_settings.showRangeCircle)
                DrawGroundRing(dl, wx, wy, wz, static_cast<float>(m_settings.pickupRange), m_settings.rangeCircleColor, scale);
            if (m_settings.showEnemyCircle)
                DrawGroundRing(dl, wx, wy, wz, static_cast<float>(m_settings.enemyRange), m_settings.enemyCircleColor, scale);
        }
    }

    if (m_settings.showOverlay) {
        const PluginSDK::ScreenSize disp = ctx()->Game.GetScreenSize();
        ImU32 col = IM_COL32(170, 170, 170, 255);
        const bool picking = m_state == State::Aiming || m_state == State::Clicked || m_state == State::Cooldown;
        if (picking) col = IM_COL32(110, 220, 130, 255);
        else if (m_state == State::NoTarget) col = IM_COL32(120, 170, 240, 255);
        else if (StateIsActive(m_state)) col = IM_COL32(230, 190, 90, 255);
        char state[96];
        if (StateIsActive(m_state) && m_wantedVisible > 0)
            std::snprintf(state, sizeof(state), "%s (%d left)", StateText(m_state), m_wantedVisible);
        else
            std::snprintf(state, sizeof(state), "%s", StateText(m_state));
        PfOverlay::StatusChip(dl, ImVec2(disp.Width, disp.Height), static_cast<PfOverlay::Anchor>(m_settings.statusAnchor), 0,
                         scale, GetName(), state, col, m_settings.overlayOpacity);
    }
}

void PickMyLootPlugin::DrawGroundRing(ImDrawList* dl, float wx, float wy, float wz, float gridRadius,
                                      const float color[4], float scale) {
    const float R = gridRadius * PfGates::kGridToWorld;
    constexpr int N = 64;
    ImVec2 pts[N];
    int n = 0;
    for (int i = 0; i < N; ++i) {
        const float a = 6.2831853f * static_cast<float>(i) / static_cast<float>(N);
        float sx = 0.f, sy = 0.f;
        if (!ctx()->Render.WorldToScreen(wx + R * std::cos(a), wy + R * std::sin(a), wz, sx, sy)) {
            n = 0;  // part of the ring is behind the camera: draw none of it
            break;
        }
        pts[n++] = ImVec2(sx, sy);
    }
    PfOverlay::GroundRing(dl, pts, n, PfOverlay::ToU32(color), std::max(1.0f, 1.5f * scale));
}
