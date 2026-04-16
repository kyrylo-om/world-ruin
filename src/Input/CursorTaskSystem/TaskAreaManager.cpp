#include "../../../../include/Input/CursorTaskSystem/TaskAreaManager.hpp"
#include "ECS/Components.hpp"
#include "ECS/Tags.hpp"
#include <algorithm>

namespace wr::input {

void TaskAreaManager::addBuildingTarget(entt::registry& registry, entt::entity building) {
    auto view = registry.view<ecs::PendingTaskArea>();
    entt::entity pendingEnt = view.empty() ? registry.create() : view.front();
    if (view.empty()) registry.emplace<ecs::PendingTaskArea>(pendingEnt);

    auto& pArea = registry.get<ecs::PendingTaskArea>(pendingEnt);
    if (std::find(pArea.targetBuildings.begin(), pArea.targetBuildings.end(), building) == pArea.targetBuildings.end()) {
        pArea.targetBuildings.push_back(building);
    } else {
        pArea.targetBuildings.erase(std::remove(pArea.targetBuildings.begin(), pArea.targetBuildings.end(), building), pArea.targetBuildings.end());
    }
}

void TaskAreaManager::addPendingArea(entt::registry& registry, math::Vec2f start, math::Vec2f end) {
    auto view = registry.view<ecs::PendingTaskArea>();
    entt::entity pendingEnt = view.empty() ? registry.create() : view.front();
    if (view.empty()) registry.emplace<ecs::PendingTaskArea>(pendingEnt);

    auto& pArea = registry.get<ecs::PendingTaskArea>(pendingEnt);

    bool canTrees = false, canRocks = false, canSmallRocks = false, canBushes = false, canLogs = false;
    auto selectedUnits = registry.view<ecs::SelectedTag, ecs::UnitData>();

    if (selectedUnits.begin() != selectedUnits.end()) {
        for (auto e : selectedUnits) {
            auto uData = selectedUnits.get<ecs::UnitData>(e);
            if (uData.type == ecs::UnitType::Lumberjack) { canTrees = true; canBushes = true; }
            if (uData.type == ecs::UnitType::Miner) { canRocks = true; }
            if (uData.type == ecs::UnitType::Warrior) { canBushes = true; }
            if (uData.type == ecs::UnitType::Courier && uData.heldItem == ecs::HeldItem::None) { canLogs = true; canSmallRocks = true; }
        }
    } else {
        canTrees = true; canRocks = true; canSmallRocks = true; canBushes = true; canLogs = true;
    }

    if (pArea.areas.empty()) {
        pArea.canHarvestTrees = canTrees; pArea.includeTrees = canTrees;
        pArea.canHarvestRocks = canRocks; pArea.includeRocks = canRocks;
        pArea.canHarvestSmallRocks = canSmallRocks; pArea.includeSmallRocks = canSmallRocks;
        pArea.canHarvestBushes = canBushes; pArea.includeBushes = canBushes;
        pArea.canHarvestLogs = canLogs; pArea.includeLogs = canLogs;
    }

    ecs::RectArea newRect{start, end, canTrees, canRocks, canSmallRocks, canBushes, canLogs,
                          pArea.includeTrees, pArea.includeRocks, pArea.includeSmallRocks, pArea.includeBushes, pArea.includeLogs};
    pArea.areas.push_back(newRect);
}

void TaskAreaManager::removeHoveredArea(entt::registry& registry, math::Vec2f hoveredPos) {
    auto view = registry.view<ecs::PendingTaskArea>();
    for (auto e : view) {
        auto& pArea = view.get<ecs::PendingTaskArea>(e);

        if (pArea.hasDropoff) {
            float dMinX = std::min(pArea.dropoffStart.x, pArea.dropoffEnd.x);
            float dMaxX = std::max(pArea.dropoffStart.x, pArea.dropoffEnd.x);
            float dMinY = std::min(pArea.dropoffStart.y, pArea.dropoffEnd.y);
            float dMaxY = std::max(pArea.dropoffStart.y, pArea.dropoffEnd.y);

            if (hoveredPos.x >= dMinX && hoveredPos.x <= dMaxX && hoveredPos.y >= dMinY && hoveredPos.y <= dMaxY) {
                pArea.hasDropoff = false;
                if (!registry.view<ecs::CityStorageTag>().empty()) {
                    pArea.useCityStorage = true;
                }
                continue;
            }
        }

        int removedIdx = -1;
        for (size_t i = 0; i < pArea.areas.size(); ++i) {
            auto& rect = pArea.areas[i];
            float minX = std::min(rect.startWorld.x, rect.endWorld.x);
            float maxX = std::max(rect.startWorld.x, rect.endWorld.x);
            float minY = std::min(rect.startWorld.y, rect.endWorld.y);
            float maxY = std::max(rect.startWorld.y, rect.endWorld.y);

            if (hoveredPos.x >= minX && hoveredPos.x <= maxX && hoveredPos.y >= minY && hoveredPos.y <= maxY) {
                removedIdx = i;
                pArea.areas.erase(pArea.areas.begin() + i);
                break;
            }
        }

        if (removedIdx != -1) {
            if (pArea.selectedAreaIndex == removedIdx) pArea.selectedAreaIndex = -1;
            else if (pArea.selectedAreaIndex > removedIdx) pArea.selectedAreaIndex--;
        }
    }
}

void TaskAreaManager::selectHoveredArea(entt::registry& registry, math::Vec2f hoveredPos) {
    auto view = registry.view<ecs::PendingTaskArea>();
    for (auto e : view) {
        auto& pArea = view.get<ecs::PendingTaskArea>(e);
        int foundIdx = -1;

        for (size_t i = 0; i < pArea.areas.size(); ++i) {
            auto& rect = pArea.areas[i];
            float minX = std::min(rect.startWorld.x, rect.endWorld.x);
            float maxX = std::max(rect.startWorld.x, rect.endWorld.x);
            float minY = std::min(rect.startWorld.y, rect.endWorld.y);
            float maxY = std::max(rect.startWorld.y, rect.endWorld.y);

            if (hoveredPos.x >= minX && hoveredPos.x <= maxX && hoveredPos.y >= minY && hoveredPos.y <= maxY) {
                foundIdx = static_cast<int>(i);
                break;
            }
        }

        if (pArea.selectedAreaIndex == foundIdx) {
            pArea.selectedAreaIndex = -1;
        } else {
            pArea.selectedAreaIndex = foundIdx;
        }
    }
}

}