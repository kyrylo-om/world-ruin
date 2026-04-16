#include "../../../include/Simulation/Unit/UnitCombatSystem.hpp"
#include "../../../include/Simulation/Unit/Combat/CombatMovement.hpp"
#include "../../../include/Simulation/Unit/Combat/CombatAnimation.hpp"
#include "../../../include/Simulation/Unit/Combat/CombatActionResolver.hpp"
#include "ECS/Tags.hpp"
#include "ECS/Components.hpp"
#include <chrono>

namespace wr::simulation {

    math::Vec2f UnitCombatSystem::processCombat(entt::registry& registry, entt::entity entity, ecs::UnitData& uData, ecs::WorkerState& wState, ecs::CombatStats* cStats, ecs::WorldPos& wp, ecs::Velocity& vel, ecs::AnimationState& anim, ecs::ActionTarget* actionTarget,
                                                const math::Vec2f& mouseWorldPos,
                                                bool isRightClicking,
                                                bool isSpacePressed,
                                                bool spaceJustPressed,
                                                bool isPaused,
                                                float dt,
                                                world::ChunkManager& chunkManager,
                                                DeferredQueues& dq,
                                                CombatTimers& ct) {

        auto selectedView = registry.view<ecs::SelectedTag>();
        bool isHero = (std::distance(selectedView.begin(), selectedView.end()) == 1 && registry.all_of<ecs::SelectedTag>(entity));
        bool isAttackingCommand = false;

        auto t0 = std::chrono::high_resolution_clock::now();
        math::Vec2f moveIntent = CombatMovement::calculateIntent(registry, entity, uData, wState, cStats, wp, vel, actionTarget, isAttackingCommand, isHero, isSpacePressed, spaceJustPressed, isPaused, dt, chunkManager, dq);

        auto t1 = std::chrono::high_resolution_clock::now();
        ct.intent += std::chrono::duration<double, std::milli>(t1 - t0).count();

        CombatAnimation::processTriggers(anim, uData, isAttackingCommand);

        auto t2 = std::chrono::high_resolution_clock::now();
        ct.triggers += std::chrono::duration<double, std::milli>(t2 - t1).count();

        CombatActionResolver::resolve(registry, entity, uData, wState, cStats, wp, vel, anim, actionTarget, isHero, chunkManager, dq);

        auto t3 = std::chrono::high_resolution_clock::now();
        ct.resolver += std::chrono::duration<double, std::milli>(t3 - t2).count();

        return moveIntent;
    }

}