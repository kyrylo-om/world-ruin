#include "Simulation/Unit/Combat/CombatActionResolver.hpp"
#include "ECS/Components.hpp"
#include "ECS/Tags.hpp"
#include "Simulation/Environment/ResourceSystem.hpp"
#include "Rendering/TileHandler.hpp"
#include "Core/Types.hpp"
#include "Core/SimLogger.hpp"

namespace wr::simulation {

void resolveHarvesterAction(entt::registry& registry, entt::entity entity, ecs::UnitData& uData, ecs::AnimationState& anim, ecs::ActionTarget* actionTarget, DeferredQueues& dq) {

    bool canHit = true;
    if (registry.all_of<ecs::RockTag>(actionTarget->target) && uData.type != ecs::UnitType::Miner) canHit = false;
    if (registry.all_of<ecs::TreeTag>(actionTarget->target) && uData.type != ecs::UnitType::Lumberjack) canHit = false;
    if (registry.all_of<ecs::BushTag>(actionTarget->target) && uData.type != ecs::UnitType::Lumberjack && uData.type != ecs::UnitType::Warrior) canHit = false;
    if (registry.all_of<ecs::LogTag>(actionTarget->target)) canHit = false;
    if (registry.all_of<ecs::SmallRockTag>(actionTarget->target)) canHit = false;

    if (canHit) {
        dq.complex.push_back([&registry, workerEnt = entity, target = actionTarget->target]() {
            if (!registry.valid(target) || !registry.valid(workerEnt)) return;
            auto& targetHealth = registry.get<ecs::Health>(target);

            if (targetHealth.current > 0) {
                targetHealth.current -= 1;

                auto* tilesPtr = registry.ctx().find<const rendering::TileHandler*>();
                if (tilesPtr && !registry.all_of<ecs::LogTag>(target) && !registry.all_of<ecs::SmallRockTag>(target)) {
                    world::ResourceSystem::onResourceHit(registry, target, **tilesPtr);
                }

                if (targetHealth.current <= 0) {
                    std::string resType = "resource";
                    if (registry.all_of<ecs::TreeTag>(target)) resType = "Tree";
                    else if (registry.all_of<ecs::RockTag>(target)) resType = "Rock";
                    else if (registry.all_of<ecs::BushTag>(target)) resType = "Bush";
                    auto& tWp = registry.get<ecs::WorldPos>(target);
                    auto& wUData = registry.get<ecs::UnitData>(workerEnt);
                    core::SimLogger::get().log("[Harvest] " + std::string(core::SimLogger::typeName(wUData.type))
                        + " #" + std::to_string(core::SimLogger::eid(workerEnt))
                        + " destroyed " + resType + " #" + std::to_string(core::SimLogger::eid(target))
                        + " at " + core::SimLogger::pos(tWp.wx, tWp.wy));

                    auto& animState = registry.get<ecs::AnimationState>(workerEnt);
                    animState.isActionLocked = false;
                    animState.currentAnim = 0;
                    animState.currentFrame = 0;
                    animState.comboRequested = false;

                    registry.remove<ecs::ActionTarget>(workerEnt);
                    registry.remove<ecs::Path>(workerEnt);
                    registry.remove<ecs::PathRequest>(workerEnt);
                    registry.remove<ecs::ClaimedTag>(target);
                }
            }
        });
    } else {
        anim.isActionLocked = false;
        anim.currentAnim = 0;
        anim.currentFrame = 0;
        anim.comboRequested = false;

        dq.removeActionTarget.push_back(entity);
        dq.removePath.push_back(entity);
        dq.removePathRequest.push_back(entity);
    }
}

}