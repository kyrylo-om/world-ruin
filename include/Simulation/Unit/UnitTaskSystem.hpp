#pragma once
#include "World/ChunkManager.hpp"
#include "Simulation/World/PathfindingSystem.hpp"
#include <entt/entt.hpp>
#include <memory>

namespace wr::simulation {

    class UnitTaskSystem {
    public:
        static void releaseUnitAction(entt::registry& registry, entt::entity e);
        static void cancelSelectedTasks(entt::registry& registry);
        static void assignWorkerTargets(entt::registry& registry, world::ChunkManager& chunkManager, std::shared_ptr<GlobalObstacleMap> obstacles);
    };

}