#include "Simulation/World/OverseerSystem.hpp"
#include "ECS/Tags.hpp"
#include "Config/BuildingsConfig.hpp"
#include <cmath>
#include <iostream>
#include <vector>
#include <algorithm>

namespace wr::simulation {

struct ExclusionZone {
    float minX, maxX, minY, maxY;
};

struct BuildingDemand {
    entt::entity bEnt;
    int netWood;
    int netRock;
};

void OverseerSystem::manageLogistics(entt::registry& registry, world::ChunkManager& chunkManager) {
    float currentTime = m_internalClock.getElapsedTime().asSeconds();
    if (currentTime - m_lastLogisticsTime < 2.0f) return;
    m_lastLogisticsTime = currentTime;

    auto& board = registry.ctx().get<SimBlackboard>();

    std::vector<entt::entity> idleCouriers;
    auto potentialWorkers = registry.view<ecs::UnitData, ecs::WorkerState>();
    for (auto w : potentialWorkers) {
        auto& uData = potentialWorkers.get<ecs::UnitData>(w);
        auto& wState = potentialWorkers.get<ecs::WorkerState>(w);

        bool isIdle = !registry.all_of<ecs::WorkerTag>(w) &&
                      (wState.currentTask == entt::null && !registry.all_of<ecs::PathRequest>(w) && !registry.all_of<ecs::Path>(w));

        if (isIdle && uData.type == ecs::UnitType::Courier) {
            idleCouriers.push_back(w);
        }
    }

    int activeLogisticsTasks = 0;
    auto taskView = registry.view<ecs::TaskArea>();
    for (auto t : taskView) {
        auto& ta = taskView.get<ecs::TaskArea>(t);
        if (ta.color == sf::Color(50, 150, 255, 80)) {
            activeLogisticsTasks++;
            int activeWorkers = 0;
            for (auto w : potentialWorkers) {
                if (potentialWorkers.get<ecs::WorkerState>(w).currentTask == t) activeWorkers++;
            }

            int markedResources = 0;
            auto markedView = registry.view<ecs::MarkedForHarvestTag>();
            for (auto m : markedView) {
                if (markedView.get<ecs::MarkedForHarvestTag>(m).taskEntity == t) markedResources++;
            }

            int requiredWorkers = std::max(1, markedResources / 2);
            int needed = requiredWorkers - activeWorkers;

            while (needed > 0 && !idleCouriers.empty()) {
                entt::entity c = idleCouriers.back();
                idleCouriers.pop_back();
                registry.get<ecs::WorkerState>(c).currentTask = t;
                registry.emplace_or_replace<ecs::WorkerTag>(c);
                registry.remove<ecs::IdleTag>(c);
                needed--;
            }
        }
    }

    if (idleCouriers.empty() || activeLogisticsTasks >= board.popCourier) return;

    ExclusionZone hubZone = {0,0,0,0};
    bool hasHub = false;
    math::Vec2f hubDropStart{0,0}, hubDropEnd{0,0};

    auto storageView = registry.view<ecs::CityStorageTag, ecs::WorldPos, ecs::ConstructionData>();
    for (auto s : storageView) {
        auto& sWp = storageView.get<ecs::WorldPos>(s);
        auto& cData = storageView.get<ecs::ConstructionData>(s);
        float rw = cData.resourceZoneWidth / 2.0f;
        float rh = cData.resourceZoneHeight / 2.0f;

        hubZone = { sWp.wx - rw, sWp.wx + rw, sWp.wy - rh, sWp.wy + rh };
        hubDropStart = { hubZone.minX, hubZone.minY };
        hubDropEnd = { hubZone.maxX, hubZone.maxY };
        hasHub = true;
        break;
    }

    auto bView = registry.view<ecs::BuildingTag, ecs::WorldPos, ecs::ConstructionData>(entt::exclude<ecs::CityStorageTag>);
    if (!hasHub) {
        math::Vec2f cityCenter{10.0f, 10.0f};
        if (bView.begin() != bView.end()) {
            float sumX = 0, sumY = 0; int bCount = 0;
            for (auto e : bView) { sumX += bView.get<ecs::WorldPos>(e).wx; sumY += bView.get<ecs::WorldPos>(e).wy; bCount++; }
            cityCenter = math::Vec2f{sumX / static_cast<float>(bCount), sumY / static_cast<float>(bCount)};
        }
        float dropRad = 2.0f;
        hubZone = { cityCenter.x - dropRad, cityCenter.x + dropRad, cityCenter.y - dropRad, cityCenter.y + dropRad };
        hubDropStart = { hubZone.minX, hubZone.minY };
        hubDropEnd = { hubZone.maxX, hubZone.maxY };
    }

    std::vector<ExclusionZone> buildingZones;
    std::vector<BuildingDemand> activeDemands;
    auto logView = registry.view<ecs::LogTag, ecs::WorldPos>();
    auto rockView = registry.view<ecs::SmallRockTag, ecs::WorldPos>();
    float buffer = 1.0f;

    buildingZones.push_back({ hubZone.minX - buffer, hubZone.maxX + buffer, hubZone.minY - buffer, hubZone.maxY + buffer });

    for (auto b : bView) {
        auto& cData = bView.get<ecs::ConstructionData>(b);
        if (!cData.isBuilt) {
            auto& bWp = bView.get<ecs::WorldPos>(b);
            float minX = bWp.wx - cData.resourceZoneWidth / 2.0f;
            float maxX = bWp.wx + cData.resourceZoneWidth / 2.0f;
            float minY = bWp.wy - cData.resourceZoneHeight / 2.0f;
            float maxY = bWp.wy + cData.resourceZoneHeight / 2.0f;

            buildingZones.push_back({ minX - buffer, maxX + buffer, minY - buffer, maxY + buffer });

            int woodInZone = 0, rockInZone = 0;
            for (auto r : logView) {
                auto& rWp = logView.get<ecs::WorldPos>(r);
                if (rWp.wx >= minX && rWp.wx <= maxX && rWp.wy >= minY && rWp.wy <= maxY) woodInZone++;
            }
            for (auto r : rockView) {
                auto& rWp = rockView.get<ecs::WorldPos>(r);
                if (rWp.wx >= minX && rWp.wx <= maxX && rWp.wy >= minY && rWp.wy <= maxY) rockInZone++;
            }

            std::vector<entt::entity> bTasks;
            for (auto t : taskView) {
                auto& ta = taskView.get<ecs::TaskArea>(t);
                if (std::find(ta.targetBuildings.begin(), ta.targetBuildings.end(), b) != ta.targetBuildings.end()) {
                    bTasks.push_back(t);
                }
            }

            int woodInTransit = 0, rockInTransit = 0;
            for (auto w : potentialWorkers) {
                auto& uData = potentialWorkers.get<ecs::UnitData>(w);
                auto& wState = potentialWorkers.get<ecs::WorkerState>(w);
                if (uData.type == ecs::UnitType::Courier && std::find(bTasks.begin(), bTasks.end(), wState.currentTask) != bTasks.end()) {
                    if (uData.heldItem == ecs::HeldItem::Wood) woodInTransit++;
                    else if (uData.heldItem == ecs::HeldItem::Rock) rockInTransit++;
                }
            }

            int netWood = cData.woodRequired - (woodInZone + woodInTransit);
            int netRock = cData.rockRequired - (rockInZone + rockInTransit);

            if (netWood > 0 || netRock > 0) activeDemands.push_back({b, std::max(0, netWood), std::max(0, netRock)});
        }
    }

    auto inBuildingZone = [&](float x, float y) {
        for (const auto& z : buildingZones) {
            if (x >= z.minX && x <= z.maxX && y >= z.minY && y <= z.maxY) return true;
        }
        return false;
    };

    auto inHubZone = [&](float x, float y) {
        return (x >= hubZone.minX && x <= hubZone.maxX && y >= hubZone.minY && y <= hubZone.maxY);
    };

    auto logs = registry.view<ecs::LogTag, ecs::WorldPos>(entt::exclude<ecs::MarkedForHarvestTag>);
    auto rocks = registry.view<ecs::SmallRockTag, ecs::WorldPos>(entt::exclude<ecs::MarkedForHarvestTag>);

    for (auto& dem : activeDemands) {
        if (idleCouriers.empty() || activeLogisticsTasks >= board.popCourier) break;

        bool needsWood = dem.netWood > 0;
        bool needsRock = dem.netRock > 0;

        entt::entity targetRes = entt::null;
        math::Vec2f resPos;
        bool targetingWood = true;

        if (needsWood) {
            for (auto e : logs) {
                auto& wp = logs.get<ecs::WorldPos>(e);
                if (inHubZone(wp.wx, wp.wy)) { targetRes = e; resPos = {wp.wx, wp.wy}; targetingWood = true; break; }
            }
        }
        if (targetRes == entt::null && needsRock) {
            for (auto e : rocks) {
                auto& wp = rocks.get<ecs::WorldPos>(e);
                if (inHubZone(wp.wx, wp.wy)) { targetRes = e; resPos = {wp.wx, wp.wy}; targetingWood = false; break; }
            }
        }

        if (targetRes != entt::null) {
            auto& bWp = registry.get<ecs::WorldPos>(dem.bEnt);
            auto& cData = registry.get<ecs::ConstructionData>(dem.bEnt);
            math::Vec2f dropStart{bWp.wx - cData.resourceZoneWidth / 2.0f, bWp.wy - cData.resourceZoneHeight / 2.0f};
            math::Vec2f dropEnd{bWp.wx + cData.resourceZoneWidth / 2.0f, bWp.wy + cData.resourceZoneHeight / 2.0f};

            ecs::RectArea rect{
                math::Vec2f{resPos.x - 2.0f, resPos.y - 2.0f}, math::Vec2f{resPos.x + 2.0f, resPos.y + 2.0f},
                false, false, !targetingWood, false, targetingWood, false, false, !targetingWood, false, targetingWood
            };

            auto taskEnt = registry.create();
            registry.emplace<ecs::TaskArea>(taskEnt, std::vector<ecs::RectArea>{rect}, std::vector<entt::entity>{dem.bEnt}, sf::Color(50, 150, 255, 80), m_globalTaskId++, true, dropStart, dropEnd, true, true);
            activeLogisticsTasks++;

            sf::Color solidColor(50, 150, 255, 255);
            int markedCount = 0;
            auto markFunc = [&](auto& view) {
                for (auto e : view) {
                    auto& wp = view.template get<ecs::WorldPos>(e);
                    if (inHubZone(wp.wx, wp.wy) && wp.wx >= rect.startWorld.x && wp.wx <= rect.endWorld.x && wp.wy >= rect.startWorld.y && wp.wy <= rect.endWorld.y) {
                        registry.emplace<ecs::MarkedForHarvestTag>(e, solidColor, taskEnt);
                        markedCount++;
                    }
                }
            };

            if (targetingWood) markFunc(logs); else markFunc(rocks);

            if (markedCount > 0) {
                int maxWorkers = std::max(1, markedCount / 2);
                int assignedCount = 0;
                while (assignedCount < maxWorkers && !idleCouriers.empty()) {
                    entt::entity w = idleCouriers.back();
                    idleCouriers.pop_back();
                    registry.get<ecs::WorkerState>(w).currentTask = taskEnt;
                    registry.emplace_or_replace<ecs::WorkerTag>(w);
                    registry.remove<ecs::IdleTag>(w);
                    assignedCount++;
                }
                if (targetingWood) dem.netWood -= markedCount; else dem.netRock -= markedCount;
            } else {
                registry.destroy(taskEnt);
                activeLogisticsTasks--;
            }
        }
    }

    while (!idleCouriers.empty() && activeLogisticsTasks < board.popCourier) {
        entt::entity seedCourier = idleCouriers.back();
        auto& cWp = registry.get<ecs::WorldPos>(seedCourier);

        entt::entity bestRes = entt::null;
        float bestDistSq = 9999999.0f;
        math::Vec2f bestPos;
        bool isWoodTarget = true;

        for (auto e : logs) {
            auto& wp = logs.get<ecs::WorldPos>(e);
            if (inBuildingZone(wp.wx, wp.wy)) continue;
            float dSq = (wp.wx - cWp.wx)*(wp.wx - cWp.wx) + (wp.wy - cWp.wy)*(wp.wy - cWp.wy);
            if (dSq < bestDistSq) { bestDistSq = dSq; bestRes = e; bestPos = {wp.wx, wp.wy}; isWoodTarget = true; }
        }
        for (auto e : rocks) {
            auto& wp = rocks.get<ecs::WorldPos>(e);
            if (inBuildingZone(wp.wx, wp.wy)) continue;
            float dSq = (wp.wx - cWp.wx)*(wp.wx - cWp.wx) + (wp.wy - cWp.wy)*(wp.wy - cWp.wy);
            if (dSq < bestDistSq) { bestDistSq = dSq; bestRes = e; bestPos = {wp.wx, wp.wy}; isWoodTarget = false; }
        }

        if (bestRes == entt::null) break;

        float boxRadius = 6.0f;
        ecs::RectArea rect{
            math::Vec2f{bestPos.x - boxRadius, bestPos.y - boxRadius}, math::Vec2f{bestPos.x + boxRadius, bestPos.y + boxRadius},
            false, false, !isWoodTarget, false, isWoodTarget, false, false, !isWoodTarget, false, isWoodTarget
        };

        auto taskEnt = registry.create();
        registry.emplace<ecs::TaskArea>(taskEnt, std::vector<ecs::RectArea>{rect}, std::vector<entt::entity>{}, sf::Color(50, 150, 255, 80), m_globalTaskId++, true, hubDropStart, hubDropEnd, true, true);
        activeLogisticsTasks++;

        sf::Color solidColor(50, 150, 255, 255);
        int markedCount = 0;

        auto markFunc = [&](auto& view) {
            for (auto e : view) {
                auto& wp = view.template get<ecs::WorldPos>(e);
                if (inBuildingZone(wp.wx, wp.wy)) continue;
                if (wp.wx >= rect.startWorld.x && wp.wx <= rect.endWorld.x && wp.wy >= rect.startWorld.y && wp.wy <= rect.endWorld.y) {
                    registry.emplace<ecs::MarkedForHarvestTag>(e, solidColor, taskEnt);
                    markedCount++;
                }
            }
        };

        if (isWoodTarget) markFunc(logs); else markFunc(rocks);

        if (markedCount == 0) {
            registry.destroy(taskEnt);
            activeLogisticsTasks--;
            idleCouriers.pop_back();
            continue;
        }

        std::sort(idleCouriers.begin(), idleCouriers.end(), [&](entt::entity a, entt::entity b) {
            auto& wa = registry.get<ecs::WorldPos>(a);
            auto& wb = registry.get<ecs::WorldPos>(b);
            float da = (wa.wx - bestPos.x)*(wa.wx - bestPos.x) + (wa.wy - bestPos.y)*(wa.wy - bestPos.y);
            float db = (wb.wx - bestPos.x)*(wb.wx - bestPos.x) + (wb.wy - bestPos.y)*(wb.wy - bestPos.y);
            return da > db;
        });

        int maxWorkers = std::max(1, markedCount / 2);
        int assignedCount = 0;

        while (assignedCount < maxWorkers && !idleCouriers.empty()) {
            entt::entity w = idleCouriers.back();
            idleCouriers.pop_back();
            registry.get<ecs::WorkerState>(w).currentTask = taskEnt;
            registry.emplace_or_replace<ecs::WorkerTag>(w);
            registry.remove<ecs::IdleTag>(w);
            assignedCount++;
        }
    }
}

}