#include "Simulation/World/OverseerSystem.hpp"
#include "Simulation/BuildingPlacement.hpp"
#include "ECS/Tags.hpp"
#include "Config/BuildingsConfig.hpp"
#include "Core/SimLogger.hpp"
#include <cmath>
#include <vector>
#include <algorithm>

namespace wr::simulation {

struct GridCandidate {
    float distSq;
    math::Vec2f pos;
};

static std::vector<math::Vec2f> generateHouseGrid(math::Vec2f storagePos, int count) {
    constexpr float storageRZHalf = config::CITY_STORAGE.resourceZoneWidth / 2.0f;
    constexpr float houseRZHalf   = config::HOUSE_1.resourceZoneWidth / 2.0f;
    constexpr float houseRZWidth  = config::HOUSE_1.resourceZoneWidth;

    std::vector<GridCandidate> candidates;

    for (int gx = -8; gx <= 8; gx++) {
        for (int gy = -8; gy <= 8; gy++) {
            if (gx == 0 && gy == 0) continue;

            float ox = 0.0f, oy = 0.0f;
            if (gx > 0)      ox =  (storageRZHalf + houseRZHalf) + (gx - 1) * houseRZWidth;
            else if (gx < 0) ox = -(storageRZHalf + houseRZHalf + (-gx - 1) * houseRZWidth);
            if (gy > 0)      oy =  (storageRZHalf + houseRZHalf) + (gy - 1) * houseRZWidth;
            else if (gy < 0) oy = -(storageRZHalf + houseRZHalf + (-gy - 1) * houseRZWidth);

            float hMinX = ox - houseRZHalf, hMaxX = ox + houseRZHalf;
            float hMinY = oy - houseRZHalf, hMaxY = oy + houseRZHalf;
            if (hMaxX > -storageRZHalf && hMinX < storageRZHalf &&
                hMaxY > -storageRZHalf && hMinY < storageRZHalf) continue;

            candidates.push_back({ox * ox + oy * oy, {storagePos.x + ox, storagePos.y + oy}});
        }
    }

    std::sort(candidates.begin(), candidates.end(),
              [](const GridCandidate& a, const GridCandidate& b) { return a.distSq < b.distSq; });

    std::vector<math::Vec2f> result;
    result.reserve(count);
    for (int i = 0; i < count && i < static_cast<int>(candidates.size()); i++) {
        result.push_back(candidates[i].pos);
    }
    return result;
}

static void queueBuildingForBuilders(entt::registry& registry, entt::entity bEnt) {
    auto builders = registry.view<ecs::UnitData, ecs::WorkerState>();
    for (auto w : builders) {
        auto& uData = builders.get<ecs::UnitData>(w);
        if (uData.type != ecs::UnitType::Builder) continue;

        auto& wState = builders.get<ecs::WorkerState>(w);
        wState.taskQueue.push_back(bEnt);

        if (wState.currentTask == entt::null) {
            wState.currentTask = wState.taskQueue.front();
            registry.emplace_or_replace<ecs::ActionTarget>(w, wState.currentTask);
            registry.emplace_or_replace<ecs::WorkerTag>(w);
            registry.remove<ecs::IdleTag>(w);
            registry.remove<ecs::Path>(w);
        }
    }
}

static bool allBuildingsComplete(entt::registry& registry, std::vector<entt::entity>& buildings) {
    bool allBuilt = true;
    for (auto it = buildings.begin(); it != buildings.end();) {
        if (!registry.valid(*it)) {
            it = buildings.erase(it);
            continue;
        }
        auto* cd = registry.try_get<ecs::ConstructionData>(*it);
        if (!cd || !cd->isBuilt) {
            allBuilt = false;
        }
        ++it;
    }
    return allBuilt;
}

void OverseerSystem::planExpansion(entt::registry& registry, world::ChunkManager& chunkManager) {
    float currentTime = m_internalClock.getElapsedTime().asSeconds();
    if (currentTime - m_lastBuildTime < 3.0f) return;
    m_lastBuildTime = currentTime;

    auto& board = registry.ctx().get<SimBlackboard>();

    switch (m_phase) {

    case ExpansionPhase::PlaceStorage: {
        auto storageView = registry.view<ecs::CityStorageTag, ecs::WorldPos>();
        if (storageView.begin() != storageView.end()) {
            m_phase = ExpansionPhase::ClearHousingArea;
            core::SimLogger::get().log("[Expansion] Storage exists. Phase: ClearHousingArea");
            break;
        }

        math::Vec2f searchCenter{10.0f, 10.0f};
        bool found = false;
        math::Vec2f bestSpot;
        bool foundTerrainOnly = false;
        math::Vec2f bestTerrainSpot;

        for (float r = 2.0f; r < 30.0f && !found; r += 2.0f) {
            for (float angle = 0.0f; angle < 6.28f; angle += 0.5f) {
                float px = searchCenter.x + std::cos(angle) * r;
                float py = searchCenter.y + std::sin(angle) * r;

                if (!isTerrainValid(chunkManager, {px, py}, config::CITY_STORAGE)) continue;

                if (!foundTerrainOnly) {
                    foundTerrainOnly = true;
                    bestTerrainSpot = {px, py};
                }

                if (hasObstacleOverlap(registry, {px, py}, config::CITY_STORAGE) ||
                    hasResourceZoneOverlap(registry, {px, py}, config::CITY_STORAGE, true)) continue;

                bestSpot = {px, py};
                found = true;
                break;
            }
        }

        if (found) {
            auto storageEnt = spawnBuilding(registry, chunkManager, config::CITY_STORAGE, bestSpot, true);
            registry.emplace<SpawnedFlag>(storageEnt);

            float buffer = 1.5f;
            float hw = config::CITY_STORAGE.hitboxWidth / 2.0f + buffer;
            float hh = config::CITY_STORAGE.hitboxHeight / 2.0f + buffer;
            createTask(registry,
                {bestSpot.x - hw, bestSpot.y - hh}, {bestSpot.x + hw, bestSpot.y + hh},
                true, true, false, {0,0}, {0,0});

            m_phase = ExpansionPhase::ClearHousingArea;
            core::SimLogger::get().log("[Expansion] City Storage placed at " + core::SimLogger::pos(bestSpot.x, bestSpot.y) + ". Phase: ClearHousingArea");
        } else if (foundTerrainOnly) {
            m_pendingStoragePos = bestTerrainSpot;

            float buffer = 1.5f;
            float hw = config::CITY_STORAGE.hitboxWidth / 2.0f + buffer;
            float hh = config::CITY_STORAGE.hitboxHeight / 2.0f + buffer;
            float maxX = bestTerrainSpot.x + hw;
            float minY = bestTerrainSpot.y - hh;
            float maxY = bestTerrainSpot.y + hh;
            m_clearingTask = createTask(registry,
                {bestTerrainSpot.x - hw, minY}, {maxX, maxY},
                true, false, true, {maxX + 0.5f, minY}, {maxX + 3.0f, maxY});

            m_phase = ExpansionPhase::ClearForStorage;
            core::SimLogger::get().log("[Expansion] No clear spot for storage. Clearing area at " + core::SimLogger::pos(bestTerrainSpot.x, bestTerrainSpot.y) + ". Phase: ClearForStorage");
        }
        break;
    }

    case ExpansionPhase::ClearForStorage: {
        // Wait for clearing task to fully complete (all resources harvested, all drops delivered)
        if (m_clearingTask != entt::null && registry.valid(m_clearingTask))
            break;

        m_clearingTask = entt::null;

        if (!hasObstacleOverlap(registry, m_pendingStoragePos, config::CITY_STORAGE) &&
            !hasResourceZoneOverlap(registry, m_pendingStoragePos, config::CITY_STORAGE, true)) {
            auto storageEnt = spawnBuilding(registry, chunkManager, config::CITY_STORAGE, m_pendingStoragePos, true);
            registry.emplace<SpawnedFlag>(storageEnt);

            m_phase = ExpansionPhase::ClearHousingArea;
            core::SimLogger::get().log("[Expansion] Area cleared. City Storage placed at " + core::SimLogger::pos(m_pendingStoragePos.x, m_pendingStoragePos.y) + ". Phase: ClearHousingArea");
        }
        break;
    }

    case ExpansionPhase::ClearHousingArea: {
        auto storageView = registry.view<ecs::CityStorageTag, ecs::WorldPos>();
        if (storageView.begin() == storageView.end()) {
            m_phase = ExpansionPhase::PlaceStorage;
            break;
        }

        if (m_clearingTask == entt::null) {
            auto& sWp = storageView.get<ecs::WorldPos>(*storageView.begin());

            constexpr float storageRZHalf = config::CITY_STORAGE.resourceZoneWidth / 2.0f;
            constexpr float houseRZWidth  = config::HOUSE_1.resourceZoneWidth;
            constexpr float houseHitboxHalf = config::HOUSE_1.hitboxWidth / 2.0f;
            float clearRadius = storageRZHalf + 2.0f * houseRZWidth + houseHitboxHalf + 1.5f;

            m_clearingTask = createTask(registry,
                {sWp.wx - clearRadius, sWp.wy - clearRadius},
                {sWp.wx + clearRadius, sWp.wy + clearRadius},
                true, true, false, {0,0}, {0,0});

            if (m_clearingTask == entt::null) {
                m_phase = ExpansionPhase::PlanInitialHousing;
                core::SimLogger::get().log("[Expansion] Housing area already clear. Phase: PlanInitialHousing");
            } else {
                core::SimLogger::get().log("[Expansion] Clearing housing area around storage");
            }
        } else {
            if (!registry.valid(m_clearingTask)) {
                m_clearingTask = entt::null;
                m_phase = ExpansionPhase::PlanInitialHousing;
                core::SimLogger::get().log("[Expansion] Housing area cleared. Phase: PlanInitialHousing");
            }
        }
        break;
    }

    case ExpansionPhase::PlanInitialHousing: {
        auto storageView = registry.view<ecs::CityStorageTag, ecs::WorldPos>();
        if (storageView.begin() == storageView.end()) {
            m_phase = ExpansionPhase::PlaceStorage;
            break;
        }

        auto& storageWp = storageView.get<ecs::WorldPos>(*storageView.begin());
        math::Vec2f sPos{storageWp.wx, storageWp.wy};

        int housesNeeded = static_cast<int>(std::ceil(
            static_cast<float>(board.totalUnits) / static_cast<float>(config::UNITS_PER_HOUSE)));
        if (housesNeeded < 1) housesNeeded = 1;

        housesNeeded -= board.totalHouses;
        if (housesNeeded <= 0) {
            m_phase = ExpansionPhase::WaitInitialHousing;
            break;
        }

        m_totalUnitsAtPlanTime = board.totalUnits;

        auto gridPositions = generateHouseGrid(sPos, housesNeeded + 20);

        m_pendingBuildings.clear();
        int placed = 0;

        for (auto& pos : gridPositions) {
            if (placed >= housesNeeded) break;
            if (!canPlaceBuilding(registry, chunkManager, pos, config::HOUSE_1, false)) continue;

            auto bEnt = spawnBuilding(registry, chunkManager, config::HOUSE_1, pos, false);

            float buffer = 1.5f;
            float hw = config::HOUSE_1.hitboxWidth / 2.0f + buffer;
            float hh = config::HOUSE_1.hitboxHeight / 2.0f + buffer;
            createTask(registry,
                {pos.x - hw, pos.y - hh}, {pos.x + hw, pos.y + hh},
                true, true, false, {0,0}, {0,0});

            queueBuildingForBuilders(registry, bEnt);
            m_pendingBuildings.push_back(bEnt);
            placed++;

            core::SimLogger::get().log("[Expansion] House blueprint at " + core::SimLogger::pos(pos.x, pos.y));
        }

        m_phase = ExpansionPhase::WaitInitialHousing;
        core::SimLogger::get().log("[Expansion] Initial housing: " + std::to_string(placed) + " houses planned for " + std::to_string(board.totalUnits) + " units. Phase: WaitInitialHousing");
        break;
    }

    case ExpansionPhase::WaitInitialHousing: {
        if (allBuildingsComplete(registry, m_pendingBuildings)) {
            m_phase = ExpansionPhase::PlanGrowthHousing;
            core::SimLogger::get().log("[Expansion] All initial houses built. Phase: PlanGrowthHousing");
        }
        break;
    }

    case ExpansionPhase::PlanGrowthHousing: {
        auto storageView = registry.view<ecs::CityStorageTag, ecs::WorldPos>();
        if (storageView.begin() == storageView.end()) {
            m_phase = ExpansionPhase::PlaceStorage;
            break;
        }

        auto& storageWp = storageView.get<ecs::WorldPos>(*storageView.begin());
        math::Vec2f sPos{storageWp.wx, storageWp.wy};

        int growthUnits = board.totalUnits / 2;
        if (growthUnits < 2) growthUnits = 2;

        int housesNeeded = static_cast<int>(std::ceil(
            static_cast<float>(growthUnits) / static_cast<float>(config::UNITS_PER_HOUSE)));

        m_totalUnitsAtPlanTime = board.totalUnits;

        auto gridPositions = generateHouseGrid(sPos, board.totalHouses + housesNeeded + 20);

        m_pendingBuildings.clear();
        int placed = 0;

        for (auto& pos : gridPositions) {
            if (placed >= housesNeeded) break;
            if (!canPlaceBuilding(registry, chunkManager, pos, config::HOUSE_1, false)) continue;

            auto bEnt = spawnBuilding(registry, chunkManager, config::HOUSE_1, pos, false);

            float buffer = 1.5f;
            float hw = config::HOUSE_1.hitboxWidth / 2.0f + buffer;
            float hh = config::HOUSE_1.hitboxHeight / 2.0f + buffer;
            createTask(registry,
                {pos.x - hw, pos.y - hh}, {pos.x + hw, pos.y + hh},
                true, true, false, {0,0}, {0,0});

            queueBuildingForBuilders(registry, bEnt);
            m_pendingBuildings.push_back(bEnt);
            placed++;
        }

        m_phase = ExpansionPhase::WaitGrowthHousing;
        core::SimLogger::get().log("[Expansion] Growth: " + std::to_string(placed) + " houses for " + std::to_string(growthUnits) + " new units (pop: " + std::to_string(board.totalUnits) + "). Phase: WaitGrowthHousing");
        break;
    }

    case ExpansionPhase::WaitGrowthHousing: {
        if (!allBuildingsComplete(registry, m_pendingBuildings)) break;

        if (board.totalUnits > m_totalUnitsAtPlanTime) {
            m_phase = ExpansionPhase::PlanGrowthHousing;
            core::SimLogger::get().log("[Expansion] Growth cycle complete. Pop: " + std::to_string(board.totalUnits) + ". Phase: PlanGrowthHousing");
        }
        break;
    }

    }
}

}
