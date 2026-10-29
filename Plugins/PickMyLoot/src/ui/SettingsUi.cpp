// SettingsUi.cpp - Pick My Loot settings tab (drawn below the host's plugin
// header, which owns the Enabled checkbox and the version), built with the
// POEFixer plugin kit (PluginUi.h): cards per topic, one measured label column
// per card, wrapped tooltips, dimmed dependents.
//
// Every text a user reads is a key of lang/en.json (I18n::Tr / TrF / TrId); the
// item names in the lists are the game's own and stay as the catalog spells them.
#include "../PickMyLootPlugin.h"

#include "../../../../POEFixer/imgui/IconsFontAwesome6.h"
#include "../../../../POEFixer/plugin_sdk/PluginUi.h"
#include "../common/ImGuiText.h"
#include "../common/TextUtil.h"

#include <imgui.h>

#include <sal.h>

#include <algorithm>
#include <cctype>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>

namespace I18n = PluginSDK::I18n;
using PluginUi::Check;
using PluginUi::Heading;
using PluginUi::Hint;
using PluginUi::LabelColumn;
using PluginUi::Tip;
using PickMyLoot::Category;

namespace {
ImU32 U32(const ImVec4& c) { return ImGui::ColorConvertFloat4ToU32(c); }

// The kit's warning amber is made for dark themes; on a light window it is
// too faint for text, so it is darkened there.
ImVec4 ReadableWarning(const PluginUi::Palette& pal) {
    const ImVec4 bg = ImGui::GetStyleColorVec4(ImGuiCol_WindowBg);
    const float luma = 0.2126f * bg.x + 0.7152f * bg.y + 0.0722f * bg.z;
    return luma > 0.5f ? ImVec4(0.62f, 0.38f, 0.0f, 1.0f) : pal.Warning;
}

// Wrapped text in one colour (a warning line).
void ColoredWrapped(const ImVec4& color, const char* text) {
    ImGui::PushStyleColor(ImGuiCol_Text, color);
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextUnformatted(text);
    ImGui::PopTextWrapPos();
    ImGui::PopStyleColor();
}

bool ContainsNoCase(const std::string& hay, const std::string& lowerNeedle) {
    if (lowerNeedle.empty()) return true;
    if (lowerNeedle.size() > hay.size()) return false;
    for (size_t i = 0; i + lowerNeedle.size() <= hay.size(); ++i) {
        size_t k = 0;
        while (k < lowerNeedle.size() &&
               std::tolower(static_cast<unsigned char>(hay[i + k])) == static_cast<unsigned char>(lowerNeedle[k]))
            ++k;
        if (k == lowerNeedle.size()) return true;
    }
    return false;
}

// printf for a TrF argument: Num("%.2f", value). The format is code, never a translation.
std::string Num(_In_z_ _Printf_format_string_ const char* format, ...) {
    char buf[64];
    va_list args;
    va_start(args, format);
    std::vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);
    return buf;
}

// A slider's printf format from a text with a {value} ("{value} ms"): common/TextUtil.h.
using PickMyLoot::SliderFormat;

// The category's name in the program language. The order of the cases is the
// enum's; the game's item names inside a category stay English (game names).
const char* CategoryText(Category c) {
    switch (c) {
        case Category::Currency:   return I18n::Tr("category_currency");
        case Category::Essence:    return I18n::Tr("category_essences");
        case Category::Rune:       return I18n::Tr("category_runes");
        case Category::SoulCore:   return I18n::Tr("category_soul_cores");
        case Category::Catalyst:   return I18n::Tr("category_catalysts");
        case Category::Delirium:   return I18n::Tr("category_delirium");
        case Category::Incursion:  return I18n::Tr("category_incursion");
        case Category::Idol:       return I18n::Tr("category_idols");
        case Category::Ritual:     return I18n::Tr("category_ritual");
        case Category::VaultKey:   return I18n::Tr("category_vault_keys");
        case Category::Fragment:   return I18n::Tr("category_fragments");
        case Category::Abyss:      return I18n::Tr("category_abyss");
        case Category::UncutGem:   return I18n::Tr("category_uncut_gems");
        case Category::LineageGem: return I18n::Tr("category_lineage_gems");
        case Category::Verisium:   return I18n::Tr("category_verisium");
        case Category::Vaal:       return I18n::Tr("category_vaal");
        case Category::Expedition: return I18n::Tr("category_expedition");
        case Category::Waystone:   return I18n::Tr("category_waystones");
        case Category::Tablet:     return I18n::Tr("category_tablets");
        case Category::Other:      return I18n::Tr("category_other");
        default:                   return "?";
    }
}

// The short unit written after a price ("div", "ex", "c").
const char* RefShortText(PickMyLoot::RefCurrency r) {
    switch (r) {
        case PickMyLoot::RefCurrency::Divine: return I18n::Tr("currency_short_divine");
        case PickMyLoot::RefCurrency::Chaos:  return I18n::Tr("currency_short_chaos");
        default:                              return I18n::Tr("currency_short_exalted");
    }
}

// "1.25 div": a price in the reference currency.
std::string PriceText(double amount, PickMyLoot::RefCurrency r) {
    return I18n::TrF("price_amount", { { "amount", Num("%.2f", amount) }, { "currency", RefShortText(r) } });
}
}  // namespace

void PickMyLootPlugin::DrawSettings() {
    if (!ctx()->ImGuiContext) return;
    ImGui::SetCurrentContext(static_cast<ImGuiContext*>(ctx()->ImGuiContext));
    PfImGui::EnsureTextTables();
    EnsureLoaded();

    const unsigned long long now = ::GetTickCount64();
    m_prices.BeginFrame(ctx(), 24, now);
    if (m_priceStatusAt == 0 || now - m_priceStatusAt > 1000) {
        m_priceStatusAt = now;
        m_priceStatus = ctx()->Prices.GetStatus();
    }
    if (m_tickedCountAt == 0 || now - m_tickedCountAt > 300) {
        m_tickedCountAt = now;
        for (int i = 0; i < PickMyLoot::kCategoryCount; ++i) {
            int n = 0;
            for (const auto& name : m_catalog.Names(static_cast<Category>(i)))
                if (m_settings.NameEnabled(name)) ++n;
            m_tickedCount[static_cast<size_t>(i)] = n;
        }
    }

    ImGui::PushID("PickMyLootSettings");
    DrawHeader();
    ImGui::Spacing();
    DrawSectionWhatToPick();
    ImGui::Spacing();
    if (PluginUi::WideEnoughForTwoColumns() &&
        ImGui::BeginTable("##cols", 2, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoPadOuterX)) {
        ImGui::TableNextColumn();
        DrawSectionValue();
        ImGui::Spacing();
        DrawSectionSpeed();
        ImGui::TableNextColumn();
        DrawSectionSafety();
        ImGui::Spacing();
        DrawSectionDisplay();
        ImGui::EndTable();
    } else {
        DrawSectionValue();
        ImGui::Spacing();
        DrawSectionSpeed();
        ImGui::Spacing();
        DrawSectionSafety();
        ImGui::Spacing();
        DrawSectionDisplay();
    }
    ImGui::Spacing();
    DrawSectionDiagnostics();
    ImGui::Spacing();
    PluginUi::Hint("Originally created by Omer Faruk ARPA. Maintained by POEFixer. GPL-3.0.");
    ImGui::Spacing();
    ImGui::PopID();

    if (m_dirty && !ImGui::IsAnyItemActive() && now - m_dirtyAt > 600) SaveSettings();
}

void PickMyLootPlugin::DrawHeader() {
    const PluginUi::Palette pal = PluginUi::Colors();
    PluginUi::BeginCard("##header");
    const bool picking = m_state == State::Aiming || m_state == State::Clicked || m_state == State::Cooldown;
    ImVec4 col = pal.TextSecondary;
    if (picking) col = pal.Success;
    else if (m_state == State::NoTarget) col = pal.Accent;
    else if (StateIsActive(m_state)) col = ReadableWarning(pal);
    if (!m_enabled) PluginUi::StatusPill(I18n::Tr("header_plugin_disabled"), U32(pal.TextSecondary));
    else PluginUi::StatusPill(m_active ? StateText(m_state) : I18n::Tr("header_off_hint"), U32(col));
    ImGui::Spacing();

    const char* keyLabel = I18n::Tr("header_toggle_key");
    const LabelColumn lc({keyLabel});
    lc.RowNoWidth(keyLabel);
    const int before = m_settings.toggleKey;
    if (PluginUi::HotkeyCapture("##toggleKey", &m_settings.toggleKey, I18n::Tr("header_toggle_key_tip"))) {
        // The plugin clicks with the left button itself, and the right button
        // would start and stop picking while you play: both are refused.
        if (m_settings.toggleKey == VK_LBUTTON || m_settings.toggleKey == VK_RBUTTON) {
            m_settings.toggleKey = before;
            m_keyRefusedAt = ::GetTickCount64();
        } else {
            m_keyRefusedAt = 0;
            OnKeyBound(m_settings.toggleKey);
        }
    }
    if (m_keyRefusedAt != 0 && ::GetTickCount64() - m_keyRefusedAt < 6000)
        ColoredWrapped(ReadableWarning(pal), I18n::Tr("header_mouse_refused"));
    if (Check(I18n::Tr("header_restore_cursor"), "restore", &m_settings.restoreCursor,
              I18n::Tr("header_restore_cursor_tip")))
        MarkDirty();
    ImGui::Spacing();
    Hint(I18n::Tr("header_intro"));
    if (m_saveGuard.Holding()) {
        ImGui::Spacing();
        ColoredWrapped(ReadableWarning(pal), m_saveGuard.Reason() == PfIo::LoadStatus::Corrupt
                                                 ? I18n::Tr("header_settings_corrupt")
                                                 : I18n::Tr("header_settings_unreadable"));
    }
    if (!m_skipped.empty()) {
        ImGui::Spacing();
        const std::string skipped = I18n::TrF("header_skipped", { { "count", std::to_string(m_skipped.size()) },
                                                                  { "clicks", std::to_string(kMaxAttemptsPerItem) } });
        ColoredWrapped(ReadableWarning(pal), skipped.c_str());
        if (PluginUi::SubtleButton(I18n::TrId("header_retry"))) {
            m_skipped.clear();
            m_attempts.clear();
        }
        Tip(I18n::Tr("header_retry_tip"));
    }
    PluginUi::EndCard();
}

void PickMyLootPlugin::DrawSectionWhatToPick() {
    PluginUi::BeginCard("##what");
    const std::string help = I18n::TrF("pick_heading_help", { { "section", I18n::Tr("value_heading") } });
    Heading(ICON_FA_LIST_CHECK, I18n::Tr("pick_heading"), help.c_str());
    // Categories in two columns on a wide pane (filled top to bottom), so it is
    // not one long list; an opened category grows only its own column. Each
    // column keeps room for long item names next to their prices.
    int cats[PickMyLoot::kCategoryCount];
    int n = 0;
    for (int i = 0; i < PickMyLoot::kCategoryCount; ++i) {
        const Category c = static_cast<Category>(i);
        if (!(m_catalog.Names(c).empty() && c == Category::Other)) cats[n++] = i;
    }
    const int cols = ImGui::GetContentRegionAvail().x >= ImGui::GetFontSize() * 48.0f ? 2 : 1;
    if (cols > 1 && ImGui::BeginTable("##catCols", cols, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoPadOuterX)) {
        const int per = (n + cols - 1) / cols;
        for (int col = 0; col < cols; ++col) {
            ImGui::TableNextColumn();
            for (int k = col * per; k < std::min(n, (col + 1) * per); ++k) DrawCategory(cats[k]);
        }
        ImGui::EndTable();
    } else {
        for (int k = 0; k < n; ++k) DrawCategory(cats[k]);
    }
    PluginUi::EndCard();
}

void PickMyLootPlugin::DrawCategory(int index) {
    const PluginUi::Palette pal = PluginUi::Colors();
    const Category c = static_cast<Category>(index);
    const auto& names = m_catalog.Names(c);
    if (names.empty() && c == Category::Other) return;
    // The name, then "ticked / all"; the id stays the category's in every language.
    const std::string hdr = std::string(CategoryText(c)) + "   " +
                            std::to_string(m_tickedCount[static_cast<size_t>(index)]) + " / " +
                            std::to_string(names.size()) + "###cat" + std::to_string(index);
    ImGui::PushID(index);
    // A quiet tree row (not a full-width filled header): stays inside the
    // card's padding and does not compete with the card headings.
    ImGui::PushStyleColor(ImGuiCol_Header, pal.SurfaceVariant);
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, pal.Hover);
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, pal.Pressed);
    const bool open = ImGui::TreeNodeEx(hdr.c_str(), ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding);
    ImGui::PopStyleColor(3);
    if (open) {
        std::string& filter = m_filter[static_cast<size_t>(index)];
        const char* selectLabel = I18n::TrId("pick_select_shown");
        const char* clearLabel = I18n::TrId("pick_clear_shown");
        const float spacing = ImGui::GetStyle().ItemSpacing.x;
        const float buttons = PluginUi::ButtonWidth(selectLabel) + PluginUi::ButtonWidth(clearLabel) + spacing * 2.0f;
        const float em = ImGui::GetFontSize();
        // The search field and both buttons share a row when they fit, else the
        // buttons go under a full-width field.
        const bool oneRow = ImGui::GetContentRegionAvail().x >= em * 8.0f + buttons;
        ImGui::SetNextItemWidth(oneRow ? std::min(em * 14.0f, ImGui::GetContentRegionAvail().x - buttons)
                                       : ImGui::GetContentRegionAvail().x);
        PluginUi::InputString("##filter", I18n::Tr("pick_search_hint"), filter);
        Tip(I18n::Tr("pick_search_tip"));
        std::string needle = filter;
        for (char& ch : needle) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));

        // Indices that pass the search, so the list below can clip to the rows on screen.
        std::vector<int> shown;
        shown.reserve(names.size());
        // A name matches in English (the catalog's) or as the game showed it on the ground.
        for (int k = 0; k < static_cast<int>(names.size()); ++k) {
            const std::string& english = names[static_cast<size_t>(k)];
            if (ContainsNoCase(english, needle) || ContainsNoCase(m_seenNames.Shown(english), needle))
                shown.push_back(k);
        }

        if (oneRow) ImGui::SameLine();
        if (PluginUi::SubtleButton(selectLabel)) {
            for (int k : shown) m_settings.SetNameEnabled(names[static_cast<size_t>(k)], true);
            m_tickedCountAt = 0;
            MarkDirty();
        }
        Tip(I18n::Tr("pick_select_shown_tip"));
        ImGui::SameLine();
        if (PluginUi::SubtleButton(clearLabel)) {
            for (int k : shown) m_settings.SetNameEnabled(names[static_cast<size_t>(k)], false);
            m_tickedCountAt = 0;
            MarkDirty();
        }
        Tip(I18n::Tr("pick_clear_shown_tip"));

        const float rowH = ImGui::GetFrameHeightWithSpacing();
        const int visibleRows = std::clamp(static_cast<int>(shown.size()), 1, 10);
        const float h = rowH * static_cast<float>(visibleRows) + ImGui::GetStyle().WindowPadding.y * 2.0f;
        // Capped at 32 em so on a wide pane the price stays near its name.
        const float listW = std::min(ImGui::GetContentRegionAvail().x, em * 32.0f);
        ImGui::BeginChild("##list", ImVec2(listW, h), ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding);
        if (shown.empty()) ImGui::TextDisabled("%s", I18n::Tr("pick_no_match"));
        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(shown.size()), rowH);
        const PickMyLoot::RefCurrency ref = m_settings.referenceCurrency;
        while (clipper.Step()) {
            for (int r = clipper.DisplayStart; r < clipper.DisplayEnd; ++r) {
                const std::string& name = names[static_cast<size_t>(shown[static_cast<size_t>(r)])];
                ImGui::PushID(shown[static_cast<size_t>(r)]);
                const float rowX = ImGui::GetCursorPosX();
                const float rowW = ImGui::GetContentRegionAvail().x;
                bool on = m_settings.NameEnabled(name);
                // Shown as the game named it on the ground this session (another game
                // language), else in English; the tick and the id stay the English name.
                const std::string& seen = m_seenNames.Shown(name);
                const std::string label = &seen == &name ? name : seen + "###" + name;
                if (ImGui::Checkbox(label.c_str(), &on)) {
                    m_settings.SetNameEnabled(name, on);
                    m_tickedCountAt = 0;
                    MarkDirty();
                }
                const PickMyLoot::PriceEntry* p = m_prices.Get(ctx(), name);
                // "..." while the price is still being looked up, "-" for none.
                const std::string price = !p ? std::string("...") : !p->known ? std::string("-") : PriceText(p->In(ref), ref);
                const float w = ImGui::CalcTextSize(price.c_str()).x;
                ImGui::SameLine();
                ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(), rowX + rowW - w));
                if (p && p->known) ImGui::TextColored(pal.Success, "%s", price.c_str());
                else ImGui::TextDisabled("%s", price.c_str());
                ImGui::PopID();
            }
        }
        ImGui::EndChild();
        ImGui::Spacing();
        ImGui::TreePop();
    }
    ImGui::PopID();
}

void PickMyLootPlugin::DrawSectionValue() {
    const PluginUi::Palette pal = PluginUi::Colors();
    PluginUi::BeginCard("##value");
    Heading(ICON_FA_COINS, I18n::Tr("value_heading"));
    PickMyLootConfig::Settings& s = m_settings;
    if (Check(I18n::Tr("value_enable"), "valueOn", &s.valueFilterEnabled, I18n::Tr("value_enable_tip"))) MarkDirty();
    const char* minLabel = I18n::Tr("value_minimum");
    const char* tolLabel = I18n::Tr("value_tolerance");
    const char* curLabel = I18n::Tr("value_currency");
    const LabelColumn col({minLabel, tolLabel, curLabel});
    ImGui::BeginDisabled(!s.valueFilterEnabled);
    col.Row(minLabel);
    if (ImGui::InputDouble("##min", &s.valueThreshold, 0.0, 0.0, "%.2f")) {
        s.Clamp();
        MarkDirty();
    }
    Tip(I18n::Tr("value_minimum_tip"));
    const std::string percent = SliderFormat(I18n::Tr("unit_percent"));
    // The example's numbers are code: 1.00 with 10% tolerance is 0.90.
    const std::string tolTip =
        I18n::TrF("value_tolerance_tip", { { "minimum", "1.00" },
                                           { "tolerance", I18n::TrF("unit_percent", { { "value", "10" } }) },
                                           { "result", "0.90" } });
    if (PluginUi::SliderIntRow(col, tolLabel, "##tol", &s.valueTolerancePct, 0, PickMyLootConfig::kValueTolerancePctMax,
                               percent.c_str(), tolTip.c_str()))
        MarkDirty();
    ImGui::EndDisabled();
    col.Row(curLabel);
    int ref = static_cast<int>(s.referenceCurrency);
    if (PluginUi::Combo("##ref", ref,
                        {I18n::Tr("currency_divine"), I18n::Tr("currency_exalted"), I18n::Tr("currency_chaos")})) {
        s.referenceCurrency = static_cast<PickMyLoot::RefCurrency>(std::clamp(ref, 0, 2));
        MarkDirty();
    }
    Tip(I18n::Tr("value_currency_tip"));
    if (s.valueFilterEnabled)
        Hint(I18n::TrF("value_summary", { { "price", PriceText(s.EffectiveThreshold(), s.referenceCurrency) } }).c_str());
    ImGui::Spacing();
    if (m_priceStatus.loaded) {
        Hint(I18n::TrF("value_prices_loaded", { { "count", std::to_string(m_priceStatus.totalItems) } }).c_str());
    } else {
        ColoredWrapped(ReadableWarning(pal), I18n::Tr("value_no_prices"));
        Tip(I18n::Tr("value_no_prices_tip"));
    }
    PluginUi::EndCard();
}

void PickMyLootPlugin::DrawSectionSpeed() {
    PluginUi::BeginCard("##speed");
    Heading(ICON_FA_GAUGE_HIGH, I18n::Tr("speed_heading"));
    PickMyLootConfig::Settings& s = m_settings;
    const char* speedLabel = I18n::Tr("speed_speed");
    const char* everyLabel = I18n::Tr("speed_check_every");
    const char* pauseLabel = I18n::Tr("speed_pause");
    const char* hoverLabel = I18n::Tr("speed_hover");
    const char* rangeLabel = I18n::Tr("speed_range");
    const char* heightLabel = I18n::Tr("speed_click_height");
    const LabelColumn col({speedLabel, everyLabel, pauseLabel, hoverLabel, rangeLabel, heightLabel});
    col.Row(speedLabel);
    int speed = static_cast<int>(s.speedPreset);
    if (PluginUi::Combo("##preset", speed,
                        {I18n::Tr("speed_preset_slow"), I18n::Tr("speed_preset_medium"), I18n::Tr("speed_preset_fast"),
                         I18n::Tr("speed_preset_instant"), I18n::Tr("speed_preset_custom")})) {
        s.ApplySpeedPreset(PickMyLootConfig::SpeedPresetFromInt(speed));
        m_phase = Phase::Idle;
        m_targetId = 0;
        MarkDirty();
    }
    Tip(I18n::Tr("speed_preset_tip"));
    const std::string ms = SliderFormat(I18n::Tr("unit_ms"));
    ImGui::BeginDisabled(s.speedPreset != PickMyLootConfig::SpeedPreset::Custom);
    if (PluginUi::SliderIntRow(col, everyLabel, "##interval", &s.actionIntervalMs,
                               PickMyLootConfig::kActionIntervalMinMs, PickMyLootConfig::kActionIntervalMaxMs,
                               ms.c_str(), I18n::Tr("speed_check_every_tip"))) MarkDirty();
    if (PluginUi::SliderIntRow(col, pauseLabel, "##pause", &s.pauseBetweenClicksMs,
                               PickMyLootConfig::kPauseBetweenClicksMinMs, PickMyLootConfig::kPauseBetweenClicksMaxMs,
                               ms.c_str(), I18n::Tr("speed_pause_tip"))) MarkDirty();
    if (PluginUi::SliderIntRow(col, hoverLabel, "##hover", &s.hoverSettleMs,
                               PickMyLootConfig::kHoverSettleMinMs, PickMyLootConfig::kHoverSettleMaxMs, ms.c_str(),
                               I18n::Tr("speed_hover_tip"))) MarkDirty();
    ImGui::EndDisabled();

    const std::string cells = SliderFormat(I18n::Tr("unit_cells"));
    const std::string rangeTip = I18n::TrF("speed_range_tip", { { "option", I18n::Tr("display_ring_pick") },
                                                                { "section", I18n::Tr("display_heading") } });
    if (PluginUi::SliderIntRow(col, rangeLabel, "##range", &s.pickupRange, PickMyLootConfig::kPickupRangeMin,
                               PickMyLootConfig::kPickupRangeMax, cells.c_str(), rangeTip.c_str()))
        MarkDirty();
    const std::string px = SliderFormat(I18n::Tr("unit_px"));
    if (PluginUi::SliderIntRow(col, heightLabel, "##labelY", &s.labelClickOffsetY,
                               PickMyLootConfig::kLabelClickOffsetMin, PickMyLootConfig::kLabelClickOffsetMax, px.c_str(),
                               I18n::Tr("speed_click_height_tip")))
        MarkDirty();
    PluginUi::EndCard();
}

void PickMyLootPlugin::DrawSectionSafety() {
    PluginUi::BeginCard("##safety");
    Heading(ICON_FA_SHIELD_HALVED, I18n::Tr("safety_heading"));
    PickMyLootConfig::Settings& s = m_settings;
    if (Check(I18n::Tr("safety_panel"), "panel", &s.gatePanelOpen, I18n::Tr("safety_panel_tip"))) MarkDirty();
    if (Check(I18n::Tr("safety_town"), "town", &s.gateTownHideout, I18n::Tr("safety_town_tip"))) MarkDirty();
    if (Check(I18n::Tr("safety_enemy"), "enemy", &s.gateEnemyNear, I18n::Tr("safety_enemy_tip"))) MarkDirty();
    ImGui::BeginDisabled(!s.gateEnemyNear);
    const char* rangeLabel = I18n::Tr("safety_enemy_range");
    const LabelColumn col({rangeLabel});
    const std::string cells = SliderFormat(I18n::Tr("unit_cells"));
    const std::string rangeTip = I18n::TrF("safety_enemy_range_tip", { { "option", I18n::Tr("display_ring_enemy") },
                                                                       { "section", I18n::Tr("display_heading") } });
    if (PluginUi::SliderIntRow(col, rangeLabel, "##enemyRange", &s.enemyRange, PickMyLootConfig::kEnemyRangeMin,
                               PickMyLootConfig::kEnemyRangeMax, cells.c_str(), rangeTip.c_str()))
        MarkDirty();
    ImGui::EndDisabled();
    if (Check(I18n::Tr("safety_lmb"), "lmb", &s.pauseWhileUserClicks, I18n::Tr("safety_lmb_tip"))) MarkDirty();
    Hint(I18n::Tr("safety_always"));
    PluginUi::EndCard();
}

void PickMyLootPlugin::DrawSectionDisplay() {
    PluginUi::BeginCard("##display");
    Heading(ICON_FA_EYE, I18n::Tr("display_heading"));
    PickMyLootConfig::Settings& s = m_settings;
    const char* anchorLabel = I18n::Tr("display_status_position");
    const char* sizeLabel = I18n::Tr("display_size");
    const char* opacityLabel = I18n::Tr("display_opacity");
    const char* pickColourLabel = I18n::Tr("display_ring_pick_colour");
    const char* enemyColourLabel = I18n::Tr("display_ring_enemy_colour");
    const LabelColumn col({anchorLabel, sizeLabel, opacityLabel, pickColourLabel, enemyColourLabel});
    if (Check(I18n::Tr("display_show_status"), "overlay", &s.showOverlay, I18n::Tr("display_show_status_tip")))
        MarkDirty();
    ImGui::BeginDisabled(!s.showOverlay);
    col.Row(anchorLabel);
    if (PluginUi::Combo("##anchor", s.statusAnchor,
                        {I18n::Tr("display_anchor_top_left"), I18n::Tr("display_anchor_top_centre"),
                         I18n::Tr("display_anchor_top_right"), I18n::Tr("display_anchor_bottom_left"),
                         I18n::Tr("display_anchor_bottom_centre"), I18n::Tr("display_anchor_bottom_right")}))
        MarkDirty();
    Tip(I18n::Tr("display_status_position_tip"));
    const std::string percent = SliderFormat(I18n::Tr("unit_percent"));
    int pct = static_cast<int>(s.overlayScale * 100.0f + 0.5f);
    if (PluginUi::SliderIntRow(col, sizeLabel, "##scale", &pct, 70, 160, percent.c_str(), I18n::Tr("display_size_tip"))) {
        s.overlayScale = static_cast<float>(pct) / 100.0f;
        MarkDirty();
    }
    int op = static_cast<int>(s.overlayOpacity * 100.0f + 0.5f);
    if (PluginUi::SliderIntRow(col, opacityLabel, "##opacity", &op, 30, 100, percent.c_str(),
                               I18n::Tr("display_opacity_tip"))) {
        s.overlayOpacity = static_cast<float>(op) / 100.0f;
        MarkDirty();
    }
    ImGui::EndDisabled();

    if (Check(I18n::Tr("display_ring_pick"), "ringPick", &s.showRangeCircle, I18n::Tr("display_ring_pick_tip")))
        MarkDirty();
    ImGui::BeginDisabled(!s.showRangeCircle);
    col.RowNoWidth(pickColourLabel);
    if (PluginUi::Swatch("##ringPickColor", s.rangeCircleColor)) MarkDirty();
    Tip(I18n::Tr("display_ring_pick_colour_tip"));
    ImGui::EndDisabled();
    const std::string enemyRingTip = I18n::TrF("display_ring_enemy_tip", { { "option", I18n::Tr("safety_enemy") } });
    if (Check(I18n::Tr("display_ring_enemy"), "ringEnemy", &s.showEnemyCircle, enemyRingTip.c_str())) MarkDirty();
    ImGui::BeginDisabled(!s.showEnemyCircle);
    col.RowNoWidth(enemyColourLabel);
    if (PluginUi::Swatch("##ringEnemyColor", s.enemyCircleColor)) MarkDirty();
    Tip(I18n::Tr("display_ring_enemy_colour_tip"));
    ImGui::EndDisabled();

    ImGui::Spacing();
    const std::string reset = std::string(ICON_FA_ROTATE_LEFT "  ") + I18n::Tr("display_reset") + "###resetDisplay";
    PluginUi::AlignRight(PluginUi::ButtonWidth(reset.c_str()));
    if (PluginUi::SubtleButton(reset.c_str())) {
        const PickMyLootConfig::Settings d;
        s.showOverlay = d.showOverlay;
        s.statusAnchor = d.statusAnchor;
        s.overlayScale = d.overlayScale;
        s.overlayOpacity = d.overlayOpacity;
        s.showRangeCircle = d.showRangeCircle;
        s.showEnemyCircle = d.showEnemyCircle;
        std::copy(std::begin(d.rangeCircleColor), std::end(d.rangeCircleColor), s.rangeCircleColor);
        std::copy(std::begin(d.enemyCircleColor), std::end(d.enemyCircleColor), s.enemyCircleColor);
        MarkDirty();
    }
    Tip(I18n::Tr("display_reset_tip"));
    PluginUi::EndCard();
}

void PickMyLootPlugin::DrawSectionDiagnostics() {
    if (PluginUi::BeginDiagnostics(I18n::Tr("diag_tip"))) {
        static const std::vector<PickMyLoot::GroundItem> kNoItems;
        const std::vector<PickMyLoot::GroundItem>& items = m_frame ? m_frame->items : kNoItems;
        // Each value is its own text; the separators between them are code.
        static constexpr char kSep[] = "   |   ";
        std::string line = I18n::TrF("diag_state", { { "state", StateText(m_state) } }) + kSep +
                           I18n::TrF("diag_in_range", { { "count", std::to_string(items.size()) } }) + kSep +
                           I18n::TrF("diag_wanted", { { "count", std::to_string(m_wantedVisible) } }) + kSep +
                           I18n::TrF("diag_skipped", { { "count", std::to_string(m_skipped.size()) } });
        Hint(line.c_str());
        const unsigned long long nowMs = ::GetTickCount64();
        const char* panel = !m_settings.gatePanelOpen ? I18n::Tr("diag_panel_not_checked")
                            : m_worker.PanelOpen(nowMs) && !m_worker.PanelLastListing()
                                ? I18n::Tr("diag_panel_not_read")
                                : m_worker.PanelLastListing() ? I18n::Tr("diag_panel_open") : I18n::Tr("diag_panel_closed");
        line = I18n::TrF("diag_prices", { { "state", m_priceStatus.loaded ? I18n::Tr("diag_prices_loaded")
                                                                         : I18n::Tr("diag_prices_not_loaded") },
                                          { "count", std::to_string(m_worker.PricesRemembered()) } }) +
               kSep +
               I18n::TrF("diag_link", { { "state", m_pickBusy.Valid() ? I18n::Tr("diag_link_ready")
                                                                      : I18n::Tr("diag_link_unavailable") } }) +
               kSep + I18n::TrF("diag_panel", { { "state", panel } });
        Hint(line.c_str());
        if (m_worker.Faults() > 0)
            Hint(I18n::TrF("diag_faults", { { "count", std::to_string(m_worker.Faults()) } }).c_str());
        if (!items.empty() &&
            ImGui::BeginTable("##items", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH |
                                                ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_PadOuterX)) {
            ImGui::TableSetupColumn(I18n::TrId("diag_col_item"), ImGuiTableColumnFlags_WidthStretch, 3.0f);
            ImGui::TableSetupColumn(I18n::TrId("diag_col_category"), ImGuiTableColumnFlags_WidthStretch, 2.0f);
            ImGui::TableSetupColumn(I18n::TrId("diag_col_distance"), ImGuiTableColumnFlags_WidthStretch, 1.0f);
            ImGui::TableSetupColumn(I18n::TrId("diag_col_status"), ImGuiTableColumnFlags_WidthStretch, 1.0f);
            ImGui::TableHeadersRow();
            const size_t n = std::min<size_t>(items.size(), 12);
            for (size_t i = 0; i < n; ++i) {
                const auto& g = items[i];
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                // The name as the game shows it; category and tick by the English identity name.
                const std::string& identity = g.englishName.empty() ? g.baseName : g.englishName;
                ImGui::TextUnformatted(g.baseName.c_str());
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(CategoryText(m_catalog.CategoryOf(identity, g.path)));
                ImGui::TableNextColumn();
                ImGui::Text("%.0f", g.distance);
                ImGui::TableNextColumn();
                const bool skipped = m_skipped.count(g.id) != 0;
                const bool wanted = m_settings.NameEnabled(identity);
                ImGui::TextUnformatted(skipped  ? I18n::Tr("diag_status_skipped")
                                       : wanted ? I18n::Tr("diag_status_ticked")
                                       : g.onScreen ? "-"
                                                    : I18n::Tr("diag_status_off_screen"));
            }
            ImGui::EndTable();
        }
        PluginUi::EndDiagnostics();
    }
}
