#include "Simulation/Unit/UnitControlSystem.hpp"
#include "Simulation/Unit/UnitCombatSystem.hpp"
#include "Simulation/Unit/UnitMovementSystem.hpp"
#include "Simulation/Unit/UnitPhysicsSystem.hpp"
#include "Simulation/World/PathfindingSystem.hpp"
#include "Simulation/World/TaskSystem.hpp"
#include "Simulation/Unit/UnitTaskSystem.hpp"
#include "Simulation/Unit/UnitCommandSystem.hpp"
#include "Config/DeveloperConfig.hpp"
#include "Core/ThreadPool.hpp"
#include "Core/Profiler.hpp"
#include "Core/SimLogger.hpp"
#include "ECS/Components.hpp"
#include "ECS/Tags.hpp"
#include <SFML/Window/Keyboard.hpp>
#include <cmath>
#include <algorithm>
#include <memory>
#include <chrono>

namespace wr::simulation {

void processGlobalCommands(entt::registry& registry, bool pJustPressed, bool isCancelCommand);
void handleHeroInput(entt::registry& registry, entt::entity entity, ecs::UnitData& uData, ecs::WorkerState& wState, ecs::WorldPos& wp, ecs::AnimationState& anim, bool passSpaceJustPressed, bool isTaskMode, bool isBuilderMode, world::ChunkManager& chunkManager, DeferredQueues& localQueues);
void verifyPathObstacles(entt::registry& registry, entt::entity entity, ecs::WorldPos& wp, ecs::Path* path, ecs::ActionTarget* actionTarget, const EntitySpatialGrid& solidGrid, core::ThreadPool* threadPool, world::ChunkManager& chunkManager, std::shared_ptr<GlobalObstacleMap> globalObstacles, DeferredQueues& localQueues);
void evaluateJobTarget(entt::registry& registry, entt::entity entity, ecs::UnitData& uData, ecs::WorkerState& wState, ecs::WorldPos& wp, ecs::ActionTarget* actionTarget, ecs::Path* path, ecs::PathRequest* req, bool isPaused, core::ThreadPool* threadPool, world::ChunkManager& chunkManager, std::shared_ptr<GlobalObstacleMap> globalObstacles, DeferredQueues& localQueues, float dt);
void applyBuildingCollisions(entt::registry& registry, entt::entity entity, ecs::WorldPos& wp, const std::vector<entt::entity>& activeBuildings);

UnitControlSystem::UnitControlSystem(world::ChunkManager& chunkManager)
    : m_chunkManager(chunkManager) {}

void UnitControlSystem::update(entt::registry& registry, float dt, rendering::ViewDirection viewDir, const math::Vec2f& mouseWorldPos, const math::Vec2f& simulationCenterWorld, bool isRightClicking, bool isTaskMode) noexcept {

    bool isBuilderMode = registry.ctx().contains<ecs::BuilderModeState>() && registry.ctx().get<ecs::BuilderModeState>().active;

    math::Vec2f kbVector = UnitMovementSystem::getKeyboardWorldVector(viewDir);

    if (registry.ctx().get<ecs::GameMode>() == ecs::GameMode::Simulation) {
        kbVector = {0.0f, 0.0f};
    } else {
        if (isTaskMode || isBuilderMode) kbVector = {0.0f, 0.0f};
    }

    bool isSpacePressed = sf::Keyboard::isKeyPressed(sf::Keyboard::Space);
    bool spaceJustPressed = isSpacePressed && !m_prevSpacePressed;

    bool isPPressed = sf::Keyboard::isKeyPressed(sf::Keyboard::P);
    bool pJustPressed = isPPressed && !m_prevPPressed;

    bool isCtrlHeld = sf::Keyboard::isKeyPressed(sf::Keyboard::LControl) || sf::Keyboard::isKeyPressed(sf::Keyboard::RControl);
    bool isCancelCommand = isCtrlHeld && sf::Keyboard::isKeyPressed(sf::Keyboard::Q);

    core::GlobalCoord simCenterBlockX = static_cast<core::GlobalCoord>(std::floor(simulationCenterWorld.x));
    core::GlobalCoord simCenterBlockY = static_cast<core::GlobalCoord>(std::floor(simulationCenterWorld.y));
    math::Vec2i64 simCenterChunk = math::worldToChunk(simCenterBlockX, simCenterBlockY);

    FastChunkCache chunkCache;
    std::vector<entt::entity> activeBuildings;

    auto& uDataStorage = registry.storage<ecs::UnitData>();
    auto& wStateStorage = registry.storage<ecs::WorkerState>();
    auto& cStatsStorage = registry.storage<ecs::CombatStats>();
    auto& wpStorage = registry.storage<ecs::WorldPos>();
    auto& velStorage = registry.storage<ecs::Velocity>();
    auto& animStorage = registry.storage<ecs::AnimationState>();
    auto& lpStorage = registry.storage<ecs::LogicalPos>();
    auto& selectedStorage = registry.storage<ecs::SelectedTag>();
    auto& pausedStorage = registry.storage<ecs::PausedTag>();
    auto& workerStorage = registry.storage<ecs::WorkerTag>();
    auto& actionStorage = registry.storage<ecs::ActionTarget>();
    auto& pathStorage = registry.storage<ecs::Path>();
    auto& pathReqStorage = registry.storage<ecs::PathRequest>();

    auto globalObstacles = std::make_shared<GlobalObstacleMap>();
    auto* threadPool = registry.ctx().get<core::ThreadPool*>();

    {
        core::ScopedTimer preLoopTimer("1.1_UCS_PreLoop");

        chunkCache.build(m_chunkManager.getChunks(), simCenterChunk, config::SIMULATION_DISTANCE_CHUNKS);
        if (chunkCache.width > 0) {
            globalObstacles->init(chunkCache.minX * 64, chunkCache.minY * 64, chunkCache.width * 64, chunkCache.height * 64);
        }

        auto futObstacles = threadPool->enqueue([&]() {
            if (chunkCache.width == 0) return;
            auto solidViewHitbox = registry.view<ecs::SolidTag, ecs::WorldPos, ecs::Hitbox>();
            for (auto e : solidViewHitbox) {
                auto& wp = wpStorage.get(e);
                if (!chunkCache.containsWorld(wp.wx, wp.wy)) continue;
                float rad = registry.get<ecs::Hitbox>(e).radius / 64.0f;
                if (rad <= 0.4f) {
                    globalObstacles->set(static_cast<int64_t>(std::floor(wp.wx)), static_cast<int64_t>(std::floor(wp.wy)));
                } else {
                    float shrink = 0.1f;
                    int minX = static_cast<int>(std::floor(wp.wx - rad + shrink));
                    int maxX = static_cast<int>(std::floor(wp.wx + rad - shrink));
                    int minY = static_cast<int>(std::floor(wp.wy - rad + shrink));
                    int maxY = static_cast<int>(std::floor(wp.wy + rad - shrink));
                    for(int x = minX; x <= maxX; ++x) {
                        for(int y = minY; y <= maxY; ++y) globalObstacles->set(x, y);
                    }
                }
            }
            auto solidViewLogical = registry.view<ecs::SolidTag, ecs::LogicalPos>(entt::exclude<ecs::WorldPos>);
            for (auto e : solidViewLogical) {
                auto& pos = lpStorage.get(e);
                if (!chunkCache.containsBlock(pos.x, pos.y)) continue;
                globalObstacles->set(pos.x, pos.y);
            }
        });

        auto futUnitGrid = threadPool->enqueue([&]() { m_unitGrid.rebuild(registry, chunkCache, registry.view<ecs::UnitTag, ecs::WorldPos>()); });
        auto futSolidGrid = threadPool->enqueue([&]() { m_solidGrid.rebuild(registry, chunkCache, registry.view<ecs::SolidTag, ecs::WorldPos>()); });

        {
            core::ScopedTimer taskTimer("2.4_TaskSystem");
            TaskSystem::update(registry);
        }

        futObstacles.wait();

        processGlobalCommands(registry, pJustPressed, isCancelCommand);
        UnitCommandSystem::processCommands(registry, m_chunkManager, pJustPressed, isCancelCommand, isRightClicking, mouseWorldPos, globalObstacles);
        UnitTaskSystem::assignWorkerTargets(registry, m_chunkManager, globalObstacles);

        auto buildingView = registry.view<ecs::BuildingTag, ecs::WorldPos, ecs::SolidTag>();
        for (auto bEnt : buildingView) {
            const auto& bWp = buildingView.get<ecs::WorldPos>(bEnt);
            if (!chunkCache.containsWorld(bWp.wx, bWp.wy)) continue;
            activeBuildings.push_back(bEnt);
        }

        futUnitGrid.wait();
        futSolidGrid.wait();
    }

    auto activeView = registry.view<ecs::UnitTag>(entt::exclude<ecs::IdleTag>);
    int selectedCount = selectedStorage.size();
    m_globalQueues.reserve(activeView.size_hint());

    std::mutex timeMutex;
    double agg_CombatIntent = 0.0, agg_CombatTrig = 0.0, agg_CombatResolv = 0.0, agg_PhysGrid = 0.0, agg_PhysMath = 0.0, agg_PhysState = 0.0;

    size_t batchSize = 256;
    std::vector<std::future<void>> futures;

    auto it = activeView.begin();
    auto end = activeView.end();

    while (it != end) {
        auto chunkStart = it;
        size_t count = 0;
        while (it != end && count < batchSize) { ++it; ++count; }
        auto chunkEnd = it;

        futures.push_back(threadPool->enqueue([&, chunkStart, chunkEnd, count, isSpacePressed, spaceJustPressed, isRightClicking, kbVector, mouseWorldPos, isTaskMode, isBuilderMode, selectedCount, dt, globalObstacles]() {
            CombatTimers cTimers;
            PhysicsTimers pTimers;

            DeferredQueues localQueues;
            localQueues.reserve(count);

            for (auto localIt = chunkStart; localIt != chunkEnd; ++localIt) {
                entt::entity entity = *localIt;

                if (!uDataStorage.contains(entity) || !wStateStorage.contains(entity) || !wpStorage.contains(entity) || !velStorage.contains(entity) || !animStorage.contains(entity)) continue;

                auto& uData = uDataStorage.get(entity);
                auto& wState = wStateStorage.get(entity);
                ecs::CombatStats* cStats = cStatsStorage.contains(entity) ? &cStatsStorage.get(entity) : nullptr;
                auto& wp = wpStorage.get(entity);
                auto& vel = velStorage.get(entity);
                auto& anim = animStorage.get(entity);
                auto& lp = lpStorage.get(entity);

                if (!chunkCache.containsWorld(wp.wx, wp.wy)) continue;

                bool isSelected = selectedStorage.contains(entity);
                ecs::ActionTarget* actionTarget = actionStorage.contains(entity) ? &actionStorage.get(entity) : nullptr;
                ecs::Path* path = pathStorage.contains(entity) ? &pathStorage.get(entity) : nullptr;
                ecs::PathRequest* req = pathReqStorage.contains(entity) ? &pathReqStorage.get(entity) : nullptr;
                bool hasTask = workerStorage.contains(entity) || (wState.currentTask != entt::null);

                bool isZMoving = (wp.targetZ > -9000.0f) && (std::abs(wp.wz - wp.targetZ) > 0.1f);
                bool isActivelyMoving = (std::abs(vel.dx) > 0.001f || std::abs(vel.dy) > 0.001f || wp.zJumpVel != 0.0f || wp.zJump > 0.0f || isZMoving);

                // Stuck detection: push stationary units out of solid entities/buildings
                bool stuckInSolid = false;
                if (!isActivelyMoving) {
                    entt::entity stationaryStuckIn = entt::null;
                    float preX = wp.wx, preY = wp.wy;
                    m_solidGrid.forEachNearby({wp.wx, wp.wy}, 1.5f, [&](const SpatialGridData& data) {
                        if (data.entity == entity) return;
                        float sdx = wp.wx - data.x;
                        float sdy = wp.wy - data.y;
                        float distSq = sdx * sdx + sdy * sdy;
                        float minDist = 0.3f + data.radius;
                        if (distSq < minDist * minDist) {
                            stuckInSolid = true;
                            stationaryStuckIn = data.entity;
                            if (distSq > 0.001f) {
                                float dist = std::sqrt(distSq);
                                wp.wx += (sdx / dist) * (minDist - dist);
                                wp.wy += (sdy / dist) * (minDist - dist);
                            } else {
                                wp.wx += minDist;
                            }
                        }
                    });
                    if (stuckInSolid) {
                        std::string sType = "solid";
                        if (registry.valid(stationaryStuckIn)) {
                            if (registry.all_of<ecs::RockTag>(stationaryStuckIn)) sType = "Rock";
                            else if (registry.all_of<ecs::TreeTag>(stationaryStuckIn)) sType = "Tree";
                        }
                        core::SimLogger::get().log("[StuckDetect] " + std::string(core::SimLogger::typeName(uData.type))
                            + " #" + std::to_string(core::SimLogger::eid(entity))
                            + " STATIONARY inside " + sType + " #" + std::to_string(core::SimLogger::eid(stationaryStuckIn))
                            + " | pushed " + core::SimLogger::pos(preX, preY) + " -> " + core::SimLogger::pos(wp.wx, wp.wy));
                    }
                    if (!stuckInSolid) {
                        for (auto bEnt : activeBuildings) {
                            auto& bWp2 = registry.get<ecs::WorldPos>(bEnt);
                            float bRad = 0.4f;
                            if (auto* bhb = registry.try_get<ecs::Hitbox>(bEnt)) bRad = bhb->radius / 64.0f;
                            float bMinX = bWp2.wx - bRad, bMaxX = bWp2.wx + bRad;
                            float bMinY = bWp2.wy - bRad, bMaxY = bWp2.wy + bRad;
                            if (wp.wx > bMinX && wp.wx < bMaxX && wp.wy > bMinY && wp.wy < bMaxY) {
                                stuckInSolid = true;
                                float toL = wp.wx - bMinX, toR = bMaxX - wp.wx;
                                float toT = wp.wy - bMinY, toB = bMaxY - wp.wy;
                                float minPush = std::min({toL, toR, toT, toB});
                                if (minPush == toL)      wp.wx = bMinX - 0.3f;
                                else if (minPush == toR) wp.wx = bMaxX + 0.3f;
                                else if (minPush == toT) wp.wy = bMinY - 0.3f;
                                else                     wp.wy = bMaxY + 0.3f;
                                break;
                            }
                        }
                    }
                }

                bool isIdle = !isActivelyMoving && !stuckInSolid && !path && !req && !actionTarget && !isSelected && !anim.isActionLocked && !hasTask;

                if (isIdle) {
                    if (wr::config::ENABLE_UNIT_SLEEP_SYSTEM) localQueues.addIdleTag.push_back(entity);
                    continue;
                }

                math::Vec2f moveIntent{0.0f, 0.0f};
                bool isHero = isSelected && (selectedCount == 1);
                bool isPaused = pausedStorage.contains(entity);

                bool passSpacePressed = isHero ? isSpacePressed : false;
                bool passSpaceJustPressed = isHero ? spaceJustPressed : false;

                if (isHero) {
                    if (isPaused || !hasTask) moveIntent = kbVector;
                    handleHeroInput(registry, entity, uData, wState, wp, anim, passSpaceJustPressed, isTaskMode, isBuilderMode, m_chunkManager, localQueues);
                    if (!isPaused && hasTask) { passSpacePressed = false; passSpaceJustPressed = false; }
                }

                verifyPathObstacles(registry, entity, wp, path, actionTarget, m_solidGrid, threadPool, m_chunkManager, globalObstacles, localQueues);

                evaluateJobTarget(registry, entity, uData, wState, wp, actionTarget, path, req, isPaused, threadPool, m_chunkManager, globalObstacles, localQueues, dt);

                math::Vec2f combatMove = UnitCombatSystem::processCombat(registry, entity, uData, wState, cStats, wp, vel, anim, actionTarget, mouseWorldPos, isRightClicking, passSpacePressed, passSpaceJustPressed, isPaused, dt, m_chunkManager, localQueues, cTimers);
                if (moveIntent.x == 0 && moveIntent.y == 0) moveIntent = combatMove;

                UnitMovementSystem::applyMovement(entity, wp, vel, anim, wState, path, req, moveIntent, localQueues);

                UnitPhysicsSystem::applyPhysicsAndCollisions(registry, entity, wp, vel, lp, anim, dt, chunkCache, m_solidGrid, m_unitGrid, pTimers);

                applyBuildingCollisions(registry, entity, wp, activeBuildings);

                // Post-movement stuck detection: if unit ended up overlapping a
                // solid entity (path routed through it), push out and reroute.
                {
                    bool pushedOut = false;
                    entt::entity stuckInEntity = entt::null;
                    float stuckSolidX = 0, stuckSolidY = 0, stuckSolidRad = 0;
                    float preFixX = wp.wx, preFixY = wp.wy;
                    m_solidGrid.forEachNearby({wp.wx, wp.wy}, 1.5f, [&](const SpatialGridData& data) {
                        if (data.entity == entity) return;
                        if (actionTarget && data.entity == actionTarget->target) return;
                        float sdx = wp.wx - data.x;
                        float sdy = wp.wy - data.y;
                        float distSq = sdx * sdx + sdy * sdy;
                        float minDist = 0.3f + data.radius;
                        if (distSq < minDist * minDist) {
                            pushedOut = true;
                            stuckInEntity = data.entity;
                            stuckSolidX = data.x;
                            stuckSolidY = data.y;
                            stuckSolidRad = data.radius;
                            if (distSq > 0.001f) {
                                float dist = std::sqrt(distSq);
                                float push = minDist - dist + 0.05f;
                                wp.wx += (sdx / dist) * push;
                                wp.wy += (sdy / dist) * push;
                            } else {
                                wp.wx += minDist;
                            }
                        }
                    });
                    if (pushedOut) {
                        std::string solidType = "unknown";
                        if (registry.valid(stuckInEntity)) {
                            if (registry.all_of<ecs::RockTag>(stuckInEntity)) solidType = "Rock";
                            else if (registry.all_of<ecs::TreeTag>(stuckInEntity)) solidType = "Tree";
                            else if (registry.all_of<ecs::BuildingTag>(stuckInEntity)) solidType = "Building";
                        }
                        std::string targetStr = "none";
                        if (actionTarget && registry.valid(actionTarget->target)) {
                            if (registry.all_of<ecs::RockTag>(actionTarget->target)) targetStr = "Rock #" + std::to_string(core::SimLogger::eid(actionTarget->target));
                            else if (registry.all_of<ecs::TreeTag>(actionTarget->target)) targetStr = "Tree #" + std::to_string(core::SimLogger::eid(actionTarget->target));
                            else if (registry.all_of<ecs::TaskArea>(actionTarget->target)) targetStr = "Task #" + std::to_string(core::SimLogger::eid(actionTarget->target));
                            else if (registry.all_of<ecs::BuildingTag>(actionTarget->target)) targetStr = "Building #" + std::to_string(core::SimLogger::eid(actionTarget->target));
                            else targetStr = "Entity #" + std::to_string(core::SimLogger::eid(actionTarget->target));
                        }
                        core::SimLogger::get().log("[StuckDetect] " + std::string(core::SimLogger::typeName(uData.type))
                            + " #" + std::to_string(core::SimLogger::eid(entity))
                            + " OVERLAPPING " + solidType + " #" + std::to_string(core::SimLogger::eid(stuckInEntity))
                            + " at " + core::SimLogger::pos(stuckSolidX, stuckSolidY)
                            + " (solidRad=" + std::to_string(stuckSolidRad).substr(0, 4)
                            + ") | unitPos: " + core::SimLogger::pos(preFixX, preFixY)
                            + " -> " + core::SimLogger::pos(wp.wx, wp.wy)
                            + " | hasPath=" + std::string(path ? "yes" : "no")
                            + " hasReq=" + std::string(req ? "yes" : "no")
                            + " | target=" + targetStr
                            + " | pathFailures=" + std::to_string(wState.pathFailures)
                            + " directMoveTimer=" + std::to_string(wState.directMoveTimer).substr(0, 4));
                    }
                    if (pushedOut && (path || req)) {
                        localQueues.removePath.push_back(entity);
                        localQueues.removePathRequest.push_back(entity);
                        // Re-request path from the pushed-out position
                        if (actionTarget && registry.valid(actionTarget->target)) {
                            math::Vec2f tPos;
                            if (registry.all_of<ecs::WorldPos>(actionTarget->target)) {
                                auto& tw = registry.get<ecs::WorldPos>(actionTarget->target);
                                tPos = {tw.wx, tw.wy};
                            } else {
                                tPos = {wp.wx, wp.wy};
                            }
                            core::SimLogger::get().log("[StuckDetect] " + std::string(core::SimLogger::typeName(uData.type))
                                + " #" + std::to_string(core::SimLogger::eid(entity))
                                + " REROUTING from " + core::SimLogger::pos(wp.wx, wp.wy)
                                + " to target at " + core::SimLogger::pos(tPos.x, tPos.y));
                            auto pathFuture = threadPool->enqueue([startWp = math::Vec2f{wp.wx, wp.wy}, tPos, &chunkManager = m_chunkManager, globalObstacles]() {
                                return PathfindingSystem::findPath(startWp, tPos, chunkManager, *globalObstacles);
                            });
                            localQueues.setPathRequest.push_back({entity, pathFuture.share()});
                        } else {
                            core::SimLogger::get().log("[StuckDetect] " + std::string(core::SimLogger::typeName(uData.type))
                                + " #" + std::to_string(core::SimLogger::eid(entity))
                                + " pushed out but NO actionTarget to reroute to — clearing path");
                        }
                    }
                }
            }

            std::lock_guard<std::mutex> lock(m_deferredMutex);
            m_globalQueues.merge(localQueues);

            std::lock_guard<std::mutex> tlock(timeMutex);
            agg_CombatIntent += cTimers.intent; agg_CombatTrig += cTimers.triggers; agg_CombatResolv += cTimers.resolver;
            agg_PhysGrid += pTimers.gridQuery; agg_PhysMath += pTimers.collisionMath; agg_PhysState += pTimers.stateUpdate;
        }));
    }

    for (auto& fut : futures) fut.wait();
    futures.clear();

    for(auto e : m_globalQueues.removeActionTarget) if(registry.valid(e)) registry.remove<ecs::ActionTarget>(e);
    for(auto e : m_globalQueues.removePath) if(registry.valid(e) && registry.all_of<ecs::Path>(e)) registry.remove<ecs::Path>(e);
    for(auto e : m_globalQueues.removePathRequest) if(registry.valid(e) && registry.all_of<ecs::PathRequest>(e)) registry.remove<ecs::PathRequest>(e);
    for(auto e : m_globalQueues.removeWorkerTag) if(registry.valid(e) && registry.all_of<ecs::WorkerTag>(e)) registry.remove<ecs::WorkerTag>(e);
    for(auto e : m_globalQueues.removeClaimedTag) if(registry.valid(e) && registry.all_of<ecs::ClaimedTag>(e)) registry.remove<ecs::ClaimedTag>(e);
    for(auto e : m_globalQueues.destroyEntity) if(registry.valid(e)) registry.destroy(e);
    for(auto e : m_globalQueues.addSolidTag) if(registry.valid(e)) registry.emplace_or_replace<ecs::SolidTag>(e);
    for(auto e : m_globalQueues.addIdleTag) if(registry.valid(e)) registry.emplace_or_replace<ecs::IdleTag>(e);
    for(auto& p : m_globalQueues.setActionTarget) if(registry.valid(p.first)) registry.emplace_or_replace<ecs::ActionTarget>(p.first, p.second);
    for(auto& p : m_globalQueues.setPathRequest) if(registry.valid(p.first)) registry.emplace_or_replace<ecs::PathRequest>(p.first, p.second);
    for(auto& p : m_globalQueues.setPath) if(registry.valid(p.first)) registry.emplace_or_replace<ecs::Path>(p.first, p.second);
    for(auto& f : m_globalQueues.complex) f();

    m_globalQueues = DeferredQueues();
    m_prevSpacePressed = isSpacePressed;
    m_prevPPressed = isPPressed;
}

}