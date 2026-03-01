#include "../../../include/Rendering/Utils/FastSorter.hpp"
#include "ECS/Components.hpp"
#include <algorithm>
#include <limits>

namespace wr::rendering {

void FastSorter::sortByScreenY(std::vector<entt::entity>& entities, entt::registry& registry) {
    if (entities.empty()) return;

    if (entities.size() < 64) {
        std::sort(entities.begin(), entities.end(), [&registry](entt::entity a, entt::entity b) {
            return registry.get<ecs::ScreenPos>(a).y < registry.get<ecs::ScreenPos>(b).y;
        });
        return;
    }

    float minY = std::numeric_limits<float>::max();
    float maxY = std::numeric_limits<float>::lowest();

    for (auto e : entities) {
        float y = registry.get<ecs::ScreenPos>(e).y;
        if (y < minY) minY = y;
        if (y > maxY) maxY = y;
    }

    if (minY >= maxY) return;

    const float BUCKET_SIZE = 8.0f;
    int numBuckets = static_cast<int>((maxY - minY) / BUCKET_SIZE) + 1;

    const int MAX_BUCKETS = 8192;
    if (numBuckets > MAX_BUCKETS) numBuckets = MAX_BUCKETS;

    thread_local std::vector<entt::entity> buckets[MAX_BUCKETS];

    for (int i = 0; i < numBuckets; ++i) {
        buckets[i].clear();
    }

    for (auto e : entities) {
        float y = registry.get<ecs::ScreenPos>(e).y;
        int idx = static_cast<int>((y - minY) / BUCKET_SIZE);

        if (idx < 0) idx = 0;
        if (idx >= numBuckets) idx = numBuckets - 1;

        buckets[idx].push_back(e);
    }

    int currentIdx = 0;
    for (int i = 0; i < numBuckets; ++i) {
        if (buckets[i].empty()) continue;

        if (buckets[i].size() > 1) {
            std::sort(buckets[i].begin(), buckets[i].end(), [&registry](entt::entity a, entt::entity b) {
                return registry.get<ecs::ScreenPos>(a).y < registry.get<ecs::ScreenPos>(b).y;
            });
        }

        for (auto e : buckets[i]) {
            entities[currentIdx++] = e;
        }
    }
}

}