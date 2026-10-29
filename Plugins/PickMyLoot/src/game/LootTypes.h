// LootTypes.h - loot categories and the ground-item record.
#pragma once

#include <cctype>
#include <cstdint>
#include <string>
#include <unordered_map>

namespace PickMyLoot {

// The order is persisted (config/cache.json keys "0".."19"): append only.
enum class Category : int {
    Currency = 0,
    Essence,
    Rune,
    SoulCore,
    Catalyst,
    Delirium,
    Incursion,
    Idol,
    Ritual,
    VaultKey,
    Fragment,
    Abyss,
    UncutGem,
    LineageGem,
    Verisium,
    Vaal,
    Expedition,
    Waystone,
    Tablet,
    Other,
    Count
};

inline constexpr int kCategoryCount = static_cast<int>(Category::Count);

// The categories' names are texts of the settings tab (ui/SettingsUi.cpp, CategoryText).

// The host price service's category id (the poe2scout category) -> ours.
// Returns Count for categories that are not stackable loot (unique items).
inline Category CategoryFromPriceCategory(const std::string& a) {
    if (a == "currency")           return Category::Currency;
    if (a == "essences")           return Category::Essence;
    if (a == "runes")              return Category::Rune;
    if (a == "ultimatum")          return Category::SoulCore;
    if (a == "expedition")         return Category::Expedition;
    if (a == "ritual")             return Category::Ritual;
    if (a == "vaultkeys")          return Category::VaultKey;
    if (a == "breach")             return Category::Catalyst;
    if (a == "abyss")              return Category::Abyss;
    if (a == "uncutgems")          return Category::UncutGem;
    if (a == "lineagesupportgems") return Category::LineageGem;
    if (a == "delirium")           return Category::Delirium;
    if (a == "incursion")          return Category::Incursion;
    if (a == "idol")               return Category::Idol;
    if (a == "verisium")           return Category::Verisium;
    if (a == "vaal")               return Category::Vaal;
    if (a == "fragments")          return Category::Fragment;
    if (a == "custom")             return Category::Other;
    return Category::Count;
}

inline std::string ToNarrow(const std::wstring& w) {
    std::string out;
    out.reserve(w.size());
    for (wchar_t c : w) out.push_back(c < 128 ? static_cast<char>(c) : '?');
    return out;
}

inline std::string ToLowerCopy(const std::string& s) {
    std::string o;
    o.reserve(s.size());
    for (char c : s) o.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    return o;
}

// Category of a base the catalog does not list. The metadata path first: it is the same in
// every game language (Metadata/Items/Maps/ holds the waystones, Metadata/Items/TowerAugment/
// the Precursor Tablets, as Tablet Helper's LooksLikeTablet reads it); the English name only
// for a path that says nothing.
inline Category ClassifyFallback(const std::string& baseName, const std::string& path) {
    const std::string b = ToLowerCopy(baseName);
    const std::string p = ToLowerCopy(path);
    if (p.rfind("metadata/items/maps/", 0) == 0)         return Category::Waystone;
    if (p.find("toweraugment") != std::string::npos)     return Category::Tablet;
    if (b.rfind("waystone", 0) == 0)                     return Category::Waystone;
    if (b.find("precursor tablet") != std::string::npos) return Category::Tablet;
    if (p.find("/currency/") != std::string::npos)       return Category::Currency;
    return Category::Other;
}

// One ground item in pickup range, as the scan worker publishes it. The
// catalog category is looked up where it is shown (the catalog belongs to the
// render thread); priceCategory is the price service's category for a name it
// prices, so the render thread can teach the catalog a new league item.
struct GroundItem {
    uint32_t       id = 0;
    std::uintptr_t address = 0;
    float          gridX = 0.f, gridY = 0.f;
    float          worldX = 0.f, worldY = 0.f, worldZ = 0.f;
    std::string    baseName;                       // as the game shows it (the game language)
    // The English identity name the lists, the ticks and the prices use: the host's
    // LookupPriceByItem name (desktop 371+), else baseName (an older host, or an item the host
    // could not name). Empty while not resolved yet: the item is not picked in that pass.
    std::string    englishName;
    std::string    path;
    float          screenX = 0.f, screenY = 0.f;   // at scan time
    float          distance = 0.f;
    bool           onScreen = false;
    bool           wanted = false;                 // ticked, or worth the value threshold
    Category       priceCategory = Category::Count;
};

// English identity name -> the name the game showed for it on the ground this session, so the
// lists (English game names, the catalog's) show an item as the game words it once it was seen.
class SeenNames {
public:
    static constexpr size_t kMax = 4096;

    void Note(const std::string& english, const std::string& shown) {
        if (english.empty() || shown.empty() || english == shown) return;
        if (m_map.size() >= kMax && m_map.find(english) == m_map.end()) return;
        m_map[english] = shown;
    }
    // The name the game showed for `english`, or `english` itself when none was seen.
    const std::string& Shown(const std::string& english) const {
        const auto it = m_map.find(english);
        return it != m_map.end() ? it->second : english;
    }
    size_t Size() const { return m_map.size(); }
    void Clear() { m_map.clear(); }

private:
    std::unordered_map<std::string, std::string> m_map;
};

}  // namespace PickMyLoot
