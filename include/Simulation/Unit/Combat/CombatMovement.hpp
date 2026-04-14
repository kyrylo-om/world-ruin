#pragma once
#include "Core/Math.hpp"
#include "World/ChunkManager.hpp"
#include "Simulation/Unit/UnitControlSystem.hpp"
#include "ECS/Components.hpp"
#include <entt/entt.hpp>

namespace wr::simulation {
    class CombatMovement {
    public:
        static math::Vec2f calculateIntent(entt::registry& registry, entt::entity entity, ecs::UnitData& uData, ecs::WorkerState& wState, ecs::CombatStats* cStats, ecs::WorldPos& wp, ecs::Velocity& vel, ecs::ActionTarget* actionTarget, bool& outIsAttackingCommand, bool isHero, bool isSpacePressed, bool spaceJustPressed, bool isPaused, float dt, world::ChunkManager& chunkManager, DeferredQueues& dq);
    };
}