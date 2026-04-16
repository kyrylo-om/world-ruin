#include "../../../include/Simulation/Unit/UnitTaskSystem.hpp"
#include "../../../include/Simulation/Environment/ResourceSystem.hpp"
#include "Core/ThreadPool.hpp"
#include "Core/SimLogger.hpp"
#include "ECS/Components.hpp"
#include "ECS/Tags.hpp"
#include "Rendering/TileHandler.hpp"
#include <algorithm>
#include <cmath>

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
    auto& simLog = core::SimLogger::get();

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

            // ─── Courier with item: deliver ───
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
                        simLog.log("[Courier] Courier #" + std::to_string(core::SimLogger::eid(entity))
                            + " delivering " + core::SimLogger::itemName(uData.heldItem)
                            + " to building #" + std::to_string(core::SimLogger::eid(chosenBuilding))
                            + " at " + core::SimLogger::pos(dropCenter.x, dropCenter.y));

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

                        simLog.log("[Courier] Courier #" + std::to_string(core::SimLogger::eid(entity))
                            + " delivering " + core::SimLogger::itemName(uData.heldItem) + " to dropoff zone");
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

                            simLog.log("[Courier] Courier #" + std::to_string(core::SimLogger::eid(entity))
                                + " delivering " + core::SimLogger::itemName(uData.heldItem) + " to City Storage");
                        }
                    }

                // ─── Building supply (simulation) — deliver to building directly ───
                } else if (registry.all_of<ecs::BuildingTag>(wState.currentTask)) {
                    auto& cData = registry.get<ecs::ConstructionData>(wState.currentTask);
                    auto& bWp = registry.get<ecs::WorldPos>(wState.currentTask);

                    if (!cData.isBuilt) {
                        simLog.log("[Supply] Courier #" + std::to_string(core::SimLogger::eid(entity))
                            + " delivering " + core::SimLogger::itemName(uData.heldItem)
                            + " to House #" + std::to_string(core::SimLogger::eid(wState.currentTask))
                            + " at " + core::SimLogger::pos(bWp.wx, bWp.wy));

                        registry.emplace_or_replace<ecs::ActionTarget>(entity, wState.currentTask);
                        // Target edge of resource zone facing the courier, not the building center (blocked tile)
                        float dx = wp.wx - bWp.wx;
                        float dy = wp.wy - bWp.wy;
                        float d = std::sqrt(dx * dx + dy * dy);
                        float rzHalf = cData.resourceZoneWidth / 2.0f - 0.3f;
                        math::Vec2f target;
                        if (d > 0.001f) {
                            target = {bWp.wx + (dx / d) * rzHalf, bWp.wy + (dy / d) * rzHalf};
                        } else {
                            target = {bWp.wx + rzHalf, bWp.wy};
                        }
                        auto pathFuture = threadPool->enqueue([startWp = math::Vec2f{wp.wx, wp.wy}, target, &chunkManager, obstacles]() {
                            return PathfindingSystem::findPath(startWp, target, chunkManager, *obstacles);
                        });
                        registry.emplace_or_replace<ecs::PathRequest>(entity, std::move(pathFuture));
                        registry.remove<ecs::IdleTag>(entity);
                    } else {
                        // Building finished while courier was carrying — drop at city storage
                        auto storeView = registry.view<ecs::CityStorageTag, ecs::WorldPos>();
                        if (storeView.begin() != storeView.end()) {
                            auto sEnt = *storeView.begin();
                            auto& sWp = storeView.get<ecs::WorldPos>(sEnt);
                            registry.emplace_or_replace<ecs::ActionTarget>(entity, sEnt);
                            math::Vec2f target{sWp.wx, sWp.wy};
                            auto pathFuture = threadPool->enqueue([startWp = math::Vec2f{wp.wx, wp.wy}, target, &chunkManager, obstacles]() {
                                return PathfindingSystem::findPath(startWp, target, chunkManager, *obstacles);
                            });
                            registry.emplace_or_replace<ecs::PathRequest>(entity, std::move(pathFuture));
                        }
                        simLog.log("[Supply] Courier #" + std::to_string(core::SimLogger::eid(entity))
                            + " — House #" + std::to_string(core::SimLogger::eid(wState.currentTask))
                            + " finished, redirecting " + core::SimLogger::itemName(uData.heldItem) + " to City Storage");
                        wState.currentTask = entt::null;
                        registry.remove<ecs::WorkerTag>(entity);
                    }

                } else {
                    wState.currentTask = entt::null;
                }

            // ─── No item: find work ───
            } else {

                // ─── Building supply (simulation) — fetch from city storage ───
                if (uData.type == ecs::UnitType::Courier && uData.heldItem == ecs::HeldItem::None &&
                    registry.valid(wState.currentTask) && registry.all_of<ecs::BuildingTag>(wState.currentTask)) {

                    auto& cData = registry.get<ecs::ConstructionData>(wState.currentTask);

                    if (cData.isBuilt) {
                        simLog.log("[Supply] Courier #" + std::to_string(core::SimLogger::eid(entity))
                            + " released — House #" + std::to_string(core::SimLogger::eid(wState.currentTask)) + " is complete");
                        registry.remove<ecs::WorkerTag>(entity);
                        wState.currentTask = entt::null;
                        continue;
                    }

                    // Count resources already in building zone
                    auto& bWp = registry.get<ecs::WorldPos>(wState.currentTask);
                    float bMinX = bWp.wx - cData.resourceZoneWidth / 2.0f;
                    float bMaxX = bWp.wx + cData.resourceZoneWidth / 2.0f;
                    float bMinY = bWp.wy - cData.resourceZoneHeight / 2.0f;
                    float bMaxY = bWp.wy + cData.resourceZoneHeight / 2.0f;

                    int woodInZone = 0, rockInZone = 0;
                    auto resViewB = registry.view<ecs::WorldPos, ecs::ResourceTag>();
                    for (auto r : resViewB) {
                        auto& rWp = resViewB.get<ecs::WorldPos>(r);
                        if (rWp.wx >= bMinX && rWp.wx <= bMaxX && rWp.wy >= bMinY && rWp.wy <= bMaxY) {
                            if (registry.all_of<ecs::LogTag>(r)) woodInZone++;
                            if (registry.all_of<ecs::SmallRockTag>(r)) rockInZone++;
                        }
                    }

                    int netWood = std::max(0, cData.woodRequired - woodInZone);
                    int netRock = std::max(0, cData.rockRequired - rockInZone);

                    if (netWood <= 0 && netRock <= 0) {
                        // Building fully supplied — wait for builder to consume, don't release yet
                        continue;
                    }

                    // Find resource in city storage
                    auto storeView = registry.view<ecs::CityStorageTag, ecs::WorldPos, ecs::ConstructionData>();
                    if (storeView.begin() == storeView.end()) continue;

                    auto sEnt = *storeView.begin();
                    auto& sWp = storeView.get<ecs::WorldPos>(sEnt);
                    auto& sCData = storeView.get<ecs::ConstructionData>(sEnt);
                    float sMinX = sWp.wx - sCData.resourceZoneWidth / 2.0f;
                    float sMaxX = sWp.wx + sCData.resourceZoneWidth / 2.0f;
                    float sMinY = sWp.wy - sCData.resourceZoneHeight / 2.0f;
                    float sMaxY = sWp.wy + sCData.resourceZoneHeight / 2.0f;

                    entt::entity bestTarget = entt::null;
                    float bestDist = 9999999.0f;

                    // Prefer wood if needed, then rock
                    if (netWood > 0) {
                        auto logView = registry.view<ecs::LogTag, ecs::WorldPos>(entt::exclude<ecs::ClaimedTag>);
                        for (auto l : logView) {
                            auto& lWp = logView.get<ecs::WorldPos>(l);
                            if (lWp.wx >= sMinX && lWp.wx <= sMaxX && lWp.wy >= sMinY && lWp.wy <= sMaxY) {
                                float dx = lWp.wx - wp.wx;
                                float dy = lWp.wy - wp.wy;
                                float d = dx * dx + dy * dy;
                                if (d < bestDist) { bestDist = d; bestTarget = l; }
                            }
                        }
                    }

                    if (bestTarget == entt::null && netRock > 0) {
                        auto srView = registry.view<ecs::SmallRockTag, ecs::WorldPos>(entt::exclude<ecs::ClaimedTag>);
                        for (auto r : srView) {
                            auto& rWp = srView.get<ecs::WorldPos>(r);
                            if (rWp.wx >= sMinX && rWp.wx <= sMaxX && rWp.wy >= sMinY && rWp.wy <= sMaxY) {
                                float dx = rWp.wx - wp.wx;
                                float dy = rWp.wy - wp.wy;
                                float d = dx * dx + dy * dy;
                                if (d < bestDist) { bestDist = d; bestTarget = r; }
                            }
                        }
                    }

                    if (bestTarget != entt::null) {
                        auto& tWp = registry.get<ecs::WorldPos>(bestTarget);
                        std::string resType = registry.all_of<ecs::LogTag>(bestTarget) ? "Log" : "SmallRock";
                        simLog.log("[Supply] Courier #" + std::to_string(core::SimLogger::eid(entity))
                            + " picking up " + resType + " #" + std::to_string(core::SimLogger::eid(bestTarget))
                            + " at " + core::SimLogger::pos(tWp.wx, tWp.wy)
                            + " from City Storage for House #" + std::to_string(core::SimLogger::eid(wState.currentTask))
                            + " — needs " + std::to_string(netWood) + " wood, " + std::to_string(netRock) + " rock");

                        registry.emplace_or_replace<ecs::ActionTarget>(entity, bestTarget);
                        registry.emplace_or_replace<ecs::ClaimedTag>(bestTarget);
                        auto pathFuture = threadPool->enqueue([startWp = math::Vec2f{wp.wx, wp.wy}, target = math::Vec2f{tWp.wx, tWp.wy}, &chunkManager, obstacles]() {
                            return PathfindingSystem::findPath(startWp, target, chunkManager, *obstacles);
                        });
                        registry.emplace_or_replace<ecs::PathRequest>(entity, std::move(pathFuture));
                        registry.remove<ecs::IdleTag>(entity);
                    }
                    // If no resource in storage, just wait — don't release
                    continue;
                }

                // ─── Normal resource search via MarkedForHarvestTag ───
                float closestDist = 9999999.0f;
                entt::entity bestTarget = entt::null;

                auto markedResources = registry.view<ecs::MarkedForHarvestTag>(entt::exclude<ecs::ClaimedTag>);
                for (auto res : markedResources) {
                    if (res == wState.lastFailedTarget) continue;
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

                    std::string resType = "resource";
                    if (registry.all_of<ecs::TreeTag>(bestTarget)) resType = "Tree";
                    else if (registry.all_of<ecs::RockTag>(bestTarget)) resType = "Rock";
                    else if (registry.all_of<ecs::LogTag>(bestTarget)) resType = "Log";
                    else if (registry.all_of<ecs::SmallRockTag>(bestTarget)) resType = "SmallRock";
                    else if (registry.all_of<ecs::BushTag>(bestTarget)) resType = "Bush";

                    simLog.log("[Worker] " + std::string(core::SimLogger::typeName(uData.type))
                        + " #" + std::to_string(core::SimLogger::eid(entity))
                        + " claimed " + resType + " #" + std::to_string(core::SimLogger::eid(bestTarget))
                        + " at " + core::SimLogger::pos(tWp.wx, tWp.wy)
                        + " for Task #" + std::to_string(core::SimLogger::eid(wState.currentTask)));

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
                                // Before releasing, scan for another active task with compatible resources
                                entt::entity newTask = entt::null;
                                auto allTasks = registry.view<ecs::TaskArea>();
                                for (auto tEnt : allTasks) {
                                    if (tEnt == wState.currentTask) continue;
                                    auto candidateMarked = registry.view<ecs::MarkedForHarvestTag>(entt::exclude<ecs::ClaimedTag>);
                                    for (auto res : candidateMarked) {
                                        auto& mTag = candidateMarked.get<ecs::MarkedForHarvestTag>(res);
                                        if (mTag.taskEntity != tEnt) continue;
                                        if (canHarvest(uData.type, uData.heldItem, res)) {
                                            newTask = tEnt;
                                            break;
                                        }
                                    }
                                    if (newTask != entt::null) break;
                                }

                                if (newTask != entt::null) {
                                    entt::entity oldTask = wState.currentTask;
                                    wState.currentTask = newTask;
                                    simLog.log("[Worker] " + std::string(core::SimLogger::typeName(uData.type))
                                        + " #" + std::to_string(core::SimLogger::eid(entity))
                                        + " switched from Task #" + std::to_string(core::SimLogger::eid(oldTask))
                                        + " to Task #" + std::to_string(core::SimLogger::eid(newTask))
                                        + " — compatible resources found");
                                } else {
                                    simLog.log("[Worker] " + std::string(core::SimLogger::typeName(uData.type))
                                        + " #" + std::to_string(core::SimLogger::eid(entity))
                                        + " released from Task #" + std::to_string(core::SimLogger::eid(wState.currentTask))
                                        + " — no resources left");
                                    registry.remove<ecs::WorkerTag>(entity);
                                    wState.currentTask = entt::null;
                                }
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
