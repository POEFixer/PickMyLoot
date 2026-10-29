// LootScanner.h - ground items in pickup range, nearest first, with their base
// names. Names are resolved once per item (a world item's inner entity and its
// base type name) and cached until the area changes; an item whose name cannot
// be read yet is retried at most once a second instead of every tick. Used by
// the scan worker only (LootWorker.h).
#pragma once

#include "../../../../POEFixer/plugin_sdk/PluginSDK.h"

#include "LootTypes.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <vector>

namespace PickMyLoot {

class LootScanner {
public:
    void ClearCache() { m_cache.clear(); }

    // snap must hold the player and the items (GetSnapshot(EntityType::Item)).
    // Fills `out` (reused between calls to avoid reallocating). englishBudget: the most host
    // lookups of English identity names in this pass (each is a price lookup on the host).
    void Scan(const PluginSDK::Context* ctx, const PluginSDK::Snapshot& snap, int pickupRange, float dispW, float dispH, unsigned long long nowMs, std::vector<GroundItem>& out,
              int englishBudget = 32) {
        out.clear();
        if (!ctx || !snap.Player.IsValid) return;
        if (m_cache.size() > 4096) m_cache.clear();

        const float px = snap.Player.GridPositionX;
        const float py = snap.Player.GridPositionY;
        const float range = static_cast<float>(pickupRange);
        const float range2 = range * range;

        for (const PluginSDK::Entity& e : snap.Entities) {
            if (!e.IsValid || e.EntityType != PluginSDK::EntityType::Item) continue;
            if (!e.Components.HasWorldItem() || e.Address == 0) continue;
            const float dx = e.GridPositionX - px;
            const float dy = e.GridPositionY - py;
            const float d2 = dx * dx + dy * dy;
            if (d2 > range2) continue;

            CacheEntry* r = Resolve(ctx, e, nowMs);
            if (!r || r->baseName.empty()) continue;

            GroundItem g;
            g.id = e.Id;
            g.address = e.Address;
            g.gridX = e.GridPositionX;
            g.gridY = e.GridPositionY;
            g.worldX = e.WorldX;
            g.worldY = e.WorldY;
            g.worldZ = e.WorldZ;
            g.baseName = r->baseName;
            g.englishName = EnglishName(ctx, e.Address, *r, englishBudget, nowMs);
            g.path = r->path;
            g.distance = std::sqrt(d2);
            float sx = 0.f, sy = 0.f;
            if (ctx->Render.WorldToScreen(e.WorldX, e.WorldY, e.WorldZ, sx, sy)) {
                g.screenX = sx;
                g.screenY = sy;
                g.onScreen = sx >= 0.f && sy >= 0.f && sx < dispW && sy < dispH;
            }
            out.push_back(std::move(g));
        }
        std::sort(out.begin(), out.end(), [](const GroundItem& a, const GroundItem& b) { return a.distance < b.distance; });
    }

    // An item the host could not name is asked again this long after (once a second at most).
    static constexpr unsigned long long kEnglishRetryMs = 1000;

private:
    struct CacheEntry {
        uintptr_t   address = 0;
        unsigned long long retryAt = 0;  // for an unresolved entry
        std::string baseName;
        // The English identity name, once the host gave it (the shown name on an older host); ""
        // until then. A failed lookup is never kept: englishRetryAt says when to ask again.
        std::string englishName;
        unsigned long long englishRetryAt = 0;   // 0 = no lookup failed
        std::string path;
    };

    // The English identity name of an item for this pass: the host prices it by identity and
    // names it in English (desktop 371+), so the lists and ticks (English game names) work in
    // every game language; an older host keeps the name the game shows, as before. Each host
    // call is a price lookup, so a pass makes at most `budget` of them; "" = not asked yet (the
    // item waits for a later pass). An item the host could not name (its item data still
    // downloading) goes by the name the game shows meanwhile and is asked again after
    // kEnglishRetryMs: the shown name is never remembered as its English one.
    static std::string EnglishName(const PluginSDK::Context* ctx, uintptr_t entity, CacheEntry& c, int& budget,
                                   unsigned long long nowMs) {
        if (!c.englishName.empty()) return c.englishName;
        if (!ctx->Prices.CanLookupByItem()) {
            c.englishName = c.baseName;
            return c.englishName;
        }
        const bool failedBefore = c.englishRetryAt != 0;
        if ((failedBefore && nowMs < c.englishRetryAt) || budget <= 0) return failedBefore ? c.baseName : std::string();
        --budget;
        std::string english;
        ctx->Prices.LookupPriceByItem(entity, &english);
        if (!english.empty()) {
            c.englishName = english;
            c.englishRetryAt = 0;
            return english;
        }
        c.englishRetryAt = nowMs + kEnglishRetryMs;
        return c.baseName;
    }

    CacheEntry* Resolve(const PluginSDK::Context* ctx, const PluginSDK::Entity& e, unsigned long long nowMs) {
        auto it = m_cache.find(e.Id);
        if (it != m_cache.end() && it->second.address == e.Address) {
            if (!it->second.baseName.empty() || nowMs < it->second.retryAt) return &it->second;
        }
        CacheEntry c;
        c.address = e.Address;
        const auto inner = ctx->Entities.GetWorldItemInner(e.Address);
        if (inner && inner->IsValid && inner->Address != 0) {
            c.path = ToNarrow(inner->Path);
            c.baseName = ctx->Inventory.ReadItemBaseTypeName(inner->Address);
        }
        if (c.baseName.empty()) c.retryAt = nowMs + 1000;
        CacheEntry& slot = m_cache[e.Id];
        slot = std::move(c);
        return &slot;
    }

    std::unordered_map<uint32_t, CacheEntry> m_cache;
};

}  // namespace PickMyLoot
