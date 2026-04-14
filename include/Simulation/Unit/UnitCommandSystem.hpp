#pragma once
#include "Core/Math.hpp"
#include "World/ChunkManager.hpp"
#include "Simulation/World/PathfindingSystem.hpp"
#include <entt/entt.hpp>
#include <memory>

namespace wr::simulation {
    class UnitCommandSystem {
    public:
        static void processCommands(entt::registry& registry, world::ChunkManager& chunkManager,
                                    bool pJustPressed, bool isCancelCommand,
                                    bool isRightClicking, const math::Vec2f& mouseWorldPos,
                                    std::shared_ptr<GlobalObstacleMap> obstacles);
    };
}