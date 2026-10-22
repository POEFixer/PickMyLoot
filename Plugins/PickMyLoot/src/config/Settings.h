// Settings.h - Pick My Loot configuration: defaults, ranges, load and save.
// File: <plugin folder>/config/settings.json, the same file and keys the
// original plugin used, so existing settings carry over. Malformed values fall
// back to the default one field at a time; saving is atomic. A file that cannot
// be read or parsed is never overwritten behind the user's back (Load). Keys of removed
// options (enabled, refresh_interval_min, gate_escape_menu, debug_mode) are
// ignored when read.
#pragma once

#include "../common/FileUtil.h"
#include "../game/PriceBook.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <string>
#include <unordered_set>
#include <vector>

namespace PickMyLootConfig {

inline constexpr int kActionIntervalMinMs = 50;
inline constexpr int kActionIntervalMaxMs = 1000;
inline constexpr int kPauseBetweenClicksMinMs = 80;
inline constexpr int kPauseBetweenClicksMaxMs = 3000;
inline constexpr int kHoverSettleMinMs = 0;
inline constexpr int kHoverSettleMaxMs = 500;
inline constexpr int kPickupRangeMin = 10;
inline constexpr int kPickupRangeMax = 400;
inline constexpr int kEnemyRangeMin = 10;
inline constexpr int kEnemyRangeMax = 300;
inline constexpr int kValueTolerancePctMax = 100;
inline constexpr int kLabelClickOffsetMin = 0;
inline constexpr int kLabelClickOffsetMax = 80;
inline constexpr double kValueThresholdMax = 100000.0;

enum class SpeedPreset : int { Slow = 0, Medium, Fast, Instant, Custom };

struct SpeedTiming {
    int actionIntervalMs;
    int pauseBetweenClicksMs;
    int hoverSettleMs;
};

inline constexpr SpeedTiming kSlowTiming{300, 700, 120};
inline constexpr SpeedTiming kMediumTiming{150, 300, 60};
inline constexpr SpeedTiming kFastTiming{100, 180, 30};
inline constexpr SpeedTiming kInstantTiming{50, 80, 0};

inline SpeedPreset SpeedPresetFromInt(int value) {
    if (value < static_cast<int>(SpeedPreset::Slow) || value > static_cast<int>(SpeedPreset::Custom))
        return SpeedPreset::Medium;
    return static_cast<SpeedPreset>(value);
}

struct Settings {
    int  toggleKey = 0x74;  // F5

    int  actionIntervalMs = 150;
    int  pauseBetweenClicksMs = 300;
    int  hoverSettleMs = 60;
    SpeedPreset speedPreset = SpeedPreset::Medium;

    int  pickupRange = 100;
    int  enemyRange = 60;
    int  labelClickOffsetY = 16;

    std::unordered_set<std::string> enabledNames;

    PickMyLoot::RefCurrency referenceCurrency = PickMyLoot::RefCurrency::Divine;
    bool   valueFilterEnabled = false;
    double valueThreshold = 1.0;
    int    valueTolerancePct = 0;

    bool gateTownHideout = true;
    bool gateEnemyNear = false;
    bool gatePanelOpen = true;
    bool pauseWhileUserClicks = false;

    bool restoreCursor = true;

    bool  showOverlay = true;
    bool  showRangeCircle = false;
    bool  showEnemyCircle = false;
    int   statusAnchor = 0;        // PfOverlay::Anchor
    float overlayScale = 1.0f;     // 0.7 .. 1.6
    float overlayOpacity = 0.9f;   // 0.3 .. 1.0
    float rangeCircleColor[4] = {0.20f, 0.90f, 0.35f, 1.0f};
    float enemyCircleColor[4] = {0.90f, 0.15f, 0.15f, 1.0f};

    bool NameEnabled(const std::string& n) const { return enabledNames.find(n) != enabledNames.end(); }

    void SetNameEnabled(const std::string& n, bool on) {
        if (on) enabledNames.insert(n);
        else enabledNames.erase(n);
    }

    double EffectiveThreshold() const {
        return valueThreshold * (1.0 - static_cast<double>(valueTolerancePct) / 100.0);
    }

    void ApplySpeedPreset(SpeedPreset preset) {
        speedPreset = SpeedPresetFromInt(static_cast<int>(preset));
        SpeedTiming t{};
        switch (speedPreset) {
            case SpeedPreset::Slow:    t = kSlowTiming; break;
            case SpeedPreset::Medium:  t = kMediumTiming; break;
            case SpeedPreset::Fast:    t = kFastTiming; break;
            case SpeedPreset::Instant: t = kInstantTiming; break;
            case SpeedPreset::Custom:  return;
        }
        actionIntervalMs = t.actionIntervalMs;
        pauseBetweenClicksMs = t.pauseBetweenClicksMs;
        hoverSettleMs = t.hoverSettleMs;
    }

    SpeedPreset PresetForCurrentTiming() const {
        const SpeedTiming all[] = {kSlowTiming, kMediumTiming, kFastTiming, kInstantTiming};
        for (int i = 0; i < 4; ++i) {
            if (actionIntervalMs == all[i].actionIntervalMs && pauseBetweenClicksMs == all[i].pauseBetweenClicksMs &&
                hoverSettleMs == all[i].hoverSettleMs)
                return static_cast<SpeedPreset>(i);
        }
        return SpeedPreset::Custom;
    }

    static std::filesystem::path SettingsPath(const std::filesystem::path& pluginDir) {
        return pluginDir / "config" / "settings.json";
    }

    void Clamp() {
        toggleKey = std::clamp(toggleKey, 0, 254);
        if (toggleKey == 0x01 || toggleKey == 0x02) toggleKey = 0x74;  // own clicks would toggle it
        actionIntervalMs = std::clamp(actionIntervalMs, kActionIntervalMinMs, kActionIntervalMaxMs);
        pauseBetweenClicksMs = std::clamp(pauseBetweenClicksMs, kPauseBetweenClicksMinMs, kPauseBetweenClicksMaxMs);
        hoverSettleMs = std::clamp(hoverSettleMs, kHoverSettleMinMs, kHoverSettleMaxMs);
        pickupRange = std::clamp(pickupRange, kPickupRangeMin, kPickupRangeMax);
        enemyRange = std::clamp(enemyRange, kEnemyRangeMin, kEnemyRangeMax);
        labelClickOffsetY = std::clamp(labelClickOffsetY, kLabelClickOffsetMin, kLabelClickOffsetMax);
        if (!std::isfinite(valueThreshold) || valueThreshold < 0.0) valueThreshold = 0.0;
        if (valueThreshold > kValueThresholdMax) valueThreshold = kValueThresholdMax;
        valueTolerancePct = std::clamp(valueTolerancePct, 0, kValueTolerancePctMax);
        statusAnchor = std::clamp(statusAnchor, 0, 5);
        overlayScale = std::clamp(overlayScale, 0.7f, 1.6f);
        overlayOpacity = std::clamp(overlayOpacity, 0.3f, 1.0f);
    }

    // A file that exists but cannot be read or parsed leaves the defaults in
    // place and is reported (Unreadable / Corrupt, the latter also copied to
    // settings.json.corrupt) so the caller can hold its writes (PfIo::SaveGuard).
    PfIo::LoadStatus Load(const std::filesystem::path& pluginDir) {
        const std::filesystem::path path = SettingsPath(pluginDir);
        std::string text;
        const PfIo::ReadStatus rs = PfIo::ReadTextFileChecked(path, text);
        if (rs == PfIo::ReadStatus::Missing) return PfIo::LoadStatus::Missing;
        if (rs == PfIo::ReadStatus::Unreadable) return PfIo::LoadStatus::Unreadable;
        const nlohmann::json j = PfIo::ParseJson(text);
        if (!j.is_object()) {
            PfIo::KeepCorruptCopy(path);
            return PfIo::LoadStatus::Corrupt;
        }
        using PfIo::JsonGet;
        toggleKey = JsonGet(j, "toggle_key", toggleKey);
        actionIntervalMs = JsonGet(j, "action_interval_ms", actionIntervalMs);
        pauseBetweenClicksMs = JsonGet(j, "pause_between_clicks_ms", pauseBetweenClicksMs);
        hoverSettleMs = JsonGet(j, "hover_settle_ms", hoverSettleMs);
        Clamp();
        const auto sp = j.find("speed_preset");
        if (sp != j.end() && sp->is_number_integer()) {
            speedPreset = SpeedPresetFromInt(sp->get<int>());
            if (speedPreset != SpeedPreset::Custom) ApplySpeedPreset(speedPreset);
        } else {
            speedPreset = PresetForCurrentTiming();
        }
        pickupRange = JsonGet(j, "pickup_range", pickupRange);
        enemyRange = JsonGet(j, "enemy_range", enemyRange);
        labelClickOffsetY = JsonGet(j, "label_click_offset_y", labelClickOffsetY);

        const auto filter = j.find("filter");
        if (filter != j.end() && filter->is_object()) {
            const auto names = filter->find("enabled_names");
            if (names != filter->end() && names->is_array()) {
                enabledNames.clear();
                for (const auto& e : *names) {
                    if (!e.is_string()) continue;
                    std::string s = e.get<std::string>();
                    if (!s.empty() && s.size() <= 128) enabledNames.insert(std::move(s));
                }
            }
        }

        referenceCurrency = PickMyLoot::RefFromKey(JsonGet(j, "reference_currency", std::string("divine")));
        valueFilterEnabled = JsonGet(j, "value_filter_enabled", valueFilterEnabled);
        valueThreshold = JsonGet(j, "value_threshold", valueThreshold);
        valueTolerancePct = JsonGet(j, "value_tolerance_pct", valueTolerancePct);

        gateTownHideout = JsonGet(j, "gate_town_hideout", gateTownHideout);
        gateEnemyNear = JsonGet(j, "gate_enemy_near", gateEnemyNear);
        gatePanelOpen = JsonGet(j, "gate_panel_open", gatePanelOpen);
        pauseWhileUserClicks = JsonGet(j, "pause_while_user_clicks", pauseWhileUserClicks);
        restoreCursor = JsonGet(j, "restore_cursor", restoreCursor);
        showOverlay = JsonGet(j, "show_overlay", showOverlay);
        showRangeCircle = JsonGet(j, "show_range_circle", showRangeCircle);
        showEnemyCircle = JsonGet(j, "show_enemy_circle", showEnemyCircle);
        statusAnchor = JsonGet(j, "status_anchor", statusAnchor);
        overlayScale = JsonGet(j, "overlay_scale", overlayScale);
        overlayOpacity = JsonGet(j, "overlay_opacity", overlayOpacity);

        const auto colors = j.find("colors");
        if (colors != j.end() && colors->is_object()) {
            if (colors->contains("range_circle")) PfIo::ColorFromJson((*colors)["range_circle"], rangeCircleColor);
            if (colors->contains("enemy_circle")) PfIo::ColorFromJson((*colors)["enemy_circle"], enemyCircleColor);
        }
        Clamp();
        return PfIo::LoadStatus::Loaded;
    }

    bool Save(const std::filesystem::path& pluginDir) const {
        if (pluginDir.empty()) return false;
        nlohmann::json j;
        j["toggle_key"] = toggleKey;
        j["action_interval_ms"] = actionIntervalMs;
        j["pause_between_clicks_ms"] = pauseBetweenClicksMs;
        j["hover_settle_ms"] = hoverSettleMs;
        j["speed_preset"] = static_cast<int>(speedPreset);
        j["pickup_range"] = pickupRange;
        j["enemy_range"] = enemyRange;
        j["label_click_offset_y"] = labelClickOffsetY;
        std::vector<std::string> names(enabledNames.begin(), enabledNames.end());
        std::sort(names.begin(), names.end());
        j["filter"] = {{"enabled_names", names}};
        j["reference_currency"] = PickMyLoot::RefKey(referenceCurrency);
        j["value_filter_enabled"] = valueFilterEnabled;
        j["value_threshold"] = valueThreshold;
        j["value_tolerance_pct"] = valueTolerancePct;
        j["gate_town_hideout"] = gateTownHideout;
        j["gate_enemy_near"] = gateEnemyNear;
        j["gate_panel_open"] = gatePanelOpen;
        j["pause_while_user_clicks"] = pauseWhileUserClicks;
        j["restore_cursor"] = restoreCursor;
        j["show_overlay"] = showOverlay;
        j["show_range_circle"] = showRangeCircle;
        j["show_enemy_circle"] = showEnemyCircle;
        j["status_anchor"] = statusAnchor;
        j["overlay_scale"] = overlayScale;
        j["overlay_opacity"] = overlayOpacity;
        j["colors"] = {{"range_circle", PfIo::ColorToJson(rangeCircleColor)},
                       {"enemy_circle", PfIo::ColorToJson(enemyCircleColor)}};
        return PfIo::WriteTextFileAtomic(SettingsPath(pluginDir), PfIo::DumpJson(j));
    }
};

}  // namespace PickMyLootConfig
