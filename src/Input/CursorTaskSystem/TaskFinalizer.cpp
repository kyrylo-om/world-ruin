#include "../../../../include/Input/CursorTaskSystem/TaskFinalizer.hpp"
#include "ECS/Tags.hpp"
#include <algorithm>

namespace wr::input {

void TaskFinalizer::cancelTask(entt::registry& registry) {
    registry.clear<ecs::PreviewHarvestTag>();
    registry.clear<ecs::PendingTaskArea>();
}

void TaskFinalizer::scanResources(entt::registry& registry, const ecs::PendingTaskArea& pArea) {
    auto resView = registry.view<ecs::WorldPos, ecs::ResourceTag>();
    for (auto e : resView) {
        auto& wp = resView.get<ecs::WorldPos>(e);
        bool match = false;

        for (const auto& rect : pArea.areas) {
            float minX = std::min(rect.startWorld.x, rect.endWorld.x);
            float maxX = std::max(rect.startWorld.x, rect.endWorld.x);
            float minY = std::min(rect.startWorld.y, rect.endWorld.y);
            float maxY = std::max(rect.startWorld.y, rect.endWorld.y);

            if (wp.wx >= minX && wp.wx <= maxX && wp.wy >= minY && wp.wy <= maxY) {
                if (rect.includeTrees && registry.all_of<ecs::TreeTag>(e)) match = true;
                if (rect.includeRocks && registry.all_of<ecs::RockTag>(e)) match = true;
                if (rect.includeSmallRocks && registry.all_of<ecs::SmallRockTag>(e)) match = true;
                if (rect.includeBushes && registry.all_of<ecs::BushTag>(e)) match = true;
                if (rect.includeLogs && registry.all_of<ecs::LogTag>(e)) match = true;
                if (match) break;
            }
        }

        if (match && pArea.hasDropoff) {
            float dMinX = std::min(pArea.dropoffStart.x, pArea.dropoffEnd.x);
            float dMaxX = std::max(pArea.dropoffStart.x, pArea.dropoffEnd.x);
            float dMinY = std::min(pArea.dropoffStart.y, pArea.dropoffEnd.y);
            float dMaxY = std::max(pArea.dropoffStart.y, pArea.dropoffEnd.y);
            if (wp.wx >= dMinX && wp.wx <= dMaxX && wp.wy >= dMinY && wp.wy <= dMaxY) {
                match = false;
            }
        }

        if (match) {
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
                        match = false;
                        break;
                    }
                }
            }
        }

        if (match) registry.emplace<ecs::PreviewHarvestTag>(e, sf::Color(255, 255, 255, 255));
    }
}

entt::entity TaskFinalizer::finalizeTask(entt::registry& registry, ecs::PendingTaskArea& pArea,
                                          int& globalTaskId, const std::vector<entt::entity>& workers) {
    auto previewView = registry.view<ecs::PreviewHarvestTag>();
    bool markedAny = false;

    int wood = 0, rock = 0, smallRock = 0, bush = 0, log = 0;
    for (auto e : previewView) {
        if (registry.all_of<ecs::TreeTag>(e)) wood++;
        if (registry.all_of<ecs::RockTag>(e)) rock++;
        if (registry.all_of<ecs::SmallRockTag>(e)) smallRock++;
        if (registry.all_of<ecs::BushTag>(e)) bush++;
        if (registry.all_of<ecs::LogTag>(e)) log++;
    }

    sf::Color randomColor(rand() % 155 + 100, rand() % 155 + 100, rand() % 155 + 100, 80);
    sf::Color solidColor(randomColor.r, randomColor.g, randomColor.b, 255);

    auto areaEntity = registry.create();
    registry.emplace<ecs::TaskArea>(areaEntity, pArea.areas, pArea.targetBuildings, randomColor, globalTaskId++,
                                    pArea.hasDropoff, pArea.dropoffStart, pArea.dropoffEnd, pArea.collectFutureDrops, pArea.useCityStorage);

    for (auto e : previewView) {
        registry.emplace_or_replace<ecs::MarkedForHarvestTag>(e, solidColor, areaEntity);
        markedAny = true;
    }

    bool hasHarvestWork = wood > 0 || rock > 0 || bush > 0;
    bool courierOnlyTask = !hasHarvestWork && (pArea.collectFutureDrops || log > 0 || smallRock > 0 || !pArea.targetBuildings.empty());

    if (markedAny || pArea.hasDropoff || !pArea.targetBuildings.empty() || courierOnlyTask) {
        for (auto e : workers) {
            if (!registry.valid(e)) continue;
            auto& uData = registry.get<ecs::UnitData>(e);
            bool canHelp = false;

            if (courierOnlyTask) {
                if (uData.type == ecs::UnitType::Courier) {
                    bool hasDestination = pArea.hasDropoff || !pArea.targetBuildings.empty() || pArea.useCityStorage;
                    if (hasDestination) canHelp = true;
                }
            } else {
                if (uData.type == ecs::UnitType::Lumberjack && (wood > 0 || bush > 0)) canHelp = true;
                if (uData.type == ecs::UnitType::Miner && rock > 0) canHelp = true;
                if (uData.type == ecs::UnitType::Warrior && bush > 0) canHelp = true;
                if (uData.type == ecs::UnitType::Courier) {
                    bool hasDestination = pArea.hasDropoff || !pArea.targetBuildings.empty() || pArea.useCityStorage;
                    if (hasDestination && (pArea.collectFutureDrops || log > 0 || smallRock > 0 || !pArea.targetBuildings.empty()))
                        canHelp = true;
                }
            }

            if (canHelp) {
                registry.get<ecs::WorkerState>(e).currentTask = areaEntity;
                registry.emplace_or_replace<ecs::WorkerTag>(e);
                registry.remove<ecs::IdleTag>(e);
            }
        }
    } else {
        registry.destroy(areaEntity);
        return entt::null;
    }

    return areaEntity;
}

}
