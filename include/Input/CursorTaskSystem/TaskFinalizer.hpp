#pragma once

#include "ECS/Components.hpp"
#include <entt/entt.hpp>
#include <vector>

namespace wr::input {
    class TaskFinalizer {
    public:
        static void cancelTask(entt::registry& registry);

        // Scans resources in the pending area and marks matches with PreviewHarvestTag.
        // Respects include flags, excludes dropoff zones and building resource zones.
        // Caller must clear PreviewHarvestTag before calling.
        static void scanResources(entt::registry& registry, const ecs::PendingTaskArea& pArea);

        // Creates a TaskArea from the pending area, converts PreviewHarvestTag to MarkedForHarvestTag,
        // and assigns compatible workers from the provided list.
        // Returns the created task entity, or entt::null if nothing was marked.
        static entt::entity finalizeTask(entt::registry& registry, ecs::PendingTaskArea& pArea,
                                         int& globalTaskId, const std::vector<entt::entity>& workers);
    };
}
