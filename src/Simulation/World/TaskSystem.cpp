#include "../../../include/Simulation/World/TaskSystem.hpp"
#include "ECS/Components.hpp"
#include "ECS/Tags.hpp"
#include "Core/SimLogger.hpp"
#include <algorithm>
#include <vector>

namespace wr::simulation {

    void TaskSystem::update(entt::registry& registry) {
        auto taskView = registry.view<ecs::TaskArea>();
        auto markedView = registry.view<ecs::MarkedForHarvestTag, ecs::WorldPos>();
        auto workerView = registry.view<ecs::UnitData, ecs::WorkerState>();
        auto unmarkedResView = registry.view<ecs::WorldPos, ecs::ResourceTag>(entt::exclude<ecs::MarkedForHarvestTag>);

        std::vector<entt::entity> completedTasks;

        for (auto taskEnt : taskView) {
            auto& taskArea = taskView.get<ecs::TaskArea>(taskEnt);

            if (taskArea.collectFutureDrops) {
                for (auto resEnt : unmarkedResView) {
                    if (registry.all_of<ecs::LogTag>(resEnt) || registry.all_of<ecs::SmallRockTag>(resEnt)) {
                        auto& wp = unmarkedResView.get<ecs::WorldPos>(resEnt);
                        bool inside = false;
                        for (const auto& rect : taskArea.areas) {
                            float minX = std::min(rect.startWorld.x, rect.endWorld.x);
                            float maxX = std::max(rect.startWorld.x, rect.endWorld.x);
                            float minY = std::min(rect.startWorld.y, rect.endWorld.y);
                            float maxY = std::max(rect.startWorld.y, rect.endWorld.y);
                            if (wp.wx >= minX && wp.wx <= maxX && wp.wy >= minY && wp.wy <= maxY) {
                                inside = true; break;
                            }
                        }

                        if (inside && taskArea.hasDropoff) {
                            float dMinX = std::min(taskArea.dropoffStart.x, taskArea.dropoffEnd.x);
                            float dMaxX = std::max(taskArea.dropoffStart.x, taskArea.dropoffEnd.x);
                            float dMinY = std::min(taskArea.dropoffStart.y, taskArea.dropoffEnd.y);
                            float dMaxY = std::max(taskArea.dropoffStart.y, taskArea.dropoffEnd.y);
                            if (wp.wx >= dMinX && wp.wx <= dMaxX && wp.wy >= dMinY && wp.wy <= dMaxY) {
                                inside = false;
                            }
                        }

                        if (inside) {
                            auto bView = registry.view<ecs::BuildingTag, ecs::WorldPos, ecs::ConstructionData>();
                            for (auto bEnt : bView) {
                                auto& bWp = bView.get<ecs::WorldPos>(bEnt);
                                auto& cData = bView.get<ecs::ConstructionData>(bEnt);
                                if (!cData.isBuilt || registry.all_of<ecs::CityStorageTag>(bEnt)) {
                                    float bMinX = bWp.wx - cData.resourceZoneWidth / 2.0f;
                                    float bMaxX = bWp.wx + cData.resourceZoneWidth / 2.0f;
                                    float bMinY = bWp.wy - cData.resourceZoneHeight / 2.0f;
                                    float bMaxY = bWp.wy + cData.resourceZoneHeight / 2.0f;
                                    if (wp.wx >= bMinX && wp.wx <= bMaxX && wp.wy >= bMinY && wp.wy <= bMaxY) {
                                        inside = false;
                                        break;
                                    }
                                }
                            }
                        }

                        if (inside) {
                            sf::Color solidColor(taskArea.color.r, taskArea.color.g, taskArea.color.b, 255);
                            registry.emplace<ecs::MarkedForHarvestTag>(resEnt, solidColor, taskEnt);
                        }
                    }
                }
            }

            bool hasResourcesLeft = false;
            bool hasGeneratorsLeft = false;
            bool courierDelivering = false;
            int activeWorkerCount = 0;

            for (auto wEnt : workerView) {
                auto& uData = workerView.get<ecs::UnitData>(wEnt);
                auto& wState = workerView.get<ecs::WorkerState>(wEnt);
                if (wState.currentTask == taskEnt) {
                    activeWorkerCount++;
                    if (uData.type == ecs::UnitType::Courier && uData.heldItem != ecs::HeldItem::None) {
                        courierDelivering = true;
                    }
                }
            }

            for (auto resEnt : registry.view<ecs::MarkedForHarvestTag>()) {
                if (registry.get<ecs::MarkedForHarvestTag>(resEnt).taskEntity == taskEnt) {
                    hasResourcesLeft = true;
                    if (registry.all_of<ecs::TreeTag>(resEnt) || registry.all_of<ecs::RockTag>(resEnt) || registry.all_of<ecs::BushTag>(resEnt)) {
                        hasGeneratorsLeft = true;
                    }
                }
            }

            if (registry.ctx().get<ecs::GameMode>() == ecs::GameMode::Player) {

                if (activeWorkerCount == 0) {
                    completedTasks.push_back(taskEnt);
                    continue;
                }
            }

            if (courierDelivering) continue;

            if (!hasResourcesLeft) {
                if (taskArea.collectFutureDrops) {
                    if (!hasGeneratorsLeft) {
                        completedTasks.push_back(taskEnt);
                    }
                } else {
                    completedTasks.push_back(taskEnt);
                }
            }
        }

        for (auto e : completedTasks) {
            if (registry.all_of<ecs::TaskArea>(e)) {
                auto& ta = registry.get<ecs::TaskArea>(e);
                core::SimLogger::get().log("[Task] Task #" + std::to_string(core::SimLogger::eid(e))
                    + " (id=" + std::to_string(ta.id) + ") completed");
            }
            std::vector<entt::entity> toUnmark;
            for (auto resEnt : registry.view<ecs::MarkedForHarvestTag>()) {
                if (registry.get<ecs::MarkedForHarvestTag>(resEnt).taskEntity == e) {
                    toUnmark.push_back(resEnt);
                }
            }

            for (auto resEnt : toUnmark) {
                registry.remove<ecs::MarkedForHarvestTag>(resEnt);
                registry.remove<ecs::ClaimedTag>(resEnt);
            }

            for (auto wEnt : registry.view<ecs::WorkerState>()) {
                auto& wState = registry.get<ecs::WorkerState>(wEnt);
                if (wState.currentTask == e) {
                    if (registry.all_of<ecs::WorkerTag>(wEnt)) registry.remove<ecs::WorkerTag>(wEnt);
                    wState.currentTask = entt::null;
                    if (registry.all_of<ecs::ActionTarget>(wEnt)) {
                        auto target = registry.get<ecs::ActionTarget>(wEnt).target;
                        registry.remove<ecs::ActionTarget>(wEnt);
                        if (registry.valid(target) && registry.all_of<ecs::ClaimedTag>(target))
                            registry.remove<ecs::ClaimedTag>(target);
                    }
                    if (registry.all_of<ecs::Path>(wEnt)) registry.remove<ecs::Path>(wEnt);
                    if (registry.all_of<ecs::PathRequest>(wEnt)) registry.remove<ecs::PathRequest>(wEnt);
                    if (registry.all_of<ecs::AnimationState>(wEnt)) {
                        auto& anim = registry.get<ecs::AnimationState>(wEnt);
                        if (anim.isActionLocked) {
                            anim.isActionLocked = false;
                            anim.currentAnim = 0;
                            anim.currentFrame = 0;
                        }
                    }
                }
            }

            registry.destroy(e);
        }
    }

}