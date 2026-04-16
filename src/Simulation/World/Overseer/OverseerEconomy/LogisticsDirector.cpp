#include "Simulation/World/OverseerSystem.hpp"
#include "Core/SimLogger.hpp"
#include "ECS/Tags.hpp"
#include <algorithm>
#include <cmath>
#include <vector>

namespace wr::simulation {

void OverseerSystem::manageLogistics(entt::registry& registry, world::ChunkManager& chunkManager) {
    float currentTime = m_internalClock.getElapsedTime().asSeconds();
    if (currentTime - m_lastLogisticsTime < 1.5f) return;
    m_lastLogisticsTime = currentTime;

    auto& simLog = core::SimLogger::get();
    auto& board = registry.ctx().get<SimBlackboard>();
    auto unitView = registry.view<ecs::UnitData, ecs::WorkerState>();
    auto taskView = registry.view<ecs::TaskArea>();

    // ═══════════════════════════════════════════════════════════════
    //  STEP 1: Profile every task — workers, drops, generators, need
    // ═══════════════════════════════════════════════════════════════

    struct TaskProfile {
        entt::entity entity;
        int harvesters{0};
        int couriers{0};
        int uncollectedDrops{0};
        bool hasGenerators{false};
        bool isSupplyTask{false};
        bool collectFutureDrops{false};
        int couriersNeeded{0};
        float importance{0.0f};
        int totalDrops{0};
        bool hasTrees{false};
        bool hasRocks{false};
        bool hasBushes{false};
        math::Vec2f center{0, 0};
    };

    std::vector<TaskProfile> taskProfiles;

    for (auto tEnt : taskView) {
        auto& ta = taskView.get<ecs::TaskArea>(tEnt);
        TaskProfile tp{tEnt};
        tp.collectFutureDrops = ta.collectFutureDrops;
        tp.isSupplyTask = !ta.targetBuildings.empty();
        if (!ta.areas.empty()) {
            tp.center = {
                (ta.areas[0].startWorld.x + ta.areas[0].endWorld.x) / 2.0f,
                (ta.areas[0].startWorld.y + ta.areas[0].endWorld.y) / 2.0f
            };
        }
        taskProfiles.push_back(tp);
    }

    // Count workers per task
    for (auto w : unitView) {
        auto& wState = unitView.get<ecs::WorkerState>(w);
        if (wState.currentTask == entt::null) continue;
        auto& uData = unitView.get<ecs::UnitData>(w);
        for (auto& tp : taskProfiles) {
            if (tp.entity == wState.currentTask) {
                if (uData.type == ecs::UnitType::Courier) tp.couriers++;
                else tp.harvesters++;
                break;
            }
        }
    }

    // Count drops, generators, and resource types per task
    auto markedView = registry.view<ecs::MarkedForHarvestTag>();
    for (auto m : markedView) {
        auto& mTag = markedView.get<ecs::MarkedForHarvestTag>(m);
        for (auto& tp : taskProfiles) {
            if (tp.entity != mTag.taskEntity) continue;

            bool isDrop = registry.all_of<ecs::LogTag>(m) || registry.all_of<ecs::SmallRockTag>(m);
            if (isDrop) {
                tp.totalDrops++;
                if (!registry.all_of<ecs::ClaimedTag>(m))
                    tp.uncollectedDrops++;
            }

            if (registry.all_of<ecs::TreeTag>(m))  { tp.hasGenerators = true; tp.hasTrees = true; }
            if (registry.all_of<ecs::RockTag>(m))   { tp.hasGenerators = true; tp.hasRocks = true; }
            if (registry.all_of<ecs::BushTag>(m))   { tp.hasGenerators = true; tp.hasBushes = true; }
            break;
        }
    }

    // Calculate how many couriers each task needs and its importance
    for (auto& tp : taskProfiles) {
        if (tp.isSupplyTask) {
            tp.couriersNeeded = 1;
            tp.importance = 100.0f;
        } else if (tp.harvesters > 0) {
            tp.couriersNeeded = std::max(1, static_cast<int>(std::ceil(tp.harvesters * 0.5f)));
            tp.importance = tp.harvesters * 2.0f + tp.uncollectedDrops * 1.0f;
        } else if (tp.uncollectedDrops > 0) {
            tp.couriersNeeded = 1;
            tp.importance = tp.uncollectedDrops * 0.5f;
        } else if (tp.collectFutureDrops && tp.hasGenerators) {
            tp.couriersNeeded = 1;
            tp.importance = 1.0f;
        } else {
            tp.couriersNeeded = 0;
            tp.importance = 0.0f;
        }

        // Guarantee at least 1 courier for tasks with ANY drops still on the ground
        // (even if they're all claimed — a courier is en route to collect them)
        if (tp.totalDrops > 0 && tp.couriersNeeded < 1) {
            tp.couriersNeeded = 1;
            tp.importance = std::max(tp.importance, tp.totalDrops * 0.5f);
        }
    }

    // --- Detailed Task Profile Dump ---
    simLog.log("[Logistics] ========== CYCLE START (t=" + std::to_string(currentTime) + "s) ==========");
    simLog.log("[Logistics] Economy: availWood=" + std::to_string(board.availWood)
        + " availRock=" + std::to_string(board.availRock)
        + " demandWood=" + std::to_string(board.demandWood)
        + " demandRock=" + std::to_string(board.demandRock)
        + " pendingBuildings=" + std::to_string(board.pendingBuildings)
        + " totalHouses=" + std::to_string(board.totalHouses));
    simLog.log("[Logistics] Population: W=" + std::to_string(board.popWarrior)
        + " L=" + std::to_string(board.popLumberjack)
        + " M=" + std::to_string(board.popMiner)
        + " C=" + std::to_string(board.popCourier)
        + " B=" + std::to_string(board.popBuilder));
    simLog.log("[Logistics] Task profiles (" + std::to_string(taskProfiles.size()) + " tasks):");
    for (auto& tp : taskProfiles) {
        std::string resTypes;
        if (tp.hasTrees) resTypes += "trees ";
        if (tp.hasRocks) resTypes += "rocks ";
        if (tp.hasBushes) resTypes += "bushes ";
        if (resTypes.empty()) resTypes = "none";
        simLog.log("[Logistics]   Task #" + std::to_string(core::SimLogger::eid(tp.entity))
            + " @ " + core::SimLogger::pos(tp.center.x, tp.center.y)
            + " | harvesters=" + std::to_string(tp.harvesters)
            + " couriers=" + std::to_string(tp.couriers)
            + " couriersNeeded=" + std::to_string(tp.couriersNeeded)
            + " importance=" + std::to_string(static_cast<int>(tp.importance))
            + " | drops: total=" + std::to_string(tp.totalDrops)
            + " uncollected=" + std::to_string(tp.uncollectedDrops)
            + " | generators=" + std::string(tp.hasGenerators ? "yes" : "no")
            + " futureDrops=" + std::string(tp.collectFutureDrops ? "yes" : "no")
            + " supply=" + std::string(tp.isSupplyTask ? "yes" : "no")
            + " | res=[" + resTypes + "]");
    }

    // ═══════════════════════════════════════════════════════════════
    //  STEP 2: Profile every courier — idle, stealable, or locked
    // ═══════════════════════════════════════════════════════════════

    struct CourierProfile {
        entt::entity entity;
        entt::entity task;
        bool isCarrying;
        float stealCost;    // -1 = idle (free), 0+ = cost to steal, huge = locked
        math::Vec2f pos;
        bool matched{false};
    };

    std::vector<CourierProfile> courierProfiles;

    for (auto w : unitView) {
        auto& uData = unitView.get<ecs::UnitData>(w);
        if (uData.type != ecs::UnitType::Courier) continue;
        auto& wState = unitView.get<ecs::WorkerState>(w);
        auto& wp = registry.get<ecs::WorldPos>(w);
        bool isPathing = registry.all_of<ecs::PathRequest>(w) || registry.all_of<ecs::Path>(w);

        CourierProfile cp;
        cp.entity = w;
        cp.task = wState.currentTask;
        cp.isCarrying = uData.heldItem != ecs::HeldItem::None;
        cp.pos = {wp.wx, wp.wy};

        // Determine steal cost
        bool isIdle = !registry.all_of<ecs::WorkerTag>(w) &&
                      wState.currentTask == entt::null && !isPathing;

        if (isIdle) {
            cp.stealCost = -1.0f;
        } else if (cp.isCarrying) {
            cp.stealCost = 9999999.0f;
        } else if (cp.task != entt::null && registry.valid(cp.task) && registry.all_of<ecs::BuildingTag>(cp.task)) {
            // Courier is on building supply duty — don't release or steal
            cp.stealCost = 100.0f;
        } else {
            // Look up this courier's task profile
            float taskImportance = 0.0f;
            int taskCouriers = 0;
            int taskNeeded = 0;
            for (auto& tp : taskProfiles) {
                if (tp.entity == cp.task) {
                    taskImportance = tp.importance;
                    taskCouriers = tp.couriers;
                    taskNeeded = tp.couriersNeeded;
                    break;
                }
            }

            if (taskNeeded == 0) {
                cp.stealCost = 0.0f;    // Task doesn't need any courier
            } else if (taskCouriers > taskNeeded) {
                cp.stealCost = taskImportance * 0.1f; // Over-staffed: cheap to steal
            } else {
                cp.stealCost = taskImportance;  // Under/exact: expensive to steal
            }
        }

        courierProfiles.push_back(cp);
    }

    // --- Detailed Courier Profile Dump ---
    simLog.log("[Logistics] Courier profiles (" + std::to_string(courierProfiles.size()) + " couriers):");
    for (auto& cp : courierProfiles) {
        std::string taskStr;
        if (cp.task == entt::null) {
            taskStr = "none";
        } else if (registry.valid(cp.task) && registry.all_of<ecs::BuildingTag>(cp.task)) {
            taskStr = "Building #" + std::to_string(core::SimLogger::eid(cp.task));
        } else {
            taskStr = "Task #" + std::to_string(core::SimLogger::eid(cp.task));
        }
        std::string stealStr;
        if (cp.stealCost < 0.0f) stealStr = "IDLE";
        else if (cp.stealCost >= 9999999.0f) stealStr = "LOCKED(carrying)";
        else if (cp.stealCost >= 100.0f) stealStr = "BUILDING(" + std::to_string(static_cast<int>(cp.stealCost)) + ")";
        else stealStr = std::to_string(static_cast<int>(cp.stealCost));
        std::string itemStr;
        if (cp.isCarrying) itemStr = "carrying";
        else itemStr = "empty";
        simLog.log("[Logistics]   Courier #" + std::to_string(core::SimLogger::eid(cp.entity))
            + " @ " + core::SimLogger::pos(cp.pos.x, cp.pos.y)
            + " | task=" + taskStr
            + " | steal=" + stealStr
            + " | " + itemStr);
    }

    // ═══════════════════════════════════════════════════════════════
    //  PHASE 0: Proactive release — free couriers from dead-end tasks
    // ═══════════════════════════════════════════════════════════════
    // A courier on a task that needs 0 couriers is doing nothing useful.
    // Release it immediately so it becomes available for assignment.

    for (auto& cp : courierProfiles) {
        if (cp.stealCost != 0.0f || cp.isCarrying) continue;
        if (cp.task == entt::null || !registry.valid(cp.task)) continue;

        auto& wState = registry.get<ecs::WorkerState>(cp.entity);
        wState.currentTask = entt::null;
        if (registry.all_of<ecs::WorkerTag>(cp.entity))
            registry.remove<ecs::WorkerTag>(cp.entity);
        if (auto* at = registry.try_get<ecs::ActionTarget>(cp.entity)) {
            entt::entity tgt = at->target;
            registry.remove<ecs::ActionTarget>(cp.entity);
            if (registry.valid(tgt) && registry.all_of<ecs::ClaimedTag>(tgt))
                registry.remove<ecs::ClaimedTag>(tgt);
        }
        if (registry.all_of<ecs::Path>(cp.entity))
            registry.remove<ecs::Path>(cp.entity);
        if (registry.all_of<ecs::PathRequest>(cp.entity))
            registry.remove<ecs::PathRequest>(cp.entity);

        for (auto& tp : taskProfiles) {
            if (tp.entity == cp.task) { tp.couriers--; break; }
        }

        simLog.log("[Logistics] Courier #" + std::to_string(core::SimLogger::eid(cp.entity))
            + " released from dead-end task (no generators, no drops)");

        cp.task = entt::null;
        cp.stealCost = -1.0f; // Now idle
    }

    // ═══════════════════════════════════════════════════════════════
    //  PHASE 1: Release excess couriers from over-staffed tasks
    // ═══════════════════════════════════════════════════════════════
    // Only release if other tasks actually need couriers (totalDeficit > 0).
    // If no task is short on couriers, keep extras where they are — don't
    // let couriers go idle when their current task has work to do.

    // Pre-check: do any unbuilt buildings lack courier supply?
    bool buildingsUndersupplied = false;
    if (board.pendingBuildings > 0) {
        auto bCheckView = registry.view<ecs::BuildingTag, ecs::ConstructionData>(entt::exclude<ecs::CityStorageTag>);
        for (auto bEnt : bCheckView) {
            if (bCheckView.get<ecs::ConstructionData>(bEnt).isBuilt) continue;
            bool hasCourier = false;
            for (auto& cp : courierProfiles) {
                if (cp.task == bEnt) { hasCourier = true; break; }
            }
            if (!hasCourier) { buildingsUndersupplied = true; break; }
        }
    }

    simLog.log("[Logistics] Phase 1: buildingsUndersupplied=" + std::string(buildingsUndersupplied ? "YES" : "no"));

    int totalDeficit = 0;
    for (auto& tp2 : taskProfiles) {
        int def = tp2.couriersNeeded - tp2.couriers;
        if (def > 0) totalDeficit += def;
    }

    simLog.log("[Logistics] Phase 1: totalDeficit=" + std::to_string(totalDeficit));

    for (auto& tp : taskProfiles) {
        // Never drop below 1 courier if task still has drops or active generators
        int minKeep = (tp.totalDrops > 0 || tp.hasGenerators) ? 1 : 0;
        int excess = tp.couriers - std::max(tp.couriersNeeded, minKeep);
        if (excess <= 0) continue;

        // If no other task needs couriers and buildings are already supplied, keep them
        if (totalDeficit <= 0 && !buildingsUndersupplied &&
            (tp.totalDrops > 0 || tp.hasGenerators || tp.harvesters > 0)) {
            simLog.log("[Logistics] Phase 1: Task #" + std::to_string(core::SimLogger::eid(tp.entity))
                + " has " + std::to_string(excess) + " excess but keeping (no deficit, buildings ok, task active)");
            continue;
        }

        for (auto& cp : courierProfiles) {
            if (excess <= 0) break;
            if (cp.task != tp.entity || cp.isCarrying || cp.stealCost < 0.0f) continue;

            auto& wState = registry.get<ecs::WorkerState>(cp.entity);
            wState.currentTask = entt::null;
            if (registry.all_of<ecs::WorkerTag>(cp.entity))
                registry.remove<ecs::WorkerTag>(cp.entity);
            if (auto* at = registry.try_get<ecs::ActionTarget>(cp.entity)) {
                entt::entity tgt = at->target;
                registry.remove<ecs::ActionTarget>(cp.entity);
                if (registry.valid(tgt) && registry.all_of<ecs::ClaimedTag>(tgt))
                    registry.remove<ecs::ClaimedTag>(tgt);
            }
            if (registry.all_of<ecs::Path>(cp.entity))
                registry.remove<ecs::Path>(cp.entity);
            if (registry.all_of<ecs::PathRequest>(cp.entity))
                registry.remove<ecs::PathRequest>(cp.entity);

            simLog.log("[Logistics] Courier #" + std::to_string(core::SimLogger::eid(cp.entity))
                + " released — excess on task (has " + std::to_string(tp.couriers) + "/" + std::to_string(tp.couriersNeeded) + " couriers)");

            tp.couriers--;
            cp.task = entt::null;
            cp.stealCost = -1.0f;
            excess--;
        }
    }

    // ═══════════════════════════════════════════════════════════════
    //  PHASE 1.5: Release excess couriers from over-supplied buildings
    // ═══════════════════════════════════════════════════════════════
    simLog.log("[Logistics] Phase 1.5: checking building courier excess");

    {
        auto buildExView = registry.view<ecs::BuildingTag, ecs::WorldPos, ecs::ConstructionData>(entt::exclude<ecs::CityStorageTag>);
        for (auto bEnt : buildExView) {
            auto& bCData = buildExView.get<ecs::ConstructionData>(bEnt);
            if (bCData.isBuilt) continue;

            auto& bWp = buildExView.get<ecs::WorldPos>(bEnt);
            float bMinX = bWp.wx - bCData.resourceZoneWidth / 2.0f;
            float bMaxX = bWp.wx + bCData.resourceZoneWidth / 2.0f;
            float bMinY = bWp.wy - bCData.resourceZoneHeight / 2.0f;
            float bMaxY = bWp.wy + bCData.resourceZoneHeight / 2.0f;

            int woodInZone = 0, rockInZone = 0;
            auto logResView = registry.view<ecs::LogTag, ecs::WorldPos>();
            for (auto l : logResView) {
                auto& lWp = logResView.get<ecs::WorldPos>(l);
                if (lWp.wx >= bMinX && lWp.wx <= bMaxX && lWp.wy >= bMinY && lWp.wy <= bMaxY) woodInZone++;
            }
            auto srResView = registry.view<ecs::SmallRockTag, ecs::WorldPos>();
            for (auto r : srResView) {
                auto& rWp = srResView.get<ecs::WorldPos>(r);
                if (rWp.wx >= bMinX && rWp.wx <= bMaxX && rWp.wy >= bMinY && rWp.wy <= bMaxY) rockInZone++;
            }

            int netWood = std::max(0, bCData.woodRequired - woodInZone);
            int netRock = std::max(0, bCData.rockRequired - rockInZone);
            int desired = (netWood + netRock > 0) ? std::min(2, std::max(1, (netWood + netRock) / 8)) : 0;

            int assigned = 0;
            for (auto& cp : courierProfiles) {
                if (cp.task == bEnt) assigned++;
            }

            simLog.log("[Logistics] Phase 1.5: Building #" + std::to_string(core::SimLogger::eid(bEnt))
                + " needW=" + std::to_string(bCData.woodRequired) + " needR=" + std::to_string(bCData.rockRequired)
                + " inZoneW=" + std::to_string(woodInZone) + " inZoneR=" + std::to_string(rockInZone)
                + " netW=" + std::to_string(netWood) + " netR=" + std::to_string(netRock)
                + " desired=" + std::to_string(desired) + " assigned=" + std::to_string(assigned));

            int bExcess = assigned - desired;
            for (auto& cp : courierProfiles) {
                if (bExcess <= 0) break;
                if (cp.task != bEnt || cp.isCarrying) continue;

                auto& wState = registry.get<ecs::WorkerState>(cp.entity);
                wState.currentTask = entt::null;
                if (registry.all_of<ecs::WorkerTag>(cp.entity))
                    registry.remove<ecs::WorkerTag>(cp.entity);
                if (auto* at = registry.try_get<ecs::ActionTarget>(cp.entity)) {
                    entt::entity tgt = at->target;
                    registry.remove<ecs::ActionTarget>(cp.entity);
                    if (registry.valid(tgt) && registry.all_of<ecs::ClaimedTag>(tgt))
                        registry.remove<ecs::ClaimedTag>(tgt);
                }
                if (registry.all_of<ecs::Path>(cp.entity))
                    registry.remove<ecs::Path>(cp.entity);
                if (registry.all_of<ecs::PathRequest>(cp.entity))
                    registry.remove<ecs::PathRequest>(cp.entity);

                simLog.log("[Logistics] Courier #" + std::to_string(core::SimLogger::eid(cp.entity))
                    + " released — excess on House #" + std::to_string(core::SimLogger::eid(bEnt))
                    + " (" + std::to_string(assigned) + "/" + std::to_string(desired) + " couriers)");

                cp.task = entt::null;
                cp.stealCost = -1.0f;
                bExcess--;
                assigned--;
            }
        }
    }

    // ═══════════════════════════════════════════════════════════════
    //  PHASE 2: Priority-based courier assignment
    // ═══════════════════════════════════════════════════════════════
    // Build demand list from tasks that need more couriers.
    // Fill demands in priority order: idle couriers first, then steal
    // from lowest-cost assignments when demand priority > steal cost.

    struct CourierDemand {
        entt::entity task;
        float priority;
        math::Vec2f pos;
    };

    std::vector<CourierDemand> demands;
    for (auto& tp : taskProfiles) {
        int deficit = tp.couriersNeeded - tp.couriers;
        for (int i = 0; i < deficit; i++) {
            demands.push_back({tp.entity, tp.importance, tp.center});
        }
    }

    std::sort(demands.begin(), demands.end(),
              [](const CourierDemand& a, const CourierDemand& b) { return a.priority > b.priority; });

    simLog.log("[Logistics] Phase 2: " + std::to_string(demands.size()) + " courier demands:");
    for (auto& d : demands) {
        simLog.log("[Logistics]   demand: Task #" + std::to_string(core::SimLogger::eid(d.task))
            + " priority=" + std::to_string(static_cast<int>(d.priority))
            + " @ " + core::SimLogger::pos(d.pos.x, d.pos.y));
    }

    auto reassignCourier = [&](CourierProfile& cp, entt::entity newTask) {
        // Clean up old state — unclaim any targeted resource
        if (auto* at = registry.try_get<ecs::ActionTarget>(cp.entity)) {
            entt::entity tgt = at->target;
            registry.remove<ecs::ActionTarget>(cp.entity);
            if (registry.valid(tgt) && registry.all_of<ecs::ClaimedTag>(tgt))
                registry.remove<ecs::ClaimedTag>(tgt);
        }
        if (registry.all_of<ecs::Path>(cp.entity))
            registry.remove<ecs::Path>(cp.entity);
        if (registry.all_of<ecs::PathRequest>(cp.entity))
            registry.remove<ecs::PathRequest>(cp.entity);

        // Update task courier counts
        for (auto& tp : taskProfiles) {
            if (tp.entity == cp.task) { tp.couriers--; break; }
        }
        for (auto& tp : taskProfiles) {
            if (tp.entity == newTask) { tp.couriers++; break; }
        }

        // Assign
        registry.get<ecs::WorkerState>(cp.entity).currentTask = newTask;
        registry.emplace_or_replace<ecs::WorkerTag>(cp.entity);
        if (registry.all_of<ecs::IdleTag>(cp.entity))
            registry.remove<ecs::IdleTag>(cp.entity);
        cp.task = newTask;
        cp.matched = true;
    };

    for (auto& demand : demands) {
        // Try idle couriers first (closest)
        int bestIdx = -1;
        float bestDist = 9999999.0f;
        for (int i = 0; i < static_cast<int>(courierProfiles.size()); i++) {
            auto& cp = courierProfiles[i];
            if (cp.matched || cp.stealCost >= 0.0f) continue; // Not idle
            float dx = cp.pos.x - demand.pos.x;
            float dy = cp.pos.y - demand.pos.y;
            float d = dx * dx + dy * dy;
            if (d < bestDist) { bestDist = d; bestIdx = i; }
        }

        if (bestIdx >= 0) {
            simLog.log("[Logistics] Courier #" + std::to_string(core::SimLogger::eid(courierProfiles[bestIdx].entity))
                + " (idle) assigned to task #" + std::to_string(core::SimLogger::eid(demand.task))
                + " (priority=" + std::to_string(static_cast<int>(demand.priority)) + ")");
            reassignCourier(courierProfiles[bestIdx], demand.task);
            continue;
        }

        // Try stealing: find courier with lowest stealCost < demand priority
        // Never steal the last courier from a task with drops or generators
        int bestStealIdx = -1;
        float lowestCost = demand.priority;
        for (int i = 0; i < static_cast<int>(courierProfiles.size()); i++) {
            auto& cp = courierProfiles[i];
            if (cp.matched || cp.stealCost < 0.0f) continue; // Already used or idle (handled above)
            if (cp.isCarrying) continue;
            if (cp.task == demand.task) continue;

            // Protect the last courier on tasks that still need one
            bool sourceProtected = false;
            for (auto& stp : taskProfiles) {
                if (stp.entity == cp.task) {
                    int minKeep = (stp.totalDrops > 0 || stp.hasGenerators) ? 1 : 0;
                    if (stp.couriers <= std::max(stp.couriersNeeded, minKeep))
                        sourceProtected = true;
                    break;
                }
            }
            if (sourceProtected) continue;

            if (cp.stealCost < lowestCost) {
                lowestCost = cp.stealCost;
                bestStealIdx = i;
            }
        }

        if (bestStealIdx >= 0) {
            simLog.log("[Logistics] Courier #" + std::to_string(core::SimLogger::eid(courierProfiles[bestStealIdx].entity))
                + " STOLEN from task #" + std::to_string(core::SimLogger::eid(courierProfiles[bestStealIdx].task))
                + " (stealCost=" + std::to_string(static_cast<int>(courierProfiles[bestStealIdx].stealCost))
                + ") -> task #" + std::to_string(core::SimLogger::eid(demand.task))
                + " (priority=" + std::to_string(static_cast<int>(demand.priority)) + ")");
            reassignCourier(courierProfiles[bestStealIdx], demand.task);
        } else if (bestIdx < 0) {
            simLog.log("[Logistics] Phase 2: UNMET demand for Task #" + std::to_string(core::SimLogger::eid(demand.task))
                + " (priority=" + std::to_string(static_cast<int>(demand.priority)) + ") — no idle or stealable courier");
        }
    }

    // ═══════════════════════════════════════════════════════════════
    //  PHASE 3: Assign couriers to buildings under construction
    // ═══════════════════════════════════════════════════════════════
    // HIGHEST PRIORITY — runs before overflow. Will steal couriers
    // from harvest tasks if no idle ones exist. Always keeps at least
    // 1 courier on tasks that still have active resources (trees/rocks).

    auto storageView = registry.view<ecs::CityStorageTag, ecs::WorldPos, ecs::ConstructionData>();
    bool hasStorage = (storageView.begin() != storageView.end());

    simLog.log("[Logistics] Phase 3: hasStorage=" + std::string(hasStorage ? "yes" : "no"));

    if (hasStorage) {
        auto findCourierForBuilding = [&]() -> entt::entity {
            // 1. Idle couriers from profiles
            for (auto& cp : courierProfiles) {
                if (cp.stealCost < 0.0f && !cp.matched && !cp.isCarrying)
                    return cp.entity;
            }
            // 2. Idle couriers missed by profiles (fresh spawns, etc.)
            for (auto w : unitView) {
                auto& uData = unitView.get<ecs::UnitData>(w);
                if (uData.type != ecs::UnitType::Courier) continue;
                if (registry.all_of<ecs::WorkerTag>(w)) continue;
                auto& wState = unitView.get<ecs::WorkerState>(w);
                if (wState.currentTask != entt::null) continue;
                if (registry.all_of<ecs::PathRequest>(w) || registry.all_of<ecs::Path>(w)) continue;
                return w;
            }
            // 3. Steal from tasks — lowest cost, keep min 1 on active tasks
            int bestIdx = -1;
            float lowestCost = 9999999.0f;
            for (int i = 0; i < static_cast<int>(courierProfiles.size()); i++) {
                auto& cp = courierProfiles[i];
                if (cp.matched || cp.isCarrying) continue;
                if (cp.stealCost >= 9999999.0f) continue;
                if (cp.task == entt::null || !registry.valid(cp.task)) continue;
                if (registry.all_of<ecs::BuildingTag>(cp.task)) continue;

                bool canSteal = false;
                for (auto& tp : taskProfiles) {
                    if (tp.entity == cp.task) {
                        int minKeep = (tp.hasGenerators || tp.totalDrops > 0) ? 1 : 0;
                        canSteal = (tp.couriers > minKeep);
                        break;
                    }
                }
                if (!canSteal) continue;

                if (cp.stealCost < lowestCost) {
                    lowestCost = cp.stealCost;
                    bestIdx = i;
                }
            }
            if (bestIdx >= 0) {
                auto& cp = courierProfiles[bestIdx];
                if (auto* at = registry.try_get<ecs::ActionTarget>(cp.entity)) {
                    entt::entity tgt = at->target;
                    registry.remove<ecs::ActionTarget>(cp.entity);
                    if (registry.valid(tgt) && registry.all_of<ecs::ClaimedTag>(tgt))
                        registry.remove<ecs::ClaimedTag>(tgt);
                }
                if (registry.all_of<ecs::Path>(cp.entity))
                    registry.remove<ecs::Path>(cp.entity);
                if (registry.all_of<ecs::PathRequest>(cp.entity))
                    registry.remove<ecs::PathRequest>(cp.entity);

                for (auto& tp : taskProfiles) {
                    if (tp.entity == cp.task) { tp.couriers--; break; }
                }

                simLog.log("[Logistics] Courier #" + std::to_string(core::SimLogger::eid(cp.entity))
                    + " STOLEN from task #" + std::to_string(core::SimLogger::eid(cp.task))
                    + " for building supply");

                cp.task = entt::null;
                cp.stealCost = -1.0f;
                return cp.entity;
            }
            return entt::null;
        };

        auto buildingView = registry.view<ecs::BuildingTag, ecs::WorldPos, ecs::ConstructionData>(entt::exclude<ecs::CityStorageTag>);

        for (auto bEnt : buildingView) {
            auto& bCData = buildingView.get<ecs::ConstructionData>(bEnt);
            if (bCData.isBuilt) continue;

            bool hasBuilder = false;
            for (auto w : unitView) {
                auto& uData = unitView.get<ecs::UnitData>(w);
                auto& wState = unitView.get<ecs::WorkerState>(w);
                if (uData.type == ecs::UnitType::Builder && wState.currentTask == bEnt) {
                    hasBuilder = true;
                    break;
                }
            }
            if (!hasBuilder) continue;

            auto& bWp = buildingView.get<ecs::WorldPos>(bEnt);
            float bMinX = bWp.wx - bCData.resourceZoneWidth / 2.0f;
            float bMaxX = bWp.wx + bCData.resourceZoneWidth / 2.0f;
            float bMinY = bWp.wy - bCData.resourceZoneHeight / 2.0f;
            float bMaxY = bWp.wy + bCData.resourceZoneHeight / 2.0f;

            int woodInZone = 0, rockInZone = 0;
            auto logView = registry.view<ecs::LogTag, ecs::WorldPos>();
            for (auto l : logView) {
                auto& lWp = logView.get<ecs::WorldPos>(l);
                if (lWp.wx >= bMinX && lWp.wx <= bMaxX && lWp.wy >= bMinY && lWp.wy <= bMaxY) woodInZone++;
            }
            auto srView = registry.view<ecs::SmallRockTag, ecs::WorldPos>();
            for (auto r : srView) {
                auto& rWp = srView.get<ecs::WorldPos>(r);
                if (rWp.wx >= bMinX && rWp.wx <= bMaxX && rWp.wy >= bMinY && rWp.wy <= bMaxY) rockInZone++;
            }

            int netWood = std::max(0, bCData.woodRequired - woodInZone);
            int netRock = std::max(0, bCData.rockRequired - rockInZone);

            simLog.log("[Logistics] Phase 3: Building #" + std::to_string(core::SimLogger::eid(bEnt))
                + " @ " + core::SimLogger::pos(bWp.wx, bWp.wy)
                + " needW=" + std::to_string(bCData.woodRequired) + " needR=" + std::to_string(bCData.rockRequired)
                + " inZoneW=" + std::to_string(woodInZone) + " inZoneR=" + std::to_string(rockInZone)
                + " netW=" + std::to_string(netWood) + " netR=" + std::to_string(netRock)
                + " hasBuilder=yes");

            if (netWood <= 0 && netRock <= 0) {
                simLog.log("[Logistics] Phase 3: Building #" + std::to_string(core::SimLogger::eid(bEnt))
                    + " fully supplied — skipping");
                continue;
            }

            int assignedCouriers = 0;
            for (auto w : unitView) {
                auto& uData = unitView.get<ecs::UnitData>(w);
                auto& wState = unitView.get<ecs::WorkerState>(w);
                if (uData.type == ecs::UnitType::Courier && wState.currentTask == bEnt)
                    assignedCouriers++;
            }

            // Cap at 2 couriers per building — enough to keep supply flowing
            // without starving harvest tasks of their couriers
            int couriersWanted = std::min(2, std::max(1, (netWood + netRock) / 8)) - assignedCouriers;

            simLog.log("[Logistics] Phase 3: Building #" + std::to_string(core::SimLogger::eid(bEnt))
                + " assignedCouriers=" + std::to_string(assignedCouriers)
                + " couriersWanted=" + std::to_string(couriersWanted));

            for (int i = 0; i < couriersWanted; i++) {
                entt::entity courier = findCourierForBuilding();
                if (courier == entt::null) {
                    simLog.log("[Logistics] Phase 3: Building #" + std::to_string(core::SimLogger::eid(bEnt))
                        + " — no courier available for slot " + std::to_string(i + 1));
                    break;
                }

                registry.get<ecs::WorkerState>(courier).currentTask = bEnt;
                registry.emplace_or_replace<ecs::WorkerTag>(courier);
                if (registry.all_of<ecs::IdleTag>(courier))
                    registry.remove<ecs::IdleTag>(courier);

                // Mark in courierProfiles so overflow doesn't reassign
                for (auto& cp : courierProfiles) {
                    if (cp.entity == courier) {
                        cp.matched = true;
                        cp.task = bEnt;
                        break;
                    }
                }

                simLog.log("[Logistics] Courier #" + std::to_string(core::SimLogger::eid(courier))
                    + " assigned to supply House #" + std::to_string(core::SimLogger::eid(bEnt))
                    + " at " + core::SimLogger::pos(bWp.wx, bWp.wy)
                    + " — needs " + std::to_string(netWood) + "W " + std::to_string(netRock) + "R"
                    + " (" + std::to_string(assignedCouriers + i + 1) + " couriers total)");
            }
        }
    }

    // ═══════════════════════════════════════════════════════════════
    //  PHASE 4: Overflow — assign remaining idle couriers to busiest task
    // ═══════════════════════════════════════════════════════════════
    // After buildings are satisfied, if couriers are still idle and
    // there's any task with work, assign them there.

    {
        int idleLeft = 0;
        for (auto& cp : courierProfiles) {
            if (!cp.matched && cp.stealCost < 0.0f && !cp.isCarrying) idleLeft++;
        }
        simLog.log("[Logistics] Phase 4: " + std::to_string(idleLeft) + " idle couriers remaining for overflow");
    }

    for (auto& cp : courierProfiles) {
        if (cp.matched || cp.stealCost >= 0.0f) continue;
        if (cp.isCarrying) continue;

        entt::entity bestTask = entt::null;
        float bestImportance = 0.0f;
        for (auto& tp : taskProfiles) {
            if (tp.importance <= 0.0f) continue;
            // Don't overflow to tasks already at or above their courier need
            if (tp.couriers >= tp.couriersNeeded) continue;
            if (tp.importance > bestImportance) {
                bestImportance = tp.importance;
                bestTask = tp.entity;
            }
        }

        if (bestTask != entt::null) {
            simLog.log("[Logistics] Courier #" + std::to_string(core::SimLogger::eid(cp.entity))
                + " (idle, overflow) assigned to task #" + std::to_string(core::SimLogger::eid(bestTask))
                + " (importance=" + std::to_string(static_cast<int>(bestImportance)) + ")");
            reassignCourier(cp, bestTask);
        }
    }

    // ═══════════════════════════════════════════════════════════════
    //  PHASE 5: Idle harvester reassignment — join compatible tasks
    // ═══════════════════════════════════════════════════════════════
    simLog.log("[Logistics] Phase 5: scanning for idle harvesters");

    for (auto w : unitView) {
        auto& uData = unitView.get<ecs::UnitData>(w);
        if (uData.type == ecs::UnitType::Courier || uData.type == ecs::UnitType::Builder) continue;

        auto& wState = unitView.get<ecs::WorkerState>(w);
        if (registry.all_of<ecs::WorkerTag>(w)) continue;
        if (wState.currentTask != entt::null) continue;
        if (registry.all_of<ecs::PathRequest>(w) || registry.all_of<ecs::Path>(w)) continue;

        entt::entity bestTask = entt::null;
        int bestCount = 0;

        for (auto& tp : taskProfiles) {
            bool compatible = false;
            if (uData.type == ecs::UnitType::Lumberjack) compatible = tp.hasTrees || tp.hasBushes;
            else if (uData.type == ecs::UnitType::Miner)  compatible = tp.hasRocks;
            else if (uData.type == ecs::UnitType::Warrior) compatible = tp.hasBushes;
            if (!compatible) continue;

            int count = 0;
            for (auto m : markedView) {
                auto& mTag = markedView.get<ecs::MarkedForHarvestTag>(m);
                if (mTag.taskEntity != tp.entity) continue;
                if (registry.all_of<ecs::ClaimedTag>(m)) continue;

                bool match = false;
                if (uData.type == ecs::UnitType::Lumberjack)
                    match = registry.all_of<ecs::TreeTag>(m) || registry.all_of<ecs::BushTag>(m);
                else if (uData.type == ecs::UnitType::Miner)
                    match = registry.all_of<ecs::RockTag>(m);
                else if (uData.type == ecs::UnitType::Warrior)
                    match = registry.all_of<ecs::BushTag>(m);

                if (match) count++;
            }

            if (count > bestCount) {
                bestCount = count;
                bestTask = tp.entity;
            }
        }

        if (bestTask != entt::null) {
            wState.currentTask = bestTask;
            registry.emplace_or_replace<ecs::WorkerTag>(w);
            if (registry.all_of<ecs::IdleTag>(w))
                registry.remove<ecs::IdleTag>(w);

            for (auto& tp : taskProfiles) {
                if (tp.entity == bestTask) { tp.harvesters++; break; }
            }

            simLog.log("[Logistics] " + std::string(core::SimLogger::typeName(uData.type))
                + " #" + std::to_string(core::SimLogger::eid(w))
                + " (idle) joined Task #" + std::to_string(core::SimLogger::eid(bestTask))
                + " (" + std::to_string(bestCount) + " compatible resources)");
        }
    }

    // --- End-of-cycle summary ---
    simLog.log("[Logistics] Final courier assignments:");
    for (auto& cp : courierProfiles) {
        std::string taskStr;
        if (cp.task == entt::null) taskStr = "IDLE";
        else if (registry.valid(cp.task) && registry.all_of<ecs::BuildingTag>(cp.task))
            taskStr = "Building #" + std::to_string(core::SimLogger::eid(cp.task));
        else taskStr = "Task #" + std::to_string(core::SimLogger::eid(cp.task));
        simLog.log("[Logistics]   Courier #" + std::to_string(core::SimLogger::eid(cp.entity))
            + " -> " + taskStr + (cp.isCarrying ? " [carrying]" : ""));
    }
    simLog.log("[Logistics] Final task staffing:");
    for (auto& tp : taskProfiles) {
        simLog.log("[Logistics]   Task #" + std::to_string(core::SimLogger::eid(tp.entity))
            + " harvesters=" + std::to_string(tp.harvesters)
            + " couriers=" + std::to_string(tp.couriers)
            + " needed=" + std::to_string(tp.couriersNeeded));
    }
    simLog.log("[Logistics] ========== CYCLE END ==========");
}

}
