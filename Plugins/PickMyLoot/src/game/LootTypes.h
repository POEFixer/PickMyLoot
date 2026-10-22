// LootTypes.h - loot categories and the ground-item record.
#pragma once

#include <cctype>
#include <cstdint>
#include <string>

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

inline const char* CategoryName(Category c) {
    switch (c) {
        case Category::Currency:   return "Currency";
        case Category::Essence:    return "Essences";
        case Category::Rune:       return "Runes";
        case Category::SoulCore:   return "Soul Cores";
        case Category::Catalyst:   return "Catalysts";
        case Category::Delirium:   return "Delirium (Liquid Emotions)";
        case Category::Incursion:  return "Incursion";
        case Category::Idol:       return "Idols";
        case Category::Ritual:     return "Ritual (Omens)";
        case Category::VaultKey:   return "Vault Keys";
        case Category::Fragment:   return "Fragments";
        case Category::Abyss:      return "Abyss";
        case Category::UncutGem:   return "Uncut Gems";
        case Category::LineageGem: return "Lineage Gems";
        case Category::Verisium:   return "Verisium and Alloys";
        case Category::Vaal:       return "Vaal";
        case Category::Expedition: return "Expedition";
        case Category::Waystone:   return "Waystones";
        case Category::Tablet:     return "Tablets";
        case Category::Other:      return "Other";
        default:                   return "?";
    }
}

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

// Category of a base the catalog does not list, from its name and metadata path.
inline Category ClassifyFallback(const std::string& baseName, const std::string& path) {
    const std::string b = ToLowerCopy(baseName);
    const std::string p = ToLowerCopy(path);
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
    std::string    baseName;
    std::string    path;
    float          screenX = 0.f, screenY = 0.f;   // at scan time
    float          distance = 0.f;
    bool           onScreen = false;
    bool           wanted = false;                 // ticked, or worth the value threshold
    Category       priceCategory = Category::Count;
};

}  // namespace PickMyLoot
