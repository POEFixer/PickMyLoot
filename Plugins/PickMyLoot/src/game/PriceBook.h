// PriceBook.h - prices from the POEFixer price service (ctx->Prices), memoised.
//
// The plugin makes no HTTP requests: POEFixer loads the league's prices once
// and refreshes them itself (Settings -> Prices). A host lookup that misses
// scans the whole price list, so every answer (found or not) is kept per name
// until the host publishes a new price revision, and new lookups are rationed
// per frame so opening a long list never stalls a frame.
//
// Only stackable-loot categories are trusted: the host also files unique items
// under their base type name, and a rare "Ruby Ring" must not be valued as the
// most expensive unique Ruby Ring.
#pragma once

#include "../../../../POEFixer/plugin_sdk/PluginSDK.h"

#include "LootTypes.h"

#include <string>
#include <unordered_map>

namespace PickMyLoot {

enum class RefCurrency : int { Divine = 0, Exalted = 1, Chaos = 2 };

inline const char* RefShort(RefCurrency r) {
    switch (r) {
        case RefCurrency::Divine:  return "div";
        case RefCurrency::Chaos:   return "c";
        default:                   return "ex";
    }
}

inline const char* RefKey(RefCurrency r) {
    switch (r) {
        case RefCurrency::Divine:  return "divine";
        case RefCurrency::Chaos:   return "chaos";
        default:                   return "exalted";
    }
}

inline RefCurrency RefFromKey(const std::string& k) {
    if (k == "exalted") return RefCurrency::Exalted;
    if (k == "chaos") return RefCurrency::Chaos;
    return RefCurrency::Divine;
}

struct PriceEntry {
    bool     known = false;            // the host has a usable (stackable-loot) price
    float    chaos = 0.f, divine = 0.f, exalt = 0.f;
    Category category = Category::Count;

    double In(RefCurrency r) const {
        if (!known) return -1.0;
        switch (r) {
            case RefCurrency::Divine:  return divine;
            case RefCurrency::Chaos:   return chaos;
            default:                   return exalt;
        }
    }
};

class PriceBook {
public:
    // Once per frame / tick: drops the memo when the host has new prices and
    // grants this frame's lookup budget.
    void BeginFrame(const PluginSDK::Context* ctx, int budget, unsigned long long nowMs) {
        m_budget = budget;
        if (!ctx) return;
        const uint64_t rev = ctx->Prices.GetRevision();
        // Hosts without a revision counter: re-read every 10 minutes.
        const bool stale = rev == 0 && nowMs - m_clearedAt > 600000;
        if (rev != m_revision || stale || m_memo.size() > 4096) {
            m_revision = rev;
            m_clearedAt = nowMs;
            m_memo.clear();
        }
    }

    void Clear() {
        m_memo.clear();
        m_revision = ~0ull;
    }

    // nullptr = not looked up yet and this frame's budget is spent.
    const PriceEntry* Get(const PluginSDK::Context* ctx, const std::string& name) {
        auto it = m_memo.find(name);
        if (it != m_memo.end()) return &it->second;
        if (!ctx || m_budget <= 0) return nullptr;
        --m_budget;
        PriceEntry e;
        const PluginSDK::PriceResult r = ctx->Prices.LookupPrice(name);
        if (r.found) {
            e.category = CategoryFromPriceCategory(r.category);
            if (e.category != Category::Count) {
                e.known = true;
                e.chaos = r.chaos;
                e.divine = r.divine;
                e.exalt = r.exalt;
            }
        }
        return &m_memo.emplace(name, e).first->second;
    }

    size_t Size() const { return m_memo.size(); }

private:
    std::unordered_map<std::string, PriceEntry> m_memo;
    uint64_t m_revision = ~0ull;
    unsigned long long m_clearedAt = 0;
    int m_budget = 0;
};

}  // namespace PickMyLoot
