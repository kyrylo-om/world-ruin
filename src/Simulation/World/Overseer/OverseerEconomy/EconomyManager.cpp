#include "Simulation/World/OverseerSystem.hpp"
#include "ECS/Tags.hpp"
#include "Config/BuildingsConfig.hpp"
#include <cmath>
#include <iterator>

namespace wr::simulation {

void OverseerSystem::updateEconomy(entt::registry& registry) {
    if (!registry.ctx().contains<SimBlackboard>()) {
        registry.ctx().emplace<SimBlackboard>();
    }

    auto& board = registry.ctx().get<SimBlackboard>();

    board.popWarrior = 0; board.popLumberjack = 0;
    board.popMiner = 0; board.popCourier = 0; board.popBuilder = 0;

    auto unitView = registry.view<ecs::UnitData>();
    for (auto e : unitView) {
        auto type = unitView.get<ecs::UnitData>(e).type;
        if (type == ecs::UnitType::Warrior) board.popWarrior++;
        else if (type == ecs::UnitType::Lumberjack) board.popLumberjack++;
        else if (type == ecs::UnitType::Miner) board.popMiner++;
        else if (type == ecs::UnitType::Courier) board.popCourier++;
        else if (type == ecs::UnitType::Builder) board.popBuilder++;
    }

    // Count resources only inside the city storage zone — scattered map resources
    // are not usable for construction until couriers deliver them.
    board.availWood = 0;
    board.availRock = 0;
    auto storageView = registry.view<ecs::CityStorageTag, ecs::WorldPos, ecs::ConstructionData>();
    if (storageView.begin() != storageView.end()) {
        auto sEnt = *storageView.begin();
        auto& sWp = storageView.get<ecs::WorldPos>(sEnt);
        auto& sCData = storageView.get<ecs::ConstructionData>(sEnt);
        float sMinX = sWp.wx - sCData.resourceZoneWidth / 2.0f;
        float sMaxX = sWp.wx + sCData.resourceZoneWidth / 2.0f;
        float sMinY = sWp.wy - sCData.resourceZoneHeight / 2.0f;
        float sMaxY = sWp.wy + sCData.resourceZoneHeight / 2.0f;

        auto resView = registry.view<ecs::WorldPos, ecs::ResourceTag>();
        for (auto e : resView) {
            auto& rWp = resView.get<ecs::WorldPos>(e);
            if (rWp.wx >= sMinX && rWp.wx <= sMaxX && rWp.wy >= sMinY && rWp.wy <= sMaxY) {
                if (registry.all_of<ecs::LogTag>(e)) board.availWood++;
                if (registry.all_of<ecs::SmallRockTag>(e)) board.availRock++;
            }
        }
    }

    board.demandWood = 0;
    board.demandRock = 0;
    board.pendingBuildings = 0;

    math::Vec2f cityCenter{10.0f, 10.0f};
    auto bView = registry.view<ecs::BuildingTag, ecs::WorldPos, ecs::ConstructionData>();

    if (bView.begin() != bView.end()) {
        float sumX = 0, sumY = 0;
        int bCount = 0;
        for (auto e : bView) {
            auto& wp = bView.get<ecs::WorldPos>(e);
            sumX += wp.wx; sumY += wp.wy;
            bCount++;

            const auto& cData = bView.get<ecs::ConstructionData>(e);
            if (!cData.isBuilt) {
                board.demandWood += cData.woodRequired;
                board.demandRock += cData.rockRequired;
                board.pendingBuildings++;
            }
        }
        if (bCount > 0) {
            cityCenter = math::Vec2f{sumX / static_cast<float>(bCount), sumY / static_cast<float>(bCount)};
        }
    }

    board.totalUnits = board.popWarrior + board.popLumberjack + board.popMiner + board.popCourier + board.popBuilder;

    // Estimate next-stage demand: how many houses the next growth cycle will need
    int nextStageHouses = std::max(1, static_cast<int>(std::ceil(
        static_cast<float>(board.totalUnits) / 2.0f / static_cast<float>(config::UNITS_PER_HOUSE))));
    board.nextStageWood = nextStageHouses * config::HOUSE_1.cost.wood;
    board.nextStageRock = nextStageHouses * config::HOUSE_1.cost.rock;

    board.totalHouses = 0;
    auto houseOnlyView = registry.view<ecs::BuildingTag>(entt::exclude<ecs::CityStorageTag>);
    for ([[maybe_unused]] auto e : houseOnlyView) {
        board.totalHouses++;
    }

    float totalDist = 0.0f;
    int distSamples = 0;
    auto taskView = registry.view<ecs::TaskArea>();
    for (auto e : taskView) {
        auto& t = taskView.get<ecs::TaskArea>(e);
        if (t.areas.empty()) continue;

        math::Vec2f hCenter = math::Vec2f{(t.areas[0].startWorld.x + t.areas[0].endWorld.x)/2.0f, (t.areas[0].startWorld.y + t.areas[0].endWorld.y)/2.0f};
        math::Vec2f dCenter = t.hasDropoff ? math::Vec2f{(t.dropoffStart.x + t.dropoffEnd.x)/2.0f, (t.dropoffStart.y + t.dropoffEnd.y)/2.0f} : cityCenter;

        float dx = dCenter.x - hCenter.x;
        float dy = dCenter.y - hCenter.y;
        totalDist += std::sqrt(dx*dx + dy*dy);
        distSamples++;
    }
    board.averageDropoffDistance = distSamples > 0 ? (totalDist / static_cast<float>(distSamples)) : 0.0f;
}

}