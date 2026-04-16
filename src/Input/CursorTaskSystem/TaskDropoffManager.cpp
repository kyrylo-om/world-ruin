#include "../../../../include/Input/CursorTaskSystem/TaskDropoffManager.hpp"
#include "ECS/Tags.hpp"
#include <algorithm>

namespace wr::input {

bool TaskDropoffManager::hasGroundRes(entt::registry& registry, const ecs::PendingTaskArea& pArea) {
    auto resView = registry.view<ecs::WorldPos, ecs::ResourceTag>();
    for (auto e : resView) {
        bool isLog = registry.all_of<ecs::LogTag>(e);
        bool isRock = registry.all_of<ecs::SmallRockTag>(e);
        if (!isLog && !isRock) continue;

        auto& wp = resView.get<ecs::WorldPos>(e);

        bool inDropoff = false;
        if (pArea.hasDropoff) {
            float dMinX = std::min(pArea.dropoffStart.x, pArea.dropoffEnd.x);
            float dMaxX = std::max(pArea.dropoffStart.x, pArea.dropoffEnd.x);
            float dMinY = std::min(pArea.dropoffStart.y, pArea.dropoffEnd.y);
            float dMaxY = std::max(pArea.dropoffStart.y, pArea.dropoffEnd.y);
            if (wp.wx >= dMinX && wp.wx <= dMaxX && wp.wy >= dMinY && wp.wy <= dMaxY) {
                inDropoff = true;
            }
        }
        if (inDropoff) continue;

        bool inBuildingZone = false;
        auto bView = registry.view<ecs::BuildingTag, ecs::WorldPos, ecs::ConstructionData>();
        for (auto bEnt : bView) {
            auto& bWp = bView.get<ecs::WorldPos>(bEnt);
            auto& cData = bView.get<ecs::ConstructionData>(bEnt);
            if (!cData.isBuilt) {
                float bMinX = bWp.wx - cData.resourceZoneWidth / 2.0f;
                float bMaxX = bWp.wx + cData.resourceZoneWidth / 2.0f;
                float bMinY = bWp.wy - cData.resourceZoneHeight / 2.0f;
                float bMaxY = bWp.wy + cData.resourceZoneHeight / 2.0f;
                if (wp.wx >= bMinX && wp.wx <= bMaxX && wp.wy >= bMinY && wp.wy <= bMaxY) {
                    inBuildingZone = true;
                    break;
                }
            }
        }
        if (inBuildingZone) continue;

        for (const auto& rect : pArea.areas) {
            if ((isLog && !rect.includeLogs) || (isRock && !rect.includeSmallRocks)) continue;

            float minX = std::min(rect.startWorld.x, rect.endWorld.x);
            float maxX = std::max(rect.startWorld.x, rect.endWorld.x);
            float minY = std::min(rect.startWorld.y, rect.endWorld.y);
            float maxY = std::max(rect.startWorld.y, rect.endWorld.y);
            if (wp.wx >= minX && wp.wx <= maxX && wp.wy >= minY && wp.wy <= maxY) {
                return true;
            }
        }
    }
    return false;
}

void TaskDropoffManager::trySetDropoffArea(entt::registry& registry, math::Vec2f start, math::Vec2f end) {
    auto view = registry.view<ecs::PendingTaskArea>();
    if (view.empty()) return;
    auto& pArea = view.get<ecs::PendingTaskArea>(view.front());

    if (pArea.collectFutureDrops || hasGroundRes(registry, pArea)) {
        pArea.hasDropoff = true;
        pArea.dropoffStart = start;
        pArea.dropoffEnd = end;
        pArea.useCityStorage = false;
    }
}

}