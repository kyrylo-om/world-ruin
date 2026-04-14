#include "../../../include/Simulation/Unit/UnitTaskSystem.hpp"
#include "../../../include/Simulation/Environment/ResourceSystem.hpp"
#include "Core/ThreadPool.hpp"
#include "ECS/Components.hpp"
#include "ECS/Tags.hpp"
#include "Rendering/TileHandler.hpp"
#include <algorithm>

namespace wr::simulation {

void UnitTaskSystem::releaseUnitAction(entt::registry& registry, entt::entity e) {
    registry.remove<ecs::WorkerTag>(e);
    registry.remove<ecs::Path>(e);
    registry.remove<ecs::PathRequest>(e);

    if (auto* action = registry.try_get<ecs::ActionTarget>(e)) {
        entt::entity targetRes = action->target;
        registry.remove<ecs::ActionTarget>(e);

        if (registry.valid(targetRes) && !registry.all_of<ecs::TaskArea>(targetRes) && !registry.all_of<ecs::BuildingTag>(targetRes)) {
            registry.remove<ecs::ClaimedTag>(targetRes);

            bool isInTaskArea = false;
            if (auto* rWp = registry.try_get<ecs::WorldPos>(targetRes)) {
                auto taskView = registry.view<ecs::TaskArea>();
                for (auto tEnt : taskView) {
                    auto& area = taskView.get<ecs::TaskArea>(tEnt);
                    for (const auto& rect : area.areas) {
                        float minX = std::min(rect.startWorld.x, rect.endWorld.x);
                        float maxX = std::max(rect.startWorld.x, rect.endWorld.x);
                        float minY = std::min(rect.startWorld.y, rect.endWorld.y);
                        float maxY = std::max(rect.startWorld.y, rect.endWorld.y);
                        if (rWp->wx >= minX && rWp->wx <= maxX && rWp->wy >= minY && rWp->wy <= maxY) {
                            isInTaskArea = true; break;
                        }
                    }
                    if (isInTaskArea) break;
                }
            }

            if (!isInTaskArea) {
                bool shared = false;
                auto seekers = registry.view<ecs::ActionTarget>();
                for (auto other : seekers) {
                    if (seekers.get<ecs::ActionTarget>(other).target == targetRes) { shared = true; break; }
                }
                if (!shared) registry.remove<ecs::MarkedForHarvestTag>(targetRes);
            }
        }
    }

    auto& anim = registry.get<ecs::AnimationState>(e);
    if (anim.isActionLocked) {
        anim.isActionLocked = false;
        anim.currentAnim = 0;
        anim.currentFrame = 0;
    }

    if (registry.all_of<ecs::WorkerState>(e)) {
        auto& wState = registry.get<ecs::WorkerState>(e);
        wState.currentTask = entt::null;
        wState.taskQueue.clear();
    }
}

void UnitTaskSystem::cancelSelectedTasks(entt::registry& registry) {
    auto selectedView = registry.view<ecs::SelectedTag, ecs::UnitData>();
    for (auto entity : selectedView) {
        releaseUnitAction(registry, entity);
    }
}

void UnitTaskSystem::assignWorkerTargets(entt::registry& registry, world::ChunkManager& chunkManager, std::shared_ptr<GlobalObstacleMap> obstacles) {
    auto canHarvest = [&](ecs::UnitType type, ecs::HeldItem heldItem, entt::entity targetRes) -> bool {
        if (type == ecs::UnitType::Builder) return false;
        if (registry.all_of<ecs::Health>(targetRes) && registry.get<ecs::Health>(targetRes).current <= 0) return false;
        if (registry.all_of<ecs::RockTag>(targetRes)) return type == ecs::UnitType::Miner;
        if (registry.all_of<ecs::TreeTag>(targetRes)) return type == ecs::UnitType::Lumberjack;
        if (registry.all_of<ecs::BushTag>(targetRes)) return type == ecs::UnitType::Lumberjack || type == ecs::UnitType::Warrior;
        if (registry.all_of<ecs::LogTag>(targetRes)) return type == ecs::UnitType::Courier && heldItem == ecs::HeldItem::None;
        if (registry.all_of<ecs::SmallRockTag>(targetRes)) return type == ecs::UnitType::Courier && heldItem == ecs::HeldItem::None;
        return true;
    };

    auto* threadPool = registry.ctx().get<core::ThreadPool*>();
    auto workerGroup = registry.view<ecs::WorkerTag, ecs::UnitData, ecs::WorkerState, ecs::WorldPos>(entt::exclude<ecs::PausedTag>);

    for (auto entity : workerGroup) {
        auto& uData = workerGroup.get<ecs::UnitData>(entity);
        auto& wState = workerGroup.get<ecs::WorkerState>(entity);
        auto& wp = workerGroup.get<ecs::WorldPos>(entity);

        bool isIdle = !registry.all_of<ecs::ActionTarget>(entity) && !registry.all_of<ecs::Path>(entity) && !registry.all_of<ecs::PathRequest>(entity);

        if (isIdle) {
            if (uData.type == ecs::UnitType::Builder) {
                continue;
            }

            if (uData.type == ecs::UnitType::Courier && uData.heldItem != ecs::HeldItem::None && registry.valid(wState.currentTask)) {
                if (registry.all_of<ecs::TaskArea>(wState.currentTask)) {
                    auto& task = registry.get<ecs::TaskArea>(wState.currentTask);

                    entt::entity chosenBuilding = entt::null;
                    math::Vec2f dropCenter;

                    for (auto bEnt : task.targetBuildings) {
                        if (registry.valid(bEnt) && registry.all_of<ecs::ConstructionData>(bEnt)) {
                            auto& cData = registry.get<ecs::ConstructionData>(bEnt);
                            if (cData.isBuilt) continue;

                            auto& bWp = registry.get<ecs::WorldPos>(bEnt);
                            float minX = bWp.wx - cData.resourceZoneWidth / 2.0f;
                            float maxX = bWp.wx + cData.resourceZoneWidth / 2.0f;
                            float minY = bWp.wy - cData.resourceZoneHeight / 2.0f;
                            float maxY = bWp.wy + cData.resourceZoneHeight / 2.0f;

                            int woodOnGround = 0;
                            int rockOnGround = 0;
                            auto resView = registry.view<ecs::WorldPos, ecs::ResourceTag>();
                            for (auto r : resView) {
                                auto& rWp = resView.get<ecs::WorldPos>(r);
                                if (rWp.wx >= minX && rWp.wx <= maxX && rWp.wy >= minY && rWp.wy <= maxY) {
                                    if (registry.all_of<ecs::LogTag>(r)) woodOnGround++;
                                    if (registry.all_of<ecs::SmallRockTag>(r)) rockOnGround++;
                                }
                            }

                            if (uData.heldItem == ecs::HeldItem::Wood && woodOnGround < cData.woodRequired) {
                                chosenBuilding = bEnt;
                                dropCenter = {bWp.wx, bWp.wy};
                                break;
                            }
                            if (uData.heldItem == ecs::HeldItem::Rock && rockOnGround < cData.rockRequired) {
                                chosenBuilding = bEnt;
                                dropCenter = {bWp.wx, bWp.wy};
                                break;
                            }
                        }
                    }

                    if (chosenBuilding != entt::null) {
                        registry.emplace_or_replace<ecs::ActionTarget>(entity, chosenBuilding);
                        auto pathFuture = threadPool->enqueue([startWp = math::Vec2f{wp.wx, wp.wy}, dropCenter, &chunkManager, obstacles]() {
                            return PathfindingSystem::findPath(startWp, dropCenter, chunkManager, *obstacles);
                        });
                        registry.emplace_or_replace<ecs::PathRequest>(entity, std::move(pathFuture));
                        registry.remove<ecs::IdleTag>(entity);
                    } else if (task.hasDropoff) {
                        registry.emplace_or_replace<ecs::ActionTarget>(entity, wState.currentTask);
                        dropCenter = {(task.dropoffStart.x + task.dropoffEnd.x)/2.f, (task.dropoffStart.y + task.dropoffEnd.y)/2.f};
                        auto pathFuture = threadPool->enqueue([startWp = math::Vec2f{wp.wx, wp.wy}, dropCenter, &chunkManager, obstacles]() {
                            return PathfindingSystem::findPath(startWp, dropCenter, chunkManager, *obstacles);
                        });
                        registry.emplace_or_replace<ecs::PathRequest>(entity, std::move(pathFuture));
                        registry.remove<ecs::IdleTag>(entity);
                    } else if (task.useCityStorage) {
                        auto storeView = registry.view<ecs::CityStorageTag, ecs::WorldPos>();
                        if (storeView.begin() != storeView.end()) {
                            auto sEnt = *storeView.begin();
                            auto& sWp = storeView.get<ecs::WorldPos>(sEnt);
                            registry.emplace_or_replace<ecs::ActionTarget>(entity, wState.currentTask);
                            auto pathFuture = threadPool->enqueue([startWp = math::Vec2f{wp.wx, wp.wy}, target = math::Vec2f{sWp.wx, sWp.wy}, &chunkManager, obstacles]() {
                                return PathfindingSystem::findPath(startWp, target, chunkManager, *obstacles);
                            });
                            registry.emplace_or_replace<ecs::PathRequest>(entity, std::move(pathFuture));
                            registry.remove<ecs::IdleTag>(entity);
                        }
                    }
                } else {
                    wState.currentTask = entt::null;
                }
            } else {
                float closestDist = 9999999.0f;
                entt::entity bestTarget = entt::null;

                auto markedResources = registry.view<ecs::MarkedForHarvestTag>(entt::exclude<ecs::ClaimedTag>);
                for (auto res : markedResources) {
                    if (!canHarvest(uData.type, uData.heldItem, res)) continue;

                    auto& marked = markedResources.get<ecs::MarkedForHarvestTag>(res);
                    if (wState.currentTask != entt::null && marked.taskEntity != wState.currentTask) continue;

                    auto& rWp = registry.get<ecs::WorldPos>(res);
                    float dx = rWp.wx - wp.wx;
                    float dy = rWp.wy - wp.wy;
                    float distSq = dx*dx + dy*dy;

                    if (distSq < closestDist) {
                        closestDist = distSq;
                        bestTarget = res;
                    }
                }

                if (bestTarget != entt::null) {
                    registry.emplace_or_replace<ecs::ActionTarget>(entity, bestTarget);
                    registry.emplace_or_replace<ecs::ClaimedTag>(bestTarget);

                    auto& marked = registry.get<ecs::MarkedForHarvestTag>(bestTarget);
                    wState.currentTask = marked.taskEntity;

                    auto& tWp = registry.get<ecs::WorldPos>(bestTarget);
                    auto pathFuture = threadPool->enqueue([startWp = math::Vec2f{wp.wx, wp.wy}, target = math::Vec2f{tWp.wx, tWp.wy}, &chunkManager, obstacles]() {
                        return PathfindingSystem::findPath(startWp, target, chunkManager, *obstacles);
                    });
                    registry.emplace_or_replace<ecs::PathRequest>(entity, std::move(pathFuture));
                    registry.remove<ecs::IdleTag>(entity);
                } else {
                    if (wState.currentTask != entt::null) {
                        if (!registry.valid(wState.currentTask)) {
                            registry.remove<ecs::WorkerTag>(entity);
                            wState.currentTask = entt::null;
                        } else if (registry.all_of<ecs::TaskArea>(wState.currentTask)) {
                            auto& task = registry.get<ecs::TaskArea>(wState.currentTask);
                            if (uData.type == ecs::UnitType::Courier && task.collectFutureDrops) {
                            } else {
                                registry.remove<ecs::WorkerTag>(entity);
                                wState.currentTask = entt::null;
                            }
                        } else {
                            registry.remove<ecs::WorkerTag>(entity);
                            wState.currentTask = entt::null;
                        }
                    } else {
                        registry.remove<ecs::WorkerTag>(entity);
                    }
                }
            }
        }
    }
}

}