#include "Simulation/Unit/UnitControlSystem.hpp"
#include "Simulation/World/PathfindingSystem.hpp"
#include "ECS/Components.hpp"
#include "ECS/Tags.hpp"
#include "Core/SimLogger.hpp"
#include <cmath>
#include <algorithm>

namespace wr::simulation {

void verifyPathObstacles(entt::registry& registry, entt::entity entity, ecs::WorldPos& wp, ecs::Path* path, ecs::ActionTarget* actionTarget, const EntitySpatialGrid& solidGrid, core::ThreadPool* threadPool, world::ChunkManager& chunkManager, std::shared_ptr<GlobalObstacleMap> globalObstacles, DeferredQueues& localQueues) {
    if (!path) return;
    if (path->currentIndex >= path->waypoints.size()) return;

    auto& nextWp = path->waypoints[path->currentIndex];
    int nx = static_cast<int>(std::floor(nextWp.x));
    int ny = static_cast<int>(std::floor(nextWp.y));
    bool pathBlocked = false;

    auto checkBlock = [&](const SpatialGridData& data) {
        if (pathBlocked || data.entity == entity || (actionTarget && data.entity == actionTarget->target)) return;
        float rad = data.radius;
        if (rad <= 0.4f) {
            int sx = static_cast<int>(std::floor(data.x));
            int sy = static_cast<int>(std::floor(data.y));
            if (nx == sx && ny == sy) pathBlocked = true;
        } else {
            float shrink = 0.1f;
            int minX = static_cast<int>(std::floor(data.x - rad + shrink));
            int maxX = static_cast<int>(std::floor(data.x + rad - shrink));
            int minY = static_cast<int>(std::floor(data.y - rad + shrink));
            int maxY = static_cast<int>(std::floor(data.y + rad - shrink));
            if (nx >= minX && nx <= maxX && ny >= minY && ny <= maxY) {
                pathBlocked = true;
            }
        }
    };

    solidGrid.forEachNearby({nextWp.x, nextWp.y}, 1.5f, checkBlock);

    if (pathBlocked) {
        math::Vec2f dest = path->waypoints.back();
        auto pathFuture = threadPool->enqueue([startWp = math::Vec2f{wp.wx, wp.wy}, dest, &chunkManager, globalObstacles]() {
            return PathfindingSystem::findPath(startWp, dest, chunkManager, *globalObstacles);
        });
        localQueues.setPathRequest.push_back({entity, pathFuture.share()});
        localQueues.removePath.push_back(entity);
    }
}

void evaluateJobTarget(entt::registry& registry, entt::entity entity, ecs::UnitData& uData, ecs::WorkerState& wState, ecs::WorldPos& wp, ecs::ActionTarget* actionTarget, ecs::Path* path, ecs::PathRequest* req, bool isPaused, core::ThreadPool* threadPool, world::ChunkManager& chunkManager, std::shared_ptr<GlobalObstacleMap> globalObstacles, DeferredQueues& localQueues, float dt) {

    if (isPaused || req || path) return;

    if ((!actionTarget || !registry.valid(actionTarget->target)) && wState.currentTask != entt::null) {

        if (registry.valid(wState.currentTask)) {
            if (uData.type == ecs::UnitType::Courier && uData.heldItem != ecs::HeldItem::None) {

                localQueues.setActionTarget.push_back({entity, wState.currentTask});
                return;
            }
            else if (uData.type == ecs::UnitType::Builder) {
                localQueues.setActionTarget.push_back({entity, wState.currentTask});
                return;
            }
            else {
                entt::entity bestRes = entt::null;
                float bestDistSq = 9999999.0f;

                auto markedView = registry.view<ecs::MarkedForHarvestTag, ecs::WorldPos>(entt::exclude<ecs::ClaimedTag>);
                for (auto r : markedView) {
                    if (markedView.get<ecs::MarkedForHarvestTag>(r).taskEntity == wState.currentTask) {

                        bool compatible = false;
                        if (uData.type == ecs::UnitType::Courier) {
                            if (registry.all_of<ecs::LogTag>(r) || registry.all_of<ecs::SmallRockTag>(r)) compatible = true;
                        } else if (uData.type == ecs::UnitType::Lumberjack) {
                            if (registry.all_of<ecs::TreeTag>(r) || registry.all_of<ecs::BushTag>(r)) compatible = true;
                        } else if (uData.type == ecs::UnitType::Miner) {
                            if (registry.all_of<ecs::RockTag>(r)) compatible = true;
                        } else if (uData.type == ecs::UnitType::Warrior) {
                            if (registry.all_of<ecs::BushTag>(r)) compatible = true;
                        }

                        if (compatible) {
                            // Skip resource we just failed to reach
                            if (r == wState.lastFailedTarget) continue;
                            auto& rWp = markedView.get<ecs::WorldPos>(r);
                            float dSq = (wp.wx - rWp.wx)*(wp.wx - rWp.wx) + (wp.wy - rWp.wy)*(wp.wy - rWp.wy);
                            if (dSq < bestDistSq) {
                                bestDistSq = dSq;
                                bestRes = r;
                            }
                        }
                    }
                }

                if (bestRes != entt::null) {
                    wState.pathFailures = 0;
                    wState.directMoveTimer = 0.0f;
                    wState.lastFailedTarget = entt::null;
                    localQueues.setActionTarget.push_back({entity, bestRes});
                    localQueues.complex.push_back([&registry, bestRes]() {
                        if (registry.valid(bestRes) && !registry.all_of<ecs::ClaimedTag>(bestRes)) {
                            registry.emplace<ecs::ClaimedTag>(bestRes);
                        }
                    });
                    return;
                } else {
                    return;
                }
            }
        } else {
            localQueues.complex.push_back([&registry, entity]() {
                if (registry.valid(entity)) {
                    auto& ws = registry.get<ecs::WorkerState>(entity);
                    ws.currentTask = entt::null;
                    registry.remove<ecs::WorkerTag>(entity);
                }
            });
            return;
        }
    }

    if (!actionTarget || !registry.valid(actionTarget->target)) return;

    bool shouldMoveToTarget = true;

    if (registry.all_of<ecs::BuildingTag>(actionTarget->target) && uData.type == ecs::UnitType::Builder) {
        auto& cData = registry.get<ecs::ConstructionData>(actionTarget->target);

        if (!cData.isBuilt) {
            float progressIncrement = 0.3f;
            float nextProgress = cData.buildProgress + progressIncrement;
            float progressRatio = std::min(nextProgress / cData.maxTime, 1.0f);

            int expectedWood = static_cast<int>(cData.initialWood * progressRatio);
            int expectedRock = static_cast<int>(cData.initialRock * progressRatio);

            bool needsWood = (cData.initialWood - cData.woodRequired) < expectedWood;
            bool needsRock = (cData.initialRock - cData.rockRequired) < expectedRock;
            bool canConsume = true;

            auto& bWp = registry.get<ecs::WorldPos>(actionTarget->target);
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

            if (needsWood && woodOnGround == 0) canConsume = false;
            if (needsRock && rockOnGround == 0) canConsume = false;

            if (!canConsume) {
                shouldMoveToTarget = false;
            }
        }
    }

    if (shouldMoveToTarget) {
        math::Vec2f tPos;
        if (registry.all_of<ecs::TaskArea>(actionTarget->target)) {
            auto& t = registry.get<ecs::TaskArea>(actionTarget->target);
            if (t.hasDropoff) {
                tPos = {(t.dropoffStart.x + t.dropoffEnd.x)/2.f, (t.dropoffStart.y + t.dropoffEnd.y)/2.f};
            } else {
                auto storeView = registry.view<ecs::CityStorageTag, ecs::WorldPos>();
                if (storeView.begin() != storeView.end()) {
                    auto sEnt = *storeView.begin();
                    auto& sWp = storeView.get<ecs::WorldPos>(sEnt);
                    tPos = {sWp.wx, sWp.wy};
                } else {
                    tPos = {wp.wx, wp.wy};
                }
            }
        } else if (registry.all_of<ecs::BuildingTag>(actionTarget->target)) {
            auto& w = registry.get<ecs::WorldPos>(actionTarget->target);
            if (uData.type == ecs::UnitType::Builder) tPos = {w.wx + wState.workOffset.x, w.wy + wState.workOffset.y};
            else tPos = {w.wx, w.wy};
        } else {
            auto& w = registry.get<ecs::WorldPos>(actionTarget->target);
            tPos = {w.wx, w.wy};
        }

        // Detect unreachable targets: if unit keeps re-pathfinding to a
        // target without reaching attack range, give up after 3 attempts
        // so it picks a different target instead of circling forever.
        // Applies to ALL unit types targeting entities with Health.
        if (registry.all_of<ecs::Health>(actionTarget->target)) {
            float tDx = tPos.x - wp.wx;
            float tDy = tPos.y - wp.wy;
            float tDistSq = tDx * tDx + tDy * tDy;
            // Attack range ~1.0 blocks — if still outside that after a path,
            // pathfinding couldn't get us close enough (partial path).
            if (tDistSq > 1.2f) {
                wState.pathFailures++;
                std::string tgtType = "unknown";
                if (registry.all_of<ecs::RockTag>(actionTarget->target)) tgtType = "Rock";
                else if (registry.all_of<ecs::TreeTag>(actionTarget->target)) tgtType = "Tree";
                else if (registry.all_of<ecs::LogTag>(actionTarget->target)) tgtType = "Log";
                else if (registry.all_of<ecs::SmallRockTag>(actionTarget->target)) tgtType = "SmallRock";
                else if (registry.all_of<ecs::BushTag>(actionTarget->target)) tgtType = "Bush";
                else if (registry.all_of<ecs::BuildingTag>(actionTarget->target)) tgtType = "Building";
                core::SimLogger::get().log("[StuckDetect] " + std::string(core::SimLogger::typeName(uData.type))
                    + " #" + std::to_string(core::SimLogger::eid(entity))
                    + " pathFailure " + std::to_string(wState.pathFailures) + "/3"
                    + " for " + tgtType + " #" + std::to_string(core::SimLogger::eid(actionTarget->target))
                    + " | dist=" + std::to_string(std::sqrt(tDistSq)).substr(0, 4)
                    + " | unitPos=" + core::SimLogger::pos(wp.wx, wp.wy)
                    + " targetPos=" + core::SimLogger::pos(tPos.x, tPos.y)
                    + " | directMoveTimer=" + std::to_string(wState.directMoveTimer).substr(0, 4));
                if (wState.pathFailures >= 3) {
                    core::SimLogger::get().log("[StuckDetect] " + std::string(core::SimLogger::typeName(uData.type))
                        + " #" + std::to_string(core::SimLogger::eid(entity))
                        + " ABANDONING " + tgtType + " #" + std::to_string(core::SimLogger::eid(actionTarget->target))
                        + " — unreachable after 3 path failures");
                    wState.lastFailedTarget = actionTarget->target;
                    wState.pathFailures = 0;
                    wState.directMoveTimer = 0.0f;
                    entt::entity tgt = actionTarget->target;
                    localQueues.removeActionTarget.push_back(entity);
                    localQueues.complex.push_back([&registry, tgt]() {
                        if (registry.valid(tgt) && registry.all_of<ecs::ClaimedTag>(tgt))
                            registry.remove<ecs::ClaimedTag>(tgt);
                    });
                    return;
                }
            } else {
                if (wState.pathFailures > 0) {
                    core::SimLogger::get().log("[StuckDetect] " + std::string(core::SimLogger::typeName(uData.type))
                        + " #" + std::to_string(core::SimLogger::eid(entity))
                        + " pathFailures RESET (within range, dist=" + std::to_string(std::sqrt(tDistSq)).substr(0, 4) + ")");
                }
                wState.pathFailures = 0;
            }
        }

        auto pathFuture = threadPool->enqueue([startWp = math::Vec2f{wp.wx, wp.wy}, tPos, &chunkManager, globalObstacles]() {
            return PathfindingSystem::findPath(startWp, tPos, chunkManager, *globalObstacles);
        });
        localQueues.setPathRequest.push_back({entity, pathFuture.share()});
    }
}

}