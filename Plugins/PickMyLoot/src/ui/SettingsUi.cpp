// SettingsUi.cpp - Pick My Loot settings tab (drawn below the host's plugin
// header, which owns the Enabled checkbox and the version), built with the
// POEFixer plugin kit (PluginUi.h): cards per topic, one measured label column
// per card, wrapped tooltips, dimmed dependents.
#include "../PickMyLootPlugin.h"

#include "../../../../POEFixer/imgui/IconsFontAwesome6.h"
#include "../../../../POEFixer/plugin_sdk/PluginUi.h"
#include "../common/ImGuiText.h"

#include <imgui.h>

#include <algorithm>
#include <cctype>
#include <cstdio>

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
    if (!m_enabled) PluginUi::StatusPill("Plugin disabled", U32(pal.TextSecondary));
    else PluginUi::StatusPill(m_active ? StateText(m_state) : "Off - press the start key in game", U32(col));
    ImGui::Spacing();

    const LabelColumn lc({"Start / stop key"});
    lc.RowNoWidth("Start / stop key");
    const int before = m_settings.toggleKey;
    if (PluginUi::HotkeyCapture("##toggleKey", &m_settings.toggleKey,
                                "Press this key in game to start or stop picking up.\n"
                                "Click the button, then press the key to use.")) {
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
        ColoredWrapped(ReadableWarning(pal), "The left and right mouse buttons cannot start picking: the plugin clicks with "
                                    "the left button itself. Choose a key, or mouse button 4 or 5.");
    if (Check("Put the cursor back when done", "restore", &m_settings.restoreCursor,
              "After the last item of a pile, the cursor returns to where it was before picking started."))
        MarkDirty();
    ImGui::Spacing();
    Hint("Picks up the items you tick below (and, if you like, anything worth more than a value), nearest first: "
         "it moves the cursor onto the item's label and clicks it.");
    if (m_saveGuard.Holding()) {
        ImGui::Spacing();
        ColoredWrapped(ReadableWarning(pal),
                       m_saveGuard.Reason() == PfIo::LoadStatus::Corrupt
                           ? "Your settings file could not be read (it is not valid JSON). A copy was kept as "
                             "config/settings.json.corrupt. The defaults are in use, and nothing is saved until "
                             "you change a setting here."
                           : "Your settings file exists but could not be opened (another program may be using "
                             "it). The defaults are in use for now; the file is not overwritten and is read "
                             "again shortly.");
    }
    if (!m_skipped.empty()) {
        char buf[160];
        std::snprintf(buf, sizeof(buf),
                      "%d item(s) skipped after %d clicks without success (inventory full or label hidden?).",
                      static_cast<int>(m_skipped.size()), kMaxAttemptsPerItem);
        ImGui::Spacing();
        ColoredWrapped(ReadableWarning(pal), buf);
        if (PluginUi::SubtleButton("Retry them###retrySkipped")) {
            m_skipped.clear();
            m_attempts.clear();
        }
        Tip("Clicks the skipped items again.\n"
            "They are also retried after the next area change or when you restart picking.");
    }
    PluginUi::EndCard();
}

void PickMyLootPlugin::DrawSectionWhatToPick() {
    PluginUi::BeginCard("##what");
    Heading(ICON_FA_LIST_CHECK, "What to pick",
            "Open a category and tick the items you want picked up. The price next to each item comes from "
            "POEFixer's price list (Settings -> Prices), in the currency chosen under Value pick.\n\n"
            "Names match the English game client. The game's loot filter still decides which labels are "
            "visible: an item whose label your filter hides cannot be clicked and is skipped after a few tries.\n\n"
            "New league items join their category by themselves once you have seen one on the ground.");
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
    char hdr[128];
    std::snprintf(hdr, sizeof(hdr), "%s   %d / %d###cat%d", PickMyLoot::CategoryName(c),
                  m_tickedCount[static_cast<size_t>(index)], static_cast<int>(names.size()), index);
    ImGui::PushID(index);
    // A quiet tree row (not a full-width filled header): stays inside the
    // card's padding and does not compete with the card headings.
    ImGui::PushStyleColor(ImGuiCol_Header, pal.SurfaceVariant);
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, pal.Hover);
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, pal.Pressed);
    const bool open = ImGui::TreeNodeEx(hdr, ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding);
    ImGui::PopStyleColor(3);
    if (open) {
        std::string& filter = m_filter[static_cast<size_t>(index)];
        const char* selectLabel = "Select shown###all";
        const char* clearLabel = "Clear shown###none";
        const float spacing = ImGui::GetStyle().ItemSpacing.x;
        const float buttons = PluginUi::ButtonWidth(selectLabel) + PluginUi::ButtonWidth(clearLabel) + spacing * 2.0f;
        const float em = ImGui::GetFontSize();
        // The search field and both buttons share a row when they fit, else the
        // buttons go under a full-width field.
        const bool oneRow = ImGui::GetContentRegionAvail().x >= em * 8.0f + buttons;
        ImGui::SetNextItemWidth(oneRow ? std::min(em * 14.0f, ImGui::GetContentRegionAvail().x - buttons)
                                       : ImGui::GetContentRegionAvail().x);
        PluginUi::InputString("##filter", "Search", filter);
        Tip("Shows only the items whose name contains this text.");
        std::string needle = filter;
        for (char& ch : needle) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));

        // Indices that pass the search, so the list below can clip to the rows on screen.
        std::vector<int> shown;
        shown.reserve(names.size());
        for (int k = 0; k < static_cast<int>(names.size()); ++k)
            if (ContainsNoCase(names[static_cast<size_t>(k)], needle)) shown.push_back(k);

        if (oneRow) ImGui::SameLine();
        if (PluginUi::SubtleButton(selectLabel)) {
            for (int k : shown) m_settings.SetNameEnabled(names[static_cast<size_t>(k)], true);
            m_tickedCountAt = 0;
            MarkDirty();
        }
        Tip("Ticks every item the search shows (all of them when the search is empty).");
        ImGui::SameLine();
        if (PluginUi::SubtleButton(clearLabel)) {
            for (int k : shown) m_settings.SetNameEnabled(names[static_cast<size_t>(k)], false);
            m_tickedCountAt = 0;
            MarkDirty();
        }
        Tip("Unticks every item the search shows.");

        const float rowH = ImGui::GetFrameHeightWithSpacing();
        const int visibleRows = std::clamp(static_cast<int>(shown.size()), 1, 10);
        const float h = rowH * static_cast<float>(visibleRows) + ImGui::GetStyle().WindowPadding.y * 2.0f;
        // Capped at 32 em so on a wide pane the price stays near its name.
        const float listW = std::min(ImGui::GetContentRegionAvail().x, em * 32.0f);
        ImGui::BeginChild("##list", ImVec2(listW, h), ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding);
        if (shown.empty()) ImGui::TextDisabled("No item matches the search.");
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
                if (ImGui::Checkbox(name.c_str(), &on)) {
                    m_settings.SetNameEnabled(name, on);
                    m_tickedCountAt = 0;
                    MarkDirty();
                }
                const PickMyLoot::PriceEntry* p = m_prices.Get(ctx(), name);
                char price[48];
                if (!p) std::snprintf(price, sizeof(price), "...");
                else if (!p->known) std::snprintf(price, sizeof(price), "-");
                else std::snprintf(price, sizeof(price), "%.2f %s", p->In(ref), PickMyLoot::RefShort(ref));
                const float w = ImGui::CalcTextSize(price).x;
                ImGui::SameLine();
                ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(), rowX + rowW - w));
                if (p && p->known) ImGui::TextColored(pal.Success, "%s", price);
                else ImGui::TextDisabled("%s", price);
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
    Heading(ICON_FA_COINS, "Value pick");
    PickMyLootConfig::Settings& s = m_settings;
    if (Check("Also pick anything worth at least", "valueOn", &s.valueFilterEnabled,
              "On top of the items you ticked, picks any item whose price is at or above the value below, "
              "even if you never ticked it. Needs POEFixer's price list.")) MarkDirty();
    const LabelColumn col({"Minimum value", "Tolerance", "Currency"});
    ImGui::BeginDisabled(!s.valueFilterEnabled);
    col.Row("Minimum value");
    if (ImGui::InputDouble("##min", &s.valueThreshold, 0.0, 0.0, "%.2f")) {
        s.Clamp();
        MarkDirty();
    }
    Tip("The price an item must reach, in the currency below.\nPress Enter or click elsewhere to apply.");
    if (PluginUi::SliderIntRow(col, "Tolerance", "##tol", &s.valueTolerancePct, 0,
                               PickMyLootConfig::kValueTolerancePctMax, "%d%%",
                               "Lowers the minimum a little, so items just under it count too.\n"
                               "Example: minimum 1.00 with 10% picks anything worth 0.90 or more."))
        MarkDirty();
    ImGui::EndDisabled();
    col.Row("Currency");
    int ref = static_cast<int>(s.referenceCurrency);
    if (PluginUi::Combo("##ref", ref, {"Divine Orb", "Exalted Orb", "Chaos Orb"})) {
        s.referenceCurrency = static_cast<PickMyLoot::RefCurrency>(std::clamp(ref, 0, 2));
        MarkDirty();
    }
    Tip("The currency prices are shown and compared in, here and in the item lists.");
    if (s.valueFilterEnabled) {
        char buf[128];
        std::snprintf(buf, sizeof(buf), "Picks anything worth %.2f %s or more, plus every ticked item.",
                      s.EffectiveThreshold(), PickMyLoot::RefShort(s.referenceCurrency));
        Hint(buf);
    }
    ImGui::Spacing();
    if (m_priceStatus.loaded) {
        char buf[128];
        std::snprintf(buf, sizeof(buf), "Price list loaded: %d items.", m_priceStatus.totalItems);
        Hint(buf);
    } else {
        ColoredWrapped(ReadableWarning(pal), "POEFixer has no price list yet.");
        Tip("Prices are loaded by POEFixer itself (Settings -> Prices: league and refresh).\n"
            "Until they arrive, only ticked items are picked and the lists show no prices.");
    }
    PluginUi::EndCard();
}

void PickMyLootPlugin::DrawSectionSpeed() {
    PluginUi::BeginCard("##speed");
    Heading(ICON_FA_GAUGE_HIGH, "Speed and range");
    PickMyLootConfig::Settings& s = m_settings;
    const LabelColumn col({"Speed", "Check every", "Pause after a click", "Hover before click", "Pickup range",
                           "Click height"});
    col.Row("Speed");
    int speed = static_cast<int>(s.speedPreset);
    if (PluginUi::Combo("##preset", speed, {"Slow", "Medium", "Fast", "Instant", "Custom"})) {
        s.ApplySpeedPreset(PickMyLootConfig::SpeedPresetFromInt(speed));
        m_phase = Phase::Idle;
        m_targetId = 0;
        MarkDirty();
    }
    Tip("Slow, Medium, Fast and Instant use tested timings. Choose Custom to set the three timings below "
        "yourself.\nActual speed also depends on the frame rate and how fast the game answers a click.");
    ImGui::BeginDisabled(s.speedPreset != PickMyLootConfig::SpeedPreset::Custom);
    if (PluginUi::SliderIntRow(col, "Check every", "##interval", &s.actionIntervalMs,
                               PickMyLootConfig::kActionIntervalMinMs, PickMyLootConfig::kActionIntervalMaxMs,
                               "%d ms", "How often the ground is scanned for the next item.")) MarkDirty();
    if (PluginUi::SliderIntRow(col, "Pause after a click", "##pause", &s.pauseBetweenClicksMs,
                               PickMyLootConfig::kPauseBetweenClicksMinMs, PickMyLootConfig::kPauseBetweenClicksMaxMs,
                               "%d ms", "Waiting time after each click before the next item.")) MarkDirty();
    if (PluginUi::SliderIntRow(col, "Hover before click", "##hover", &s.hoverSettleMs,
                               PickMyLootConfig::kHoverSettleMinMs, PickMyLootConfig::kHoverSettleMaxMs, "%d ms",
                               "How long the cursor rests on the label before clicking,\n"
                               "so the game highlights it first.")) MarkDirty();
    ImGui::EndDisabled();

    if (PluginUi::SliderIntRow(col, "Pickup range", "##range", &s.pickupRange, PickMyLootConfig::kPickupRangeMin,
                               PickMyLootConfig::kPickupRangeMax, "%d cells",
                               "How far from your character an item may lie, in map cells.\n"
                               "Turn on \"Show pickup range ring\" under Display to see it while you adjust it."))
        MarkDirty();
    if (PluginUi::SliderIntRow(col, "Click height", "##labelY", &s.labelClickOffsetY,
                               PickMyLootConfig::kLabelClickOffsetMin, PickMyLootConfig::kLabelClickOffsetMax, "%d px",
                               "How far above the item's ground position the click lands, to hit its name label.\n"
                               "Raise it if pickups often need a second click; larger loot filter fonts need more."))
        MarkDirty();
    PluginUi::EndCard();
}

void PickMyLootPlugin::DrawSectionSafety() {
    PluginUi::BeginCard("##safety");
    Heading(ICON_FA_SHIELD_HALVED, "Stop picking while");
    PickMyLootConfig::Settings& s = m_settings;
    if (Check("An item panel is open", "panel", &s.gatePanelOpen,
              "Inventory, stash or vendor window.\nThe full-screen map does not stop picking.")) MarkDirty();
    if (Check("In town or a hideout", "town", &s.gateTownHideout, "No picking in towns and hideouts.")) MarkDirty();
    if (Check("An enemy is near", "enemy", &s.gateEnemyNear,
              "Waits until no hostile monster is within the range below, so picking never walks you into a "
              "fight.")) MarkDirty();
    ImGui::BeginDisabled(!s.gateEnemyNear);
    const LabelColumn col({"Enemy range"});
    if (PluginUi::SliderIntRow(col, "Enemy range", "##enemyRange", &s.enemyRange, PickMyLootConfig::kEnemyRangeMin,
                               PickMyLootConfig::kEnemyRangeMax, "%d cells",
                               "How close a hostile monster must be to stop picking, in map cells.\n"
                               "Turn on \"Show enemy range ring\" under Display to see it on the ground."))
        MarkDirty();
    ImGui::EndDisabled();
    if (Check("You hold the left mouse button", "lmb", &s.pauseWhileUserClicks,
              "Waits while you hold the left mouse button yourself, for example while moving or attacking."))
        MarkDirty();
    Hint("Picking also always stops while the POEFixer menu is open, during loading screens, while the game "
         "window is not in front, while the game is paused and while you are dead.");
    PluginUi::EndCard();
}

void PickMyLootPlugin::DrawSectionDisplay() {
    PluginUi::BeginCard("##display");
    Heading(ICON_FA_EYE, "Display");
    PickMyLootConfig::Settings& s = m_settings;
    const LabelColumn col({"Status position", "Size", "Opacity", "Pickup ring colour", "Enemy ring colour"});
    if (Check("Show status on screen", "overlay", &s.showOverlay,
              "A small status chip: picking, paused (and why), or off.")) MarkDirty();
    ImGui::BeginDisabled(!s.showOverlay);
    col.Row("Status position");
    if (PluginUi::Combo("##anchor", s.statusAnchor,
                        {"Top left", "Top centre", "Top right", "Bottom left", "Bottom centre", "Bottom right"}))
        MarkDirty();
    Tip("Where the status chip sits.\nAuto My Aim's chip stacks below it when both use the same corner.");
    int pct = static_cast<int>(s.overlayScale * 100.0f + 0.5f);
    if (PluginUi::SliderIntRow(col, "Size", "##scale", &pct, 70, 160, "%d%%",
                               "Size of the status chip and the ring lines, relative to the POEFixer font.")) {
        s.overlayScale = static_cast<float>(pct) / 100.0f;
        MarkDirty();
    }
    int op = static_cast<int>(s.overlayOpacity * 100.0f + 0.5f);
    if (PluginUi::SliderIntRow(col, "Opacity", "##opacity", &op, 30, 100, "%d%%", "Opacity of the status chip.")) {
        s.overlayOpacity = static_cast<float>(op) / 100.0f;
        MarkDirty();
    }
    ImGui::EndDisabled();

    if (Check("Show pickup range ring", "ringPick", &s.showRangeCircle,
              "Draws the pickup range on the ground around your character.")) MarkDirty();
    ImGui::BeginDisabled(!s.showRangeCircle);
    col.RowNoWidth("Pickup ring colour");
    if (PluginUi::Swatch("##ringPickColor", s.rangeCircleColor)) MarkDirty();
    Tip("Colour of the pickup range ring.");
    ImGui::EndDisabled();
    if (Check("Show enemy range ring", "ringEnemy", &s.showEnemyCircle,
              "Draws the enemy range used by \"An enemy is near\".")) MarkDirty();
    ImGui::BeginDisabled(!s.showEnemyCircle);
    col.RowNoWidth("Enemy ring colour");
    if (PluginUi::Swatch("##ringEnemyColor", s.enemyCircleColor)) MarkDirty();
    Tip("Colour of the enemy range ring.");
    ImGui::EndDisabled();

    ImGui::Spacing();
    const char* reset = ICON_FA_ROTATE_LEFT "  Reset display###resetDisplay";
    PluginUi::AlignRight(PluginUi::ButtonWidth(reset));
    if (PluginUi::SubtleButton(reset)) {
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
    Tip("Restores the display settings above to their defaults.");
    PluginUi::EndCard();
}

void PickMyLootPlugin::DrawSectionDiagnostics() {
    if (PluginUi::BeginDiagnostics("Picking state, prices, the item panel and the items in range with their status.")) {
        static const std::vector<PickMyLoot::GroundItem> kNoItems;
        const std::vector<PickMyLoot::GroundItem>& items = m_frame ? m_frame->items : kNoItems;
        char line[256];
        std::snprintf(line, sizeof(line),
                      "State: %s   |   items in range: %d   |   wanted on screen: %d   |   skipped: %d",
                      StateText(m_state), static_cast<int>(items.size()), m_wantedVisible,
                      static_cast<int>(m_skipped.size()));
        Hint(line);
        const unsigned long long nowMs = ::GetTickCount64();
        const char* panel = !m_settings.gatePanelOpen ? "not checked"
                            : m_worker.PanelOpen(nowMs) && !m_worker.PanelLastListing()
                                ? "not read yet (counts as open)"
                                : m_worker.PanelLastListing() ? "open" : "closed";
        std::snprintf(line, sizeof(line),
                      "Prices: %s, %d remembered   |   link with Auto My Aim: %s   |   item panel: %s",
                      m_priceStatus.loaded ? "loaded" : "not loaded", m_worker.PricesRemembered(),
                      m_pickBusy.Valid() ? "ready" : "unavailable", panel);
        Hint(line);
        if (m_worker.Faults() > 0) {
            std::snprintf(line, sizeof(line), "Scan errors caught: %llu (each one skips a scan pass).",
                          static_cast<unsigned long long>(m_worker.Faults()));
            Hint(line);
        }
        if (!items.empty() &&
            ImGui::BeginTable("##items", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH |
                                                ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_PadOuterX)) {
            ImGui::TableSetupColumn("Item", ImGuiTableColumnFlags_WidthStretch, 3.0f);
            ImGui::TableSetupColumn("Category", ImGuiTableColumnFlags_WidthStretch, 2.0f);
            ImGui::TableSetupColumn("Distance", ImGuiTableColumnFlags_WidthStretch, 1.0f);
            ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthStretch, 1.0f);
            ImGui::TableHeadersRow();
            const size_t n = std::min<size_t>(items.size(), 12);
            for (size_t i = 0; i < n; ++i) {
                const auto& g = items[i];
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(g.baseName.c_str());
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(PickMyLoot::CategoryName(m_catalog.CategoryOf(g.baseName, g.path)));
                ImGui::TableNextColumn();
                ImGui::Text("%.0f", g.distance);
                ImGui::TableNextColumn();
                const bool skipped = m_skipped.count(g.id) != 0;
                const bool wanted = m_settings.NameEnabled(g.baseName);
                ImGui::TextUnformatted(skipped ? "skipped" : wanted ? "ticked" : g.onScreen ? "-" : "off screen");
            }
            ImGui::EndTable();
        }
        PluginUi::EndDiagnostics();
    }
}
