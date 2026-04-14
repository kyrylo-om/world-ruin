#include "Simulation/World/OverseerSystem.hpp"
#include "ECS/Tags.hpp"
#include "Config/BuildingsConfig.hpp"
#include <cmath>
#include <iostream>
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

static bool isValidBuildSpot(world::ChunkManager& chunkManager, float px, float py) {
    int bx = static_cast<int>(std::floor(px));
    int by = static_cast<int>(std::floor(py));

    auto info = chunkManager.getGlobalTileInfo(bx, by);
    if (info.type == core::TerrainType::Water || info.isRamp) return false;

    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            auto nInfo = chunkManager.getGlobalTileInfo(bx + dx, by + dy);
            if (nInfo.elevationLevel != info.elevationLevel ||
                nInfo.type == core::TerrainType::Water || nInfo.isRamp) {
                return false;
            }
        }
    }
    return true;
}

static uint8_t getElevationAt(world::ChunkManager& chunkManager, float px, float py) {
    return chunkManager.getGlobalTileInfo(
        static_cast<int>(std::floor(px)),
        static_cast<int>(std::floor(py))
    ).elevationLevel;
}

static bool overlapsExistingBuilding(entt::registry& registry, math::Vec2f pos, const config::BuildingDef& bDef) {
    float hw = bDef.hitboxWidth / 2.0f;
    float hh = bDef.hitboxHeight / 2.0f;
    float minX = pos.x - hw, maxX = pos.x + hw;
    float minY = pos.y - hh, maxY = pos.y + hh;

    auto bView = registry.view<ecs::BuildingTag, ecs::WorldPos>();
    for (auto e : bView) {
        auto& bWp = bView.get<ecs::WorldPos>(e);
        float entityRad = 0.4f;
        if (auto* hb = registry.try_get<ecs::Hitbox>(e)) entityRad = hb->radius / 64.0f;

        if (bWp.wx + entityRad > minX && bWp.wx - entityRad < maxX &&
            bWp.wy + entityRad > minY && bWp.wy - entityRad < maxY) {
            return true;
        }
    }

    float rw = bDef.resourceZoneWidth / 2.0f;
    float rh = bDef.resourceZoneHeight / 2.0f;
    float rMinX = pos.x - rw, rMaxX = pos.x + rw;
    float rMinY = pos.y - rh, rMaxY = pos.y + rh;

    auto storageView = registry.view<ecs::CityStorageTag, ecs::WorldPos>();
    for (auto s : storageView) {
        auto& sWp = storageView.get<ecs::WorldPos>(s);
        float sRw = config::CITY_STORAGE.resourceZoneWidth / 2.0f;
        float sRh = config::CITY_STORAGE.resourceZoneHeight / 2.0f;
        if (!(rMaxX < sWp.wx - sRw || rMinX > sWp.wx + sRw ||
              rMaxY < sWp.wy - sRh || rMinY > sWp.wy + sRh)) {
            return true;
        }
    }

    return false;
}

static entt::entity placeBuilding(entt::registry& registry, const config::BuildingDef& bDef,
                                   math::Vec2f pos, uint8_t elevLevel, bool isStorage, int& globalTaskId) {
    float zHeight = (elevLevel - 1) * 32.0f;

    auto bEnt = registry.create();
    registry.emplace<ecs::BuildingTag>(bEnt);
    if (isStorage) registry.emplace<ecs::CityStorageTag>(bEnt);

    registry.emplace<ecs::LogicalPos>(bEnt, static_cast<int64_t>(pos.x), static_cast<int64_t>(pos.y));
    registry.emplace<ecs::WorldPos>(bEnt, pos.x, pos.y, zHeight, zHeight);
    registry.emplace<ecs::ScreenPos>(bEnt, 0.f, 0.f, 255.0f, 255.0f, static_cast<uint8_t>(0));
    registry.emplace<ecs::Hitbox>(bEnt, (bDef.hitboxWidth * 64.0f) / 2.0f);
    registry.emplace<ecs::ClickArea>(bEnt, (bDef.hitboxWidth * 64.0f) / 2.0f);
    registry.emplace<ecs::SolidTag>(bEnt);

    bool instant = (bDef.buildTime <= 0.0f);
    registry.emplace<ecs::ConstructionData>(bEnt,
        bDef.cost.wood, bDef.cost.rock,
        instant ? 0 : bDef.cost.wood,
        instant ? 0 : bDef.cost.rock,
        bDef.resourceZoneWidth, bDef.resourceZoneHeight,
        instant, instant ? bDef.buildTime : 0.0f, bDef.buildTime
    );

    auto* tilesPtr = registry.ctx().find<const rendering::TileHandler*>();
    auto& spr = registry.emplace<ecs::SpriteComponent>(bEnt).sprite;
    if (tilesPtr) {
        spr.setTexture((*tilesPtr)->getHouse1Texture());
        auto bounds = spr.getLocalBounds();
        spr.setOrigin(bounds.width / 2.0f, bounds.height * 0.65f);

        if (isStorage) {
            spr.setScale(2.4f, 2.4f);
            spr.setColor(sf::Color(150, 200, 255));
        } else {
            spr.setScale(1.8f, 1.8f);
        }
    }

    return bEnt;
}

static void clearBuildingSite(entt::registry& registry, const config::BuildingDef& bDef,
                               math::Vec2f pos, int& globalTaskId) {
    float hw = bDef.resourceZoneWidth / 2.0f;
    float hh = bDef.resourceZoneHeight / 2.0f;
    float minX = pos.x - hw, maxX = pos.x + hw;
    float minY = pos.y - hh, maxY = pos.y + hh;

    std::vector<entt::entity> toMark;
    auto resView = registry.view<ecs::WorldPos>();
    for (auto e : resView) {
        if (registry.any_of<ecs::TreeTag, ecs::RockTag, ecs::SmallRockTag, ecs::BushTag, ecs::LogTag>(e) &&
            !registry.any_of<ecs::MarkedForHarvestTag>(e)) {
            auto& wp = resView.get<ecs::WorldPos>(e);
            if (wp.wx >= minX && wp.wx <= maxX && wp.wy >= minY && wp.wy <= maxY) {
                toMark.push_back(e);
            }
        }
    }

    if (!toMark.empty()) {
        ecs::RectArea rect{
            math::Vec2f{minX, minY}, math::Vec2f{maxX, maxY},
            true, true, true, true, true, true, true, true, true, true
        };
        auto taskEnt = registry.create();
        sf::Color taskCol(255, 50, 50, 80);
        registry.emplace<ecs::TaskArea>(taskEnt, std::vector<ecs::RectArea>{rect},
            std::vector<entt::entity>{}, taskCol, globalTaskId++, false,
            math::Vec2f{0, 0}, math::Vec2f{0, 0}, true, true);

        sf::Color solidCol(255, 50, 50, 255);
        for (auto e : toMark) {
            registry.emplace<ecs::MarkedForHarvestTag>(e, solidCol, taskEnt);
        }

        auto workers = registry.view<ecs::UnitData, ecs::WorkerState>();
        for (auto w : workers) {
            if (!registry.all_of<ecs::WorkerTag>(w)) {
                auto type = workers.get<ecs::UnitData>(w).type;
                if (type == ecs::UnitType::Warrior || type == ecs::UnitType::Lumberjack || type == ecs::UnitType::Miner) {
                    registry.get<ecs::WorkerState>(w).currentTask = taskEnt;
                    registry.emplace_or_replace<ecs::WorkerTag>(w);
                    registry.remove<ecs::IdleTag>(w);
                }
            }
        }
    }
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
            m_phase = ExpansionPhase::PlanInitialHousing;
            std::cout << "[Overseer] Storage exists. Moving to initial housing." << std::endl;
            break;
        }

        math::Vec2f searchCenter{10.0f, 10.0f};
        bool found = false;
        math::Vec2f bestSpot;

        for (float r = 2.0f; r < 30.0f && !found; r += 2.0f) {
            for (float angle = 0.0f; angle < 6.28f; angle += 0.5f) {
                float px = searchCenter.x + std::cos(angle) * r;
                float py = searchCenter.y + std::sin(angle) * r;

                if (!isValidBuildSpot(chunkManager, px, py)) continue;

                bestSpot = {px, py};
                found = true;
                break;
            }
        }

        if (found) {
            uint8_t elev = getElevationAt(chunkManager, bestSpot.x, bestSpot.y);
            auto storageEnt = placeBuilding(registry, config::CITY_STORAGE, bestSpot, elev, true, m_globalTaskId);

            registry.emplace<SpawnedFlag>(storageEnt);

            clearBuildingSite(registry, config::CITY_STORAGE, bestSpot, m_globalTaskId);

            m_phase = ExpansionPhase::PlanInitialHousing;
            std::cout << "[Overseer] City Storage placed at (" << bestSpot.x << ", " << bestSpot.y << ")." << std::endl;
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
            if (!isValidBuildSpot(chunkManager, pos.x, pos.y)) continue;
            if (overlapsExistingBuilding(registry, pos, config::HOUSE_1)) continue;

            uint8_t elev = getElevationAt(chunkManager, pos.x, pos.y);
            auto bEnt = placeBuilding(registry, config::HOUSE_1, pos, elev, false, m_globalTaskId);
            clearBuildingSite(registry, config::HOUSE_1, pos, m_globalTaskId);
            queueBuildingForBuilders(registry, bEnt);

            m_pendingBuildings.push_back(bEnt);
            placed++;

            std::cout << "[Overseer] House blueprint at (" << pos.x << ", " << pos.y << ")." << std::endl;
        }

        m_phase = ExpansionPhase::WaitInitialHousing;
        std::cout << "[Overseer] Initial housing: " << placed << " houses planned for "
                  << board.totalUnits << " units." << std::endl;
        break;
    }

    case ExpansionPhase::WaitInitialHousing: {
        if (allBuildingsComplete(registry, m_pendingBuildings)) {
            m_phase = ExpansionPhase::PlanGrowthHousing;
            std::cout << "[Overseer] All initial houses built. Planning growth expansion." << std::endl;
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
            if (!isValidBuildSpot(chunkManager, pos.x, pos.y)) continue;
            if (overlapsExistingBuilding(registry, pos, config::HOUSE_1)) continue;

            uint8_t elev = getElevationAt(chunkManager, pos.x, pos.y);
            auto bEnt = placeBuilding(registry, config::HOUSE_1, pos, elev, false, m_globalTaskId);
            clearBuildingSite(registry, config::HOUSE_1, pos, m_globalTaskId);
            queueBuildingForBuilders(registry, bEnt);

            m_pendingBuildings.push_back(bEnt);
            placed++;
        }

        m_phase = ExpansionPhase::WaitGrowthHousing;
        std::cout << "[Overseer] Growth: " << placed << " houses for " << growthUnits
                  << " new units (pop: " << board.totalUnits << ")." << std::endl;
        break;
    }

    case ExpansionPhase::WaitGrowthHousing: {
        if (!allBuildingsComplete(registry, m_pendingBuildings)) break;

        if (board.totalUnits > m_totalUnitsAtPlanTime) {
            m_phase = ExpansionPhase::PlanGrowthHousing;
            std::cout << "[Overseer] Growth cycle complete. Pop: " << board.totalUnits
                      << ". Starting next expansion." << std::endl;
        }
        break;
    }

    }
}

}
