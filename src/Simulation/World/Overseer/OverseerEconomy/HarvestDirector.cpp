#include "Simulation/World/OverseerSystem.hpp"
#include "ECS/Tags.hpp"
#include "Config/BuildingsConfig.hpp"
#include <cmath>
#include <iostream>
#include <vector>

namespace wr::simulation {

struct ExclusionZone {
    float minX, maxX, minY, maxY;
};

void OverseerSystem::manageResources(entt::registry& registry, world::ChunkManager& chunkManager) {
    float currentTime = m_internalClock.getElapsedTime().asSeconds();
    if (currentTime - m_lastScanTime < 2.5f) return;
    m_lastScanTime = currentTime;

    auto& board = registry.ctx().get<SimBlackboard>();

    int activeWoodTasks = 0;
    int activeRockTasks = 0;
    auto taskView = registry.view<ecs::TaskArea>();
    for(auto t : taskView) {
        auto& ta = taskView.get<ecs::TaskArea>(t);
        if (ta.color == sf::Color(50, 200, 50, 80)) activeWoodTasks++;
        else if (ta.color == sf::Color(150, 150, 150, 80)) activeRockTasks++;
    }

    auto dispatchHarvest = [&](bool forWood) {

        if (forWood && activeWoodTasks >= board.popLumberjack) return;
        if (!forWood && activeRockTasks >= board.popMiner) return;

        std::vector<entt::entity> capableIdleWorkers;
        auto potentialWorkers = registry.view<ecs::UnitData, ecs::WorkerState>();
        for (auto w : potentialWorkers) {
            auto& uData = potentialWorkers.get<ecs::UnitData>(w);
            auto& wState = potentialWorkers.get<ecs::WorkerState>(w);

            bool isIdle = !registry.all_of<ecs::WorkerTag>(w) &&
                          (wState.currentTask == entt::null && !registry.all_of<ecs::PathRequest>(w) && !registry.all_of<ecs::Path>(w));

            if (isIdle) {
                if (forWood && uData.type == ecs::UnitType::Lumberjack) capableIdleWorkers.push_back(w);
                if (!forWood && uData.type == ecs::UnitType::Miner) capableIdleWorkers.push_back(w);
            }
        }

        if (capableIdleWorkers.empty()) return;

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

        math::Vec2f searchOrigin{0.0f, 0.0f};
        for (auto e : capableIdleWorkers) {
            auto& wp = registry.get<ecs::WorldPos>(e);
            searchOrigin.x += wp.wx; searchOrigin.y += wp.wy;
        }
        searchOrigin.x /= capableIdleWorkers.size(); searchOrigin.y /= capableIdleWorkers.size();

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

        float boxRadius = 6.0f;
        ecs::RectArea rect{
            math::Vec2f{bestPos.x - boxRadius, bestPos.y - boxRadius},
            math::Vec2f{bestPos.x + boxRadius, bestPos.y + boxRadius},
            forWood, !forWood, false, false, false, forWood, !forWood, false, false, false
        };

        std::vector<ecs::RectArea> areas = {rect};
        std::vector<entt::entity> bldgs;
        sf::Color taskColor = forWood ? sf::Color(50, 200, 50, 80) : sf::Color(150, 150, 150, 80);

        auto taskEnt = registry.create();
        registry.emplace<ecs::TaskArea>(taskEnt, areas, bldgs, taskColor, m_globalTaskId++, false,
            math::Vec2f{0.0f, 0.0f}, math::Vec2f{0.0f, 0.0f}, false, false);

        sf::Color solidColor = forWood ? sf::Color(50, 200, 50, 255) : sf::Color(150, 150, 150, 255);
        int markedCount = 0;

        if (forWood) {
            auto trees = registry.view<ecs::TreeTag, ecs::WorldPos>(entt::exclude<ecs::MarkedForHarvestTag>);
            for (auto e : trees) {
                auto& wp = trees.get<ecs::WorldPos>(e);
                if (wp.wx >= rect.startWorld.x && wp.wx <= rect.endWorld.x && wp.wy >= rect.startWorld.y && wp.wy <= rect.endWorld.y) {
                    registry.emplace<ecs::MarkedForHarvestTag>(e, solidColor, taskEnt);
                    markedCount++;
                }
            }
        } else {
            auto rocks = registry.view<ecs::RockTag, ecs::WorldPos>(entt::exclude<ecs::MarkedForHarvestTag>);
            for (auto e : rocks) {
                auto& wp = rocks.get<ecs::WorldPos>(e);
                if (wp.wx >= rect.startWorld.x && wp.wx <= rect.endWorld.x && wp.wy >= rect.startWorld.y && wp.wy <= rect.endWorld.y) {
                    registry.emplace<ecs::MarkedForHarvestTag>(e, solidColor, taskEnt);
                    markedCount++;
                }
            }
        }

        if (markedCount == 0) {
            registry.destroy(taskEnt);
        } else {
            int maxWorkers = std::max(1, markedCount / 2);
            int assignedCount = 0;

            for (auto w : capableIdleWorkers) {
                if (assignedCount >= maxWorkers) break;

                registry.get<ecs::WorkerState>(w).currentTask = taskEnt;
                registry.emplace_or_replace<ecs::WorkerTag>(w);
                registry.remove<ecs::IdleTag>(w);
                assignedCount++;
            }
            std::cout << "[Overseer] Dispatched Harvester Task " << (m_globalTaskId - 1) << " with " << assignedCount << " workers." << std::endl;
        }
    };

    if (board.availWood < board.demandWood + 20 && board.popLumberjack > 0) dispatchHarvest(true);
    if (board.availRock < board.demandRock + 15 && board.popMiner > 0) dispatchHarvest(false);
}

}