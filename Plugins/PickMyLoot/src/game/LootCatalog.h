// LootCatalog.h - the base names offered in the "What to pick" lists, by
// category.
//
// Sources, merged: the built-in list below; names from config/cache.json (the
// list the original plugin downloaded from poe2scout for the league, kept so
// existing users keep their full lists); and names learned in play: a ground
// item the catalog does not know joins the category the host price service
// files it under, so new league items appear without a plugin update.
// cache.json keeps the original format ({"names": {"<category>": [...]}}) and
// is written atomically; prices are no longer stored there. A cache.json that
// does not parse is copied to cache.json.corrupt before it can be replaced; one
// that exists but cannot be read is never replaced in that session.
#pragma once

#include "LootTypes.h"
#include "../common/FileUtil.h"

#include <array>
#include <filesystem>
#include <initializer_list>
#include <string>
#include <unordered_map>
#include <vector>

namespace PickMyLoot {

class LootCatalog {
public:
    LootCatalog() { Build(); }

    const std::vector<std::string>& Names(Category c) const {
        return m_names[static_cast<size_t>(c)];
    }

    bool Knows(const std::string& baseName) const { return m_byName.find(baseName) != m_byName.end(); }

    Category CategoryOf(const std::string& baseName, const std::string& path) const {
        auto it = m_byName.find(baseName);
        if (it != m_byName.end()) return it->second;
        return ClassifyFallback(baseName, path);
    }

    // Adds a name seen in play. Returns true when it was new.
    bool Learn(const std::string& baseName, Category c) {
        if (baseName.empty() || baseName.size() > 128 || c == Category::Count) return false;
        if (Knows(baseName)) return false;
        m_byName.emplace(baseName, c);
        m_names[static_cast<size_t>(c)].push_back(baseName);
        m_dirty = true;
        return true;
    }

    bool Dirty() const { return m_dirty; }

    static std::filesystem::path CachePath(const std::filesystem::path& pluginDir) {
        return pluginDir / "config" / "cache.json";
    }

    void Load(const std::filesystem::path& pluginDir) {
        std::string text;
        const PfIo::ReadStatus rs = PfIo::ReadTextFileChecked(CachePath(pluginDir), text);
        if (rs == PfIo::ReadStatus::Unreadable) m_saveBlocked = true;
        if (rs != PfIo::ReadStatus::Ok) return;
        const nlohmann::json j = PfIo::ParseJson(text);
        if (!j.is_object()) {
            PfIo::KeepCorruptCopy(CachePath(pluginDir));
            return;
        }
        const auto names = j.find("names");
        if (names == j.end() || !names->is_object()) return;
        for (int i = 0; i < kCategoryCount; ++i) {
            const auto arr = names->find(std::to_string(i));
            if (arr == names->end() || !arr->is_array()) continue;
            for (const auto& e : *arr) {
                if (!e.is_string()) continue;
                const std::string n = e.get<std::string>();
                if (n.empty() || n.size() > 128 || Knows(n)) continue;
                m_byName.emplace(n, static_cast<Category>(i));
                m_names[static_cast<size_t>(i)].push_back(n);
            }
        }
        m_dirty = false;
    }

    bool Save(const std::filesystem::path& pluginDir) {
        if (pluginDir.empty() || m_saveBlocked) return false;
        nlohmann::json names = nlohmann::json::object();
        for (int i = 0; i < kCategoryCount; ++i) names[std::to_string(i)] = m_names[static_cast<size_t>(i)];
        nlohmann::json j;
        j["names"] = std::move(names);
        if (!PfIo::WriteTextFileAtomic(CachePath(pluginDir), PfIo::DumpJson(j, -1))) return false;
        m_dirty = false;
        return true;
    }

private:
    std::array<std::vector<std::string>, kCategoryCount> m_names;
    std::unordered_map<std::string, Category> m_byName;
    bool m_dirty = false;
    bool m_saveBlocked = false;  // cache.json exists but could not be read

    void Set(Category c, std::initializer_list<const char*> list) {
        auto& v = m_names[static_cast<size_t>(c)];
        for (const char* s : list) {
            std::string n(s);
            if (m_byName.find(n) != m_byName.end()) continue;
            m_byName.emplace(n, c);
            v.push_back(std::move(n));
        }
    }

    void Build() {
        Set(Category::Currency, {
            "Mirror of Kalandra", "Hinekora's Lock", "Fracturing Orb", "Perfect Chaos Orb",
            "Perfect Exalted Orb", "Divine Orb", "Orb of Annulment", "Cryptic Key",
            "Greater Chaos Orb", "Perfect Jeweller's Orb", "Chaos Orb", "Perfect Regal Orb",
            "Perfect Orb of Augmentation", "Perfect Orb of Transmutation", "Transmutation Shard",
            "Orb of Chance", "Gemcutter's Prism", "Arcanist's Etcher", "Vaal Orb",
            "Greater Exalted Orb", "Greater Regal Orb", "Glassblower's Bauble", "Artificer's Orb",
            "Greater Jeweller's Orb", "Armourer's Scrap", "Blacksmith's Whetstone", "Orb of Alchemy",
            "Exalted Orb", "Greater Orb of Transmutation", "Chance Shard", "Regal Orb",
            "Orb of Augmentation", "Greater Orb of Augmentation", "Orb of Transmutation",
            "Artificer's Shard", "Regal Shard", "Scroll of Wisdom", "Lesser Jeweller's Orb",
        });
        Set(Category::Essence, {
            "Essence of Delirium", "Lesser Essence of Ice", "Essence of Hysteria", "Essence of Horror",
            "Essence of Opulence", "Essence of the Breach", "Lesser Essence of Alacrity",
            "Essence of Abrasion", "Essence of Enhancement", "Perfect Essence of the Infinite",
            "Perfect Essence of Grounding", "Perfect Essence of Seeking", "Essence of Sorcery",
            "Lesser Essence of Sorcery", "Essence of Command", "Essence of Thawing",
            "Essence of the Infinite", "Perfect Essence of Opulence", "Lesser Essence of Ruin",
            "Essence of Haste", "Perfect Essence of Enhancement", "Perfect Essence of Haste",
            "Essence of Insulation", "Lesser Essence of Grounding", "Essence of the Abyss",
            "Essence of Seeking", "Lesser Essence of Flames", "Perfect Essence of Alacrity",
            "Greater Essence of Opulence", "Perfect Essence of Battle", "Lesser Essence of Opulence",
            "Perfect Essence of the Mind", "Essence of Insanity", "Lesser Essence of Electricity",
            "Lesser Essence of Abrasion", "Perfect Essence of Abrasion", "Lesser Essence of Thawing",
            "Perfect Essence of Thawing", "Greater Essence of Seeking", "Essence of Alacrity",
            "Greater Essence of Ice", "Perfect Essence of Electricity", "Greater Essence of Abrasion",
            "Lesser Essence of Haste", "Lesser Essence of Battle", "Greater Essence of Ruin",
            "Essence of the Body", "Perfect Essence of Command", "Greater Essence of Haste",
            "Lesser Essence of Insulation", "Perfect Essence of Ice", "Greater Essence of the Infinite",
            "Lesser Essence of Command", "Lesser Essence of the Infinite", "Greater Essence of Command",
            "Essence of Ice", "Perfect Essence of Flames", "Greater Essence of Enhancement",
            "Perfect Essence of Sorcery", "Greater Essence of Sorcery", "Greater Essence of Grounding",
            "Lesser Essence of the Mind", "Essence of Flames", "Essence of Ruin", "Essence of the Mind",
            "Essence of Electricity",
        });
        Set(Category::Rune, {
            "Aldur's Legacy", "Serle's Triumph", "Perfect Body Rune", "Astrid's Creativity",
            "Perfect Stone Rune", "Perfect Charging Rune", "Perfect Adept Rune", "Perfect Robust Rune",
            "Perfect Storm Rune", "Perfect Resolve Rune", "Perfect Vision Rune", "Perfect Desert Rune",
            "Perfect Mind Rune", "Perfect Glacial Rune", "Greater Rune of Leadership",
            "Perfect Rebirth Rune", "Perfect Iron Rune", "Perfect Inspiration Rune", "Perfect Ward Rune",
            "Kolr's Hunt", "Cadigan's Epiphany", "Greater Rune of Alacrity", "Masterwork Rune",
            "Warding Rune of Stability", "Greater Rune of Tithing", "Uhtred's Sidereus",
            "Greater Ward Rune", "Iron Rune", "Warding Rune of Reinforcement", "Ward Rune",
            "Medved's Tending", "Lesser Resolve Rune", "Lesser Robust Rune",
            "Hedgewitch Assandra's Rune of Wisdom", "Thrud's Might", "Saqawal's Rune of Erosion",
            "Rune of Foundations", "Rune of Renown", "Warding Rune of Armature", "Katla's Gloom",
            "Storm Rune", "Warding Rune of Heart", "Rune of the Blossom", "Vision Rune",
            "Ancient Rune of Discovery", "Farrul's Rune of the Chase", "Greater Charging Rune",
            "Lesser Body Rune", "Rune of Reach", "Warding Rune of Protection", "Saqawal's Rune of the Sky",
            "Mind Rune", "Fenumus' Rune of Agony", "Robust Rune", "Fenumus' Rune of Draining",
            "Resolve Rune", "The Greatwolf's Rune of Willpower", "Rune of Acrobatics",
            "Saqawal's Rune of Memory", "Thane Grannell's Rune of Mastery", "Rune of the Prism",
            "Craiceann's Rune of Recovery", "Desert Rune", "Warding Rune of Annihilation",
            "Rune of Culmination", "Fenumus' Rune of Spinning", "Vorana's Carnage",
        });
        Set(Category::SoulCore, {
            "Xopec's Soul Core of Power", "Soul Core of Quipolatl", "Soul Core of Azcapa",
            "Tzamoto's Soul Core of Ferocity", "Soul Core of Zalatl", "Opiloti's Soul Core of Assault",
            "Soul Core of Cholotl", "Quipolatl's Soul Core of Flow", "Soul Core of Jiquani",
            "Soul Core of Citaqualotl", "Soul Core of Zantipi", "Guatelitzi's Soul Core of Endurance",
            "Soul Core of Tacati", "Soul Core of Ticaba", "Zalatl's Soul Core of Insulation",
            "Hayoxi's Soul Core of Heatproofing", "Citaqualotl's Soul Core of Foulness",
            "Soul Core of Xopec", "Soul Core of Puhuarte", "Soul Core of Atmohua",
            "Estazunti's Soul Core of Convalescence", "Soul Core of Tzamoto", "Soul Core of Opiloti",
            "Soul Core of Topotante", "Xipocado's Soul Core of Dominion", "Atmohua's Soul Core of Retreat",
            "Tacati's Soul Core of Affliction", "Topotante's Soul Core of Dampening",
            "Cholotl's Soul Core of War", "Uromoti's Soul Core of Attenuation",
        });
        Set(Category::Catalyst, {
            "Refined Sibilant Catalyst", "Refined Reaver Catalyst", "Refined Necrotic Catalyst",
            "Sibilant Catalyst", "Skittering Catalyst", "Reaver Catalyst", "Refined Carapace Catalyst",
            "Refined Tul's Catalyst", "Refined Esh's Catalyst", "Refined Skittering Catalyst",
            "Refined Chayula's Catalyst", "Refined Adaptive Catalyst", "Refined Neural Catalyst",
            "Refined Flesh Catalyst", "Refined Xoph's Catalyst", "Refined Uul-Netol's Catalyst",
            "Neural Catalyst", "Esh's Catalyst", "Xoph's Catalyst", "Necrotic Catalyst", "Tul's Catalyst",
            "Carapace Catalyst", "Chayula's Catalyst", "Adaptive Catalyst", "Flesh Catalyst",
            "Uul-Netol's Catalyst",
        });
        Set(Category::Delirium, {
            "Ancient Potent Liquid Contempt", "Potent Liquid Ferocity", "Potent Liquid Contempt",
            "Concentrated Liquid Isolation", "Concentrated Liquid Suffering",
            "Ancient Potent Liquid Ferocity", "Ancient Potent Liquid Melancholy",
            "Ancient Concentrated Liquid Isolation", "Ancient Concentrated Liquid Suffering",
            "Ancient Liquid Disgust", "Concentrated Liquid Fear", "Potent Liquid Melancholy",
            "Liquid Despair", "Ancient Liquid Despair", "Ancient Concentrated Liquid Fear",
            "Liquid Paranoia", "Liquid Disgust", "Liquid Envy", "Diluted Liquid Greed",
            "Ancient Liquid Paranoia", "Diluted Liquid Guilt", "Diluted Liquid Ire", "Ancient Liquid Envy",
            "Ancient Diluted Liquid Greed", "Ancient Diluted Liquid Ire", "Ancient Diluted Liquid Guilt",
        });
        Set(Category::Incursion, {
            "Jiquani's Thesis", "Guatelitzi's Thesis", "Architect's Orb", "Vaal Cultivation Orb",
            "Core Destabiliser", "Crystallised Corruption", "Quipolatl's Thesis", "Orb of Extraction",
            "Citaqualotl's Thesis", "Ancient Infuser", "Vaal Siphoner",
        });
        Set(Category::Idol, {
            "Fox Idol", "Idol of Maxarius", "Rabbit Idol", "Idol of Eeshta", "Owl Idol", "Idol of Egrin",
            "Idol of Sirrius", "Primate Idol", "Idol of Ralakesh", "Idol of Grold", "Cat Idol",
            "Idol of Thruldana", "Ox Idol", "Bear Idol", "Boar Idol", "Wolf Idol", "Stag Idol",
        });
        Set(Category::Ritual, {
            "Raven-Touched Shard", "Omen of Chance", "Omen of Sinistral Annulment",
            "Omen of Sinistral Erasure", "Omen of Dextral Erasure", "Omen of Whittling",
            "Omen of Dextral Annulment", "Omen of Light", "Head of the King", "Omen of Secret Compartments",
            "Omen of the Hunt", "Omen of the Blessed", "Omen of Chaotic Quantity", "Omen of Sanctification",
            "Omen of Chaotic Rarity", "Omen of the Blackblooded", "Omen of Dextral Crystallisation",
            "Omen of Reinforcements", "Omen of Chaotic Monsters", "Omen of Abyssal Echoes",
            "Idol of Yeena", "Omen of Sinistral Crystallisation", "Omen of Answered Prayers",
            "Idol of Eramir", "Idol of the Martyr", "Idol of the Pharisee", "Omen of Chaotic Effectiveness",
            "Idol of Alira", "Omen of Bartering", "Omen of Sinistral Exaltation", "Omen of Resurgence",
            "Idol of the Sycophant", "Omen of Catalysing Exaltation", "Call of the Shadows",
            "Omen of Amelioration", "Idol of Greust", "Omen of Dextral Exaltation", "Omen of the Ancients",
            "Idol of Kraityn", "Omen of Greater Exaltation", "Omen of the Sovereign", "Snake Idol",
            "Omen of Dextral Necromancy", "Omen of Refreshment", "Idol of Oak",
            "Omen of Sinistral Necromancy", "Omen of Putrefaction", "Idol of Silk", "Omen of Gambling",
            "Omen of the Liege",
        });
        Set(Category::VaultKey, {
            "The Trialmaster's Reliquary Key", "Zarokh's Reliquary Key: Against the Darkness",
            "Xesht's Reliquary Key", "Tangmazu's Reliquary Key", "The Arbiter's Reliquary Key",
            "Olroth's Reliquary Key", "Ritualistic Reliquary Key", "Twilight Reliquary Key",
        });
        Set(Category::Fragment, {
            "Azmeri Reliquary Key", "Faded Crisis Fragment", "Breachlord Sac", "Cowardly Fate",
            "Origin Core", "Deadly Fate", "Victorious Fate", "Origin Cradle", "Origin Spark",
            "Simulacrum", "Ancient Crisis Fragment", "Kulemak's Invitation", "Weathered Crisis Fragment",
            "Breachstone", "An Audience with the King", "Simulacrum Splinter", "Breach Splinter",
        });
        Set(Category::Abyss, {
            "Kurgal's Gaze", "Preserved Cranium", "Altered Collarbone", "Ancient Collarbone",
            "Ancient Jawbone", "Ancient Rib", "Tecrod's Gaze", "Ulaman's Gaze", "Amanamu's Gaze",
            "Gnawed Rib", "Preserved Collarbone", "Gnawed Collarbone", "Gnawed Jawbone",
            "Preserved Jawbone", "Preserved Rib",
        });
        Set(Category::UncutGem, {
            "Uncut Skill Gem (Level 1)", "Uncut Skill Gem (Level 2)", "Uncut Skill Gem (Level 3)",
            "Uncut Skill Gem (Level 4)", "Uncut Skill Gem (Level 5)", "Uncut Skill Gem (Level 6)",
            "Uncut Skill Gem (Level 7)", "Uncut Skill Gem (Level 8)", "Uncut Skill Gem (Level 9)",
            "Uncut Skill Gem (Level 10)", "Uncut Skill Gem (Level 11)", "Uncut Skill Gem (Level 12)",
            "Uncut Skill Gem (Level 13)", "Uncut Skill Gem (Level 14)", "Uncut Skill Gem (Level 15)",
            "Uncut Skill Gem (Level 16)", "Uncut Skill Gem (Level 17)", "Uncut Skill Gem (Level 18)",
            "Uncut Skill Gem (Level 19)", "Uncut Skill Gem (Level 20)", "Uncut Support Gem (Level 1)",
            "Uncut Support Gem (Level 2)", "Uncut Support Gem (Level 3)", "Uncut Support Gem (Level 4)",
            "Uncut Support Gem (Level 5)", "Uncut Spirit Gem (Level 4)", "Uncut Spirit Gem (Level 5)",
            "Uncut Spirit Gem (Level 6)", "Uncut Spirit Gem (Level 7)", "Uncut Spirit Gem (Level 8)",
            "Uncut Spirit Gem (Level 9)", "Uncut Spirit Gem (Level 10)", "Uncut Spirit Gem (Level 11)",
            "Uncut Spirit Gem (Level 12)", "Uncut Spirit Gem (Level 13)", "Uncut Spirit Gem (Level 14)",
            "Uncut Spirit Gem (Level 15)", "Uncut Spirit Gem (Level 16)", "Uncut Spirit Gem (Level 17)",
            "Uncut Spirit Gem (Level 18)", "Uncut Spirit Gem (Level 19)", "Uncut Spirit Gem (Level 20)",
        });
        Set(Category::LineageGem, {
            "Garukhan's Resolve", "Uul-Netol's Embrace", "Atziri's Communion", "Dialla's Desire",
            "Uhtred's Exodus", "Seraph's Heart", "Her Declaration", "Rigwald's Ferocity", "Rakiata's Flow",
            "Esh's Radiance", "Tul's Stillness", "Uhtred's Augury", "Xoph's Pyre", "Uhtred's Omen",
            "Vorana's Siege", "Olroth's Conviction", "Breachlord's Rift", "Ixchel's Torment",
            "Breachlord's Amalgam", "Atalui's Bloodletting", "Khatal's Rejuvenation", "Arbiter's Ignition",
            "Atziri's Allure", "Esh's Prowess", "Catha's Brilliance", "Amanamu's Tithe",
            "Morrigan's Insight", "Medved's Felling", "Sione's Temper", "Zarokh's Revolt",
            "Xibaqua's Rending", "Zerphi's Infamy", "Uhtred's Constellation", "Arbiter's Reach",
            "Ailith's Chimes", "Uhtred's Rite", "Doedre's Undoing", "Styrn's Mountain", "Styrn's Ferocity",
            "Brutus' Brain", "Kurgal's Leash", "Ahn's Citadel", "Dominus' Grasp", "Hayoxi's Fulmination",
            "Vilenta's Propulsion", "Zarokh's Refrain", "Olroth's Hubris", "Arakaali's Lust",
            "Kaom's Madness", "Atziri's Impatience", "Kulemak's Dominion", "Paquate's Pact",
            "Trickster's Shard", "Vruun's Aftermath", "Tecrod's Revenge", "Tul's Avalanche", "Oisin's Oath",
            "Prototype Seventeen", "Morgana's Tempest", "Tangmazu's Thurible", "Einhar's Beastrite",
            "Kalisa's Crescendo",
        });
        Set(Category::Verisium, {
            "Celestial Alloy", "Sovereign Alloy", "Veridical Starlit Ore", "Warding Starlit Ore",
            "Olroth's Crest of the Sun", "Revered Starlit Ore", "Venerable Starlit Ore",
            "The Runebinder's Alloy", "Adaptive Alloy", "Transcendent Alloy", "Liquid Verisium",
            "Protective Alloy", "Mystic Alloy", "Prismatic Alloy", "Cyclonic Alloy", "Runic Alloy",
            "Expansive Alloy", "Swift Alloy", "Uhtred's Crest of the Chalice", "The Runefather's Alloy",
            "Exceptional Verisium", "Medved's Crest of the Circle", "Vorana's Crest of the Scythe",
            "Verisium",
        });
        Set(Category::Vaal, {
            "Vaal Catalysing Infuser", "Vaal Armourer's Infuser", "Kopec's Orb of Sacrifice",
            "Vaal Blacksmith's Infuser", "Yaomac's Orb of Sacrifice", "Kamasa's Orb of Sacrifice",
            "Yugul's Orb of Sacrifice", "Vaal Arcanist's Infuser",
        });
        Set(Category::Expedition, {
            "Carved Majesty", "Perfect Flux", "Aldur's Saga", "Emergent Possibility", "Emergent Vigour",
            "Carved Cunning", "Uhtred's Saga", "Expedition Logbook", "Carved Tenacity",
            "Emergent Protection", "Thaumaturgic Flux (Level 20)", "Emergent Instinct", "Carved Mischief",
            "Olroth's Saga", "Thaumaturgic Flux (Level 13)", "Thaumaturgic Flux (Level 11)",
            "Thaumaturgic Flux (Level 9)", "Void Flux", "Blazing Flux", "Crackling Flux", "Chilling Flux",
            "Thaumaturgic Flux (Level 14)", "Medved's Saga", "Thaumaturgic Flux (Level 12)",
            "Thaumaturgic Flux (Level 18)", "Thaumaturgic Flux (Level 8)", "Vorana's Saga",
            "Thaumaturgic Flux (Level 16)", "Thaumaturgic Flux (Level 10)", "Thaumaturgic Flux (Level 17)",
            "Thaumaturgic Flux (Level 19)", "Thaumaturgic Flux (Level 15)",
        });
        Set(Category::Waystone, {
            "Waystone (Tier 1)", "Waystone (Tier 2)", "Waystone (Tier 3)", "Waystone (Tier 4)",
            "Waystone (Tier 5)", "Waystone (Tier 6)", "Waystone (Tier 7)", "Waystone (Tier 8)",
            "Waystone (Tier 9)", "Waystone (Tier 10)", "Waystone (Tier 11)", "Waystone (Tier 12)",
            "Waystone (Tier 13)", "Waystone (Tier 14)", "Waystone (Tier 15)", "Waystone (Tier 16)",
        });
        Set(Category::Tablet, {
            "Precursor Tablet", "Breach Precursor Tablet", "Ritual Precursor Tablet",
            "Abyss Precursor Tablet", "Delirium Precursor Tablet", "Expedition Precursor Tablet",
            "Overseer Precursor Tablet",
        });
    }
};

}
