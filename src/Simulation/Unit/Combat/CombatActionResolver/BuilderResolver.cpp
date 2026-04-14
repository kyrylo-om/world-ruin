#include "Simulation/Unit/Combat/CombatActionResolver.hpp"
#include "ECS/Components.hpp"
#include "ECS/Tags.hpp"
#include "Core/Types.hpp"
#include <algorithm>

namespace wr::simulation {

void resolveBuilderAction(entt::registry& registry, entt::entity entity, ecs::UnitData& uData, ecs::AnimationState& anim, ecs::ActionTarget* actionTarget, DeferredQueues& dq) {

    if (registry.all_of<ecs::BuildingTag>(actionTarget->target)) {

        dq.complex.push_back([&registry, workerEnt = entity, target = actionTarget->target]() {
            if (!registry.valid(target) || !registry.valid(workerEnt)) return;
            auto& cData = registry.get<ecs::ConstructionData>(target);

            if (!cData.isBuilt) {
                float progressIncrement = 0.3f;
                float newProgress = std::min(cData.buildProgress + progressIncrement, cData.maxTime);

                float progressRatio = newProgress / cData.maxTime;
                int expectedWood = static_cast<int>(cData.initialWood * progressRatio);
                int expectedRock = static_cast<int>(cData.initialRock * progressRatio);

                int woodToConsume = expectedWood - (cData.initialWood - cData.woodRequired);
                int rockToConsume = expectedRock - (cData.initialRock - cData.rockRequired);

                bool allConsumed = true;

                auto& bWp = registry.get<ecs::WorldPos>(target);
                float minX = bWp.wx - cData.resourceZoneWidth / 2.0f;
                float maxX = bWp.wx + cData.resourceZoneWidth / 2.0f;
                float minY = bWp.wy - cData.resourceZoneHeight / 2.0f;
                float maxY = bWp.wy + cData.resourceZoneHeight / 2.0f;

                auto resView = registry.view<ecs::WorldPos, ecs::ResourceTag>();

                for (int i = 0; i < woodToConsume; ++i) {
                    bool found = false;
                    for (auto r : resView) {
                        if (registry.all_of<ecs::LogTag>(r)) {
                            auto& resData = registry.get<ecs::ResourceData>(r);
                            if (resData.isDestroyed) continue;
                            auto& rWp = resView.get<ecs::WorldPos>(r);
                            if (rWp.wx >= minX && rWp.wx <= maxX && rWp.wy >= minY && rWp.wy <= maxY) {
                                resData.isDestroyed = true;
                                registry.destroy(r);
                                cData.woodRequired--;
                                found = true;
                                break;
                            }
                        }
                    }
                    if (!found) allConsumed = false;
                }

                for (int i = 0; i < rockToConsume; ++i) {
                    bool found = false;
                    for (auto r : resView) {
                        if (registry.all_of<ecs::SmallRockTag>(r)) {
                            auto& resData = registry.get<ecs::ResourceData>(r);
                            if (resData.isDestroyed) continue;
                            auto& rWp = resView.get<ecs::WorldPos>(r);
                            if (rWp.wx >= minX && rWp.wx <= maxX && rWp.wy >= minY && rWp.wy <= maxY) {
                                resData.isDestroyed = true;
                                registry.destroy(r);
                                cData.rockRequired--;
                                found = true;
                                break;
                            }
                        }
                    }
                    if (!found) allConsumed = false;
                }

                if (allConsumed) {
                    cData.buildProgress = newProgress;

                    if (cData.buildProgress > 0.0f && !registry.all_of<ecs::SolidTag>(target)) {
                        registry.emplace_or_replace<ecs::SolidTag>(target);
                    }

                    if (cData.buildProgress >= cData.maxTime) {
                        cData.isBuilt = true;
                        cData.woodRequired = 0;
                        cData.rockRequired = 0;

                        auto& animState = registry.get<ecs::AnimationState>(workerEnt);
                        animState.isActionLocked = false;
                        animState.currentAnim = 0;
                        animState.currentFrame = 0;

                        registry.remove<ecs::ActionTarget>(workerEnt);
                        registry.remove<ecs::Path>(workerEnt);
                        registry.remove<ecs::PathRequest>(workerEnt);

                        auto& wState = registry.get<ecs::WorkerState>(workerEnt);

                        if (!wState.taskQueue.empty()) {
                            if (wState.taskQueue.front() == target) wState.taskQueue.erase(wState.taskQueue.begin());
                            if (!wState.taskQueue.empty()) {
                                wState.currentTask = wState.taskQueue.front();
                                registry.emplace_or_replace<ecs::ActionTarget>(workerEnt, wState.currentTask);
                            } else {
                                wState.currentTask = entt::null;
                                registry.remove<ecs::WorkerTag>(workerEnt);
                            }
                        } else {
                            wState.currentTask = entt::null;
                            registry.remove<ecs::WorkerTag>(workerEnt);
                        }
                    }
                } else {
                    registry.get<ecs::AnimationState>(workerEnt).isActionLocked = false;
                }
            } else {
                auto& wState = registry.get<ecs::WorkerState>(workerEnt);
                wState.currentTask = entt::null;

                registry.remove<ecs::ActionTarget>(workerEnt);
                registry.remove<ecs::Path>(workerEnt);
                registry.remove<ecs::PathRequest>(workerEnt);
                registry.remove<ecs::WorkerTag>(workerEnt);
            }
        });
    }
}

}