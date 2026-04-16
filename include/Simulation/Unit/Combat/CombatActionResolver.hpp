#pragma once
#include "World/ChunkManager.hpp"
#include "Simulation/Unit/UnitControlSystem.hpp"
#include "ECS/Components.hpp"
#include <entt/entt.hpp>
#include "Core/Math.hpp"

namespace wr::simulation {
    class CombatActionResolver {
    public:
        static void resolve(entt::registry& registry, entt::entity entity, ecs::UnitData& uData, ecs::WorkerState& wState, ecs::CombatStats* cStats, ecs::WorldPos& wp, ecs::Velocity& vel, ecs::AnimationState& anim, ecs::ActionTarget* actionTarget, bool isHero, world::ChunkManager& chunkManager, DeferredQueues& dq);
    };
}