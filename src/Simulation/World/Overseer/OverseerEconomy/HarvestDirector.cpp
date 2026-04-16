#include "Simulation/World/OverseerSystem.hpp"
#include "Input/CursorTaskSystem/TaskFinalizer.hpp"
#include "ECS/Tags.hpp"
#include "Config/BuildingsConfig.hpp"
#include "Core/SimLogger.hpp"
#include <cmath>
#include <vector>

namespace wr::simulation {

void OverseerSystem::manageResources(entt::registry& registry, world::ChunkManager& chunkManager) {
    auto storageCheck = registry.view<ecs::CityStorageTag>();
    if (storageCheck.begin() == storageCheck.end()) return;

    float currentTime = m_internalClock.getElapsedTime().asSeconds();
    if (currentTime - m_lastScanTime < 2.5f) return;
    m_lastScanTime = currentTime;

    auto& board = registry.ctx().get<SimBlackboard>();

    auto dispatchHarvest = [&](bool forWood) {
        // Find idle workers of the right type and idle couriers
        std::vector<entt::entity> workers;
        entt::entity spareCourier = entt::null;
        int idleCouriers = 0;
        auto potentialWorkers = registry.view<ecs::UnitData, ecs::WorkerState>();
        for (auto w : potentialWorkers) {
            auto& uData = potentialWorkers.get<ecs::UnitData>(w);
            auto& wState = potentialWorkers.get<ecs::WorkerState>(w);

            bool isIdle = !registry.all_of<ecs::WorkerTag>(w) &&
                          (wState.currentTask == entt::null && !registry.all_of<ecs::PathRequest>(w) && !registry.all_of<ecs::Path>(w));
            if (!isIdle) continue;

            if (forWood && uData.type == ecs::UnitType::Lumberjack) workers.push_back(w);
            else if (!forWood && uData.type == ecs::UnitType::Miner) workers.push_back(w);
            else if (uData.type == ecs::UnitType::Courier) {
                if (spareCourier == entt::null) spareCourier = w;
                idleCouriers++;
            }
        }

        // No idle harvesters → nothing to do
        if (workers.empty()) return;

        // GATE: require at least 1 courier for logistics support
        if (spareCourier == entt::null) return;

        // Reserve a courier for building supply only if no courier is already supplying
        if (board.pendingBuildings > 0 && idleCouriers <= 1) {
            int couriersOnBuildings = 0;
            for (auto bw : potentialWorkers) {
                auto& bwData = potentialWorkers.get<ecs::UnitData>(bw);
                auto& bwState = potentialWorkers.get<ecs::WorkerState>(bw);
                if (bwData.type == ecs::UnitType::Courier && bwState.currentTask != entt::null &&
                    registry.valid(bwState.currentTask) && registry.all_of<ecs::BuildingTag>(bwState.currentTask))
                    couriersOnBuildings++;
            }
            if (couriersOnBuildings == 0) return;
        }

        workers.push_back(spareCourier);

        // Exclusion zones: don't harvest inside unbuilt building resource zones
        struct ExclusionZone { float minX, maxX, minY, maxY; };
        std::vector<ExclusionZone> exclusions;
        auto bView = registry.view<ecs::BuildingTag, ecs::WorldPos, ecs::ConstructionData>();
        for (auto b : bView) {
            auto& cData = bView.get<ecs::ConstructionData>(b);
            if (!cData.isBuilt) {
                auto& bWp = bView.get<ecs::WorldPos>(b);
                exclusions.push_back({
                    bWp.wx - cData.resourceZoneWidth / 2.0f, bWp.wx + cData.resourceZoneWidth / 2.0f,
                    bWp.wy - cData.resourceZoneHeight / 2.0f, bWp.wy + cData.resourceZoneHeight / 2.0f
                });
            }
        }

        auto inExclusionZone = [&](float x, float y) {
            for (const auto& z : exclusions) {
                if (x >= z.minX && x <= z.maxX && y >= z.minY && y <= z.maxY) return true;
            }
            return false;
        };

        // Find closest resource to workers' average position
        math::Vec2f searchOrigin{0.0f, 0.0f};
        for (auto e : workers) {
            auto& wp = registry.get<ecs::WorldPos>(e);
            searchOrigin.x += wp.wx; searchOrigin.y += wp.wy;
        }
        searchOrigin.x /= workers.size(); searchOrigin.y /= workers.size();

        entt::entity bestRes = entt::null;
        float bestDistSq = 9999999.0f;
        math::Vec2f bestPos;

        if (forWood) {
            auto trees = registry.view<ecs::TreeTag, ecs::WorldPos>(entt::exclude<ecs::MarkedForHarvestTag>);
            for (auto e : trees) {
                auto& wp = trees.get<ecs::WorldPos>(e);
                if (inExclusionZone(wp.wx, wp.wy)) continue;
                float dSq = (wp.wx - searchOrigin.x) * (wp.wx - searchOrigin.x) + (wp.wy - searchOrigin.y) * (wp.wy - searchOrigin.y);
                if (dSq < bestDistSq) { bestDistSq = dSq; bestRes = e; bestPos = math::Vec2f{wp.wx, wp.wy}; }
            }
        } else {
            auto rocks = registry.view<ecs::RockTag, ecs::WorldPos>(entt::exclude<ecs::MarkedForHarvestTag>);
            for (auto e : rocks) {
                auto& wp = rocks.get<ecs::WorldPos>(e);
                if (inExclusionZone(wp.wx, wp.wy)) continue;
                float dSq = (wp.wx - searchOrigin.x) * (wp.wx - searchOrigin.x) + (wp.wy - searchOrigin.y) * (wp.wy - searchOrigin.y);
                if (dSq < bestDistSq) { bestDistSq = dSq; bestRes = e; bestPos = math::Vec2f{wp.wx, wp.wy}; }
            }
        }

        if (bestRes == entt::null) return;

        // Build PendingTaskArea
        float boxRadius = 6.0f;
        bool canTrees = forWood, canRocks = !forWood;

        ecs::PendingTaskArea pArea;
        pArea.areas.push_back(ecs::RectArea{
            math::Vec2f{bestPos.x - boxRadius, bestPos.y - boxRadius},
            math::Vec2f{bestPos.x + boxRadius, bestPos.y + boxRadius},
            canTrees, canRocks, true, false, true,
            canTrees, canRocks, true, false, true
        });
        pArea.collectFutureDrops = true;
        pArea.useCityStorage = true;

        // Use the shared task pipeline
        registry.clear<ecs::PreviewHarvestTag>();
        input::TaskFinalizer::scanResources(registry, pArea);
        auto taskEnt = input::TaskFinalizer::finalizeTask(registry, pArea, m_globalTaskId, workers);
        registry.clear<ecs::PreviewHarvestTag>();

        if (taskEnt != entt::null) {
            std::string workerList;
            for (auto w : workers) {
                auto& ud = registry.get<ecs::UnitData>(w);
                workerList += " " + std::string(core::SimLogger::typeName(ud.type))
                    + " #" + std::to_string(core::SimLogger::eid(w));
            }
            core::SimLogger::get().log("[Harvest] Dispatched " + std::string(forWood ? "wood" : "rock")
                + " task #" + std::to_string(core::SimLogger::eid(taskEnt))
                + " (id=" + std::to_string(m_globalTaskId - 1) + ") with" + workerList
                + " near " + core::SimLogger::pos(bestPos.x, bestPos.y)
                + " (idle couriers=" + std::to_string(idleCouriers) + ")");
        }
    };

    // Current demand: resources needed for buildings under construction
    // If current demand is met, proactively gather for the next stage (one stage ahead only)
    // Only proactively gather once blueprints have been placed (houses exist or are pending)
    int targetWood = board.demandWood;
    int targetRock = board.demandRock;
    if ((board.totalHouses > 0 || board.pendingBuildings > 0) &&
        board.availWood >= board.demandWood && board.availRock >= board.demandRock) {
        targetWood += board.nextStageWood;
        targetRock += board.nextStageRock;
    }

    bool needWood = targetWood > 0 && board.availWood < targetWood && board.popLumberjack > 0;
    bool needRock = targetRock > 0 && board.availRock < targetRock && board.popMiner > 0;

    if (needWood && needRock) {
        // Dispatch the resource with the greater deficit ratio first
        float woodRatio = static_cast<float>(board.availWood) / static_cast<float>(targetWood);
        float rockRatio = static_cast<float>(board.availRock) / static_cast<float>(targetRock);
        if (rockRatio <= woodRatio) {
            dispatchHarvest(false);
            dispatchHarvest(true);
        } else {
            dispatchHarvest(true);
            dispatchHarvest(false);
        }
    } else if (needWood) {
        dispatchHarvest(true);
    } else if (needRock) {
        dispatchHarvest(false);
    }
}

}
