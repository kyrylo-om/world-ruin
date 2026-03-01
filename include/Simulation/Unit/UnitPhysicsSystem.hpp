#pragma once

#include "World/ChunkManager.hpp"
#include <entt/entt.hpp>
#include <vector>

namespace wr::simulation {

    class UnitPhysicsSystem {
    public:
        static std::vector<entt::entity> getNearbyEntities(entt::registry& registry, entt::entity entity, const world::ChunkManager& chunkManager);

        static void applyPhysicsAndCollisions(entt::registry& registry, entt::entity entity, float dt, world::ChunkManager& chunkManager, const std::vector<entt::entity>& nearbyEntities);
    };

}