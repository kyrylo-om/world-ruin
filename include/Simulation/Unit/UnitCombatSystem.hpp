#pragma once
#include "Core/Math.hpp"
#include "World/ChunkManager.hpp"
#include "Simulation/Unit/UnitControlSystem.hpp"
#include "ECS/Components.hpp"
#include <entt/entt.hpp>
#include <vector>

namespace wr::simulation {

    struct CombatTimers {
        double intent = 0.0;
        double triggers = 0.0;
        double resolver = 0.0;
    };

    class UnitCombatSystem {
    public:
        static math::Vec2f processCombat(entt::registry& registry, entt::entity entity, ecs::UnitData& uData, ecs::WorkerState& wState, ecs::CombatStats* cStats, ecs::WorldPos& wp, ecs::Velocity& vel, ecs::AnimationState& anim, ecs::ActionTarget* actionTarget,
                                         const math::Vec2f& mouseWorldPos,
                                         bool isRightClicking,
                                         bool isSpacePressed,
                                         bool spaceJustPressed,
                                         bool isPaused,
                                         float dt,
                                         world::ChunkManager& chunkManager,
                                         DeferredQueues& dq,
                                         CombatTimers& ct);
    };

}