#pragma once

#include "Core/Math.hpp"
#include "Rendering/Perspective.hpp"
#include "World/ChunkManager.hpp"
#include <entt/entt.hpp>
#include <vector>

namespace wr::simulation {

    class UnitCombatSystem {
    public:

        static math::Vec2f processCombat(entt::registry& registry, entt::entity entity,
                                         const std::vector<entt::entity>& nearbyEntities,
                                         const math::Vec2f& mouseWorldPos,
                                         bool isRightClicking,
                                         bool isSpacePressed,
                                         bool spaceJustPressed,
                                         float dt,
                                         world::ChunkManager& chunkManager);
    };

}