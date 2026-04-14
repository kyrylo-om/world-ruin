#include "../../include/Input/CursorTaskSystem.hpp"
#include "../../include/Input/CursorTaskSystem/TaskAreaManager.hpp"
#include "../../include/Input/CursorTaskSystem/TaskDropoffManager.hpp"
#include "../../include/Input/CursorTaskSystem/TaskFinalizer.hpp"
#include "ECS/Tags.hpp"
#include <SFML/Window/Keyboard.hpp>
#include <algorithm>

namespace wr::input {

bool CursorTaskSystem::hasPendingTask(entt::registry& registry) const noexcept {
    return !registry.view<ecs::PendingTaskArea>().empty();
}

void CursorTaskSystem::addBuildingTarget(entt::registry& registry, entt::entity building) {
    TaskAreaManager::addBuildingTarget(registry, building);
}

void CursorTaskSystem::addPendingArea(entt::registry& registry, math::Vec2f start, math::Vec2f end) {
    TaskAreaManager::addPendingArea(registry, start, end);
}

void CursorTaskSystem::removeHoveredArea(entt::registry& registry, math::Vec2f hoveredPos) {
    TaskAreaManager::removeHoveredArea(registry, hoveredPos);
}

void CursorTaskSystem::selectHoveredArea(entt::registry& registry, math::Vec2f hoveredPos) {
    TaskAreaManager::selectHoveredArea(registry, hoveredPos);
}

void CursorTaskSystem::trySetDropoffArea(entt::registry& registry, math::Vec2f start, math::Vec2f end) {
    TaskDropoffManager::trySetDropoffArea(registry, start, end);
}

void CursorTaskSystem::update(entt::registry& registry, float dt) {
    bool currentQ = sf::Keyboard::isKeyPressed(sf::Keyboard::Q);
    bool currentF = sf::Keyboard::isKeyPressed(sf::Keyboard::F);
    bool currentEnter = sf::Keyboard::isKeyPressed(sf::Keyboard::Enter);
    bool current1 = sf::Keyboard::isKeyPressed(sf::Keyboard::Num1);
    bool current2 = sf::Keyboard::isKeyPressed(sf::Keyboard::Num2);
    bool current3 = sf::Keyboard::isKeyPressed(sf::Keyboard::Num3);
    bool current4 = sf::Keyboard::isKeyPressed(sf::Keyboard::Num4);
    bool current5 = sf::Keyboard::isKeyPressed(sf::Keyboard::Num5);
    bool isCtrl = sf::Keyboard::isKeyPressed(sf::Keyboard::LControl) || sf::Keyboard::isKeyPressed(sf::Keyboard::RControl);

    if (currentQ && !m_prevQ && !isCtrl) {
        m_taskModeActive = !m_taskModeActive;

        if (!registry.ctx().contains<ecs::TaskModeState>()) {
            registry.ctx().emplace<ecs::TaskModeState>(ecs::TaskModeState{m_taskModeActive});
        } else {
            registry.ctx().get<ecs::TaskModeState>().active = m_taskModeActive;
        }

        if (!m_taskModeActive) {
            TaskFinalizer::cancelTask(registry);
        }
    }

    auto pendingView = registry.view<ecs::PendingTaskArea>();
    for (auto pEnt : pendingView) {
        auto& pArea = pendingView.get<ecs::PendingTaskArea>(pEnt);
        if (pArea.errorTimer > 0.0f) {
            pArea.errorTimer -= dt;
        }
    }

    if (!m_taskModeActive) {
        m_prevQ = currentQ; m_prevF = currentF; m_prevEnter = currentEnter;
        m_prev1 = current1; m_prev2 = current2; m_prev3 = current3;
        m_prev4 = current4; m_prev5 = current5;
        return;
    }

    registry.clear<ecs::PreviewHarvestTag>();

    bool hasCityStorage = !registry.view<ecs::CityStorageTag>().empty();

    if (currentF && !m_prevF && !isCtrl) {
        for (auto e : pendingView) {
            auto& pArea = pendingView.get<ecs::PendingTaskArea>(e);
            bool hasCourier = false;
            auto selectedUnits = registry.view<ecs::SelectedTag, ecs::UnitData>();
            for (auto se : selectedUnits) {
                if (selectedUnits.get<ecs::UnitData>(se).type == ecs::UnitType::Courier) {
                    hasCourier = true;
                    break;
                }
            }
            if (hasCourier) {
                if (pArea.collectFutureDrops) {

                    pArea.collectFutureDrops = false;
                } else {

                    bool hasDestination = pArea.hasDropoff || !pArea.targetBuildings.empty() || hasCityStorage;
                    if (hasDestination) {
                        pArea.collectFutureDrops = true;
                        if (!pArea.hasDropoff && hasCityStorage) {
                            pArea.useCityStorage = true;
                        }
                    }
                }
            }
        }
    }

    for (auto pEnt : pendingView) {
        auto& pArea = pendingView.get<ecs::PendingTaskArea>(pEnt);

        if (pArea.selectedAreaIndex >= 0 && pArea.selectedAreaIndex < static_cast<int>(pArea.areas.size())) {
            auto& rect = pArea.areas[pArea.selectedAreaIndex];
            if (current1 && !m_prev1 && rect.canHarvestTrees) rect.includeTrees = !rect.includeTrees;
            if (current2 && !m_prev2 && rect.canHarvestRocks) rect.includeRocks = !rect.includeRocks;
            if (current3 && !m_prev3 && rect.canHarvestBushes) rect.includeBushes = !rect.includeBushes;
            if (current4 && !m_prev4 && rect.canHarvestLogs) rect.includeLogs = !rect.includeLogs;
            if (current5 && !m_prev5 && rect.canHarvestSmallRocks) rect.includeSmallRocks = !rect.includeSmallRocks;
        } else {
            if (current1 && !m_prev1 && pArea.canHarvestTrees) {
                pArea.includeTrees = !pArea.includeTrees;
                for(auto& r : pArea.areas) r.includeTrees = pArea.includeTrees;
            }
            if (current2 && !m_prev2 && pArea.canHarvestRocks) {
                pArea.includeRocks = !pArea.includeRocks;
                for(auto& r : pArea.areas) r.includeRocks = pArea.includeRocks;
            }
            if (current3 && !m_prev3 && pArea.canHarvestBushes) {
                pArea.includeBushes = !pArea.includeBushes;
                for(auto& r : pArea.areas) r.includeBushes = pArea.includeBushes;
            }
            if (current4 && !m_prev4 && pArea.canHarvestLogs) {
                pArea.includeLogs = !pArea.includeLogs;
                for(auto& r : pArea.areas) r.includeLogs = pArea.includeLogs;
            }
            if (current5 && !m_prev5 && pArea.canHarvestSmallRocks) {
                pArea.includeSmallRocks = !pArea.includeSmallRocks;
                for(auto& r : pArea.areas) r.includeSmallRocks = pArea.includeSmallRocks;
            }
        }

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

    if (currentEnter && !m_prevEnter) {
        if (!pendingView.empty()) {
            auto pEnt = pendingView.front();
            auto& pArea = pendingView.get<ecs::PendingTaskArea>(pEnt);

            bool needsDropoff = pArea.collectFutureDrops || TaskDropoffManager::hasGroundRes(registry, pArea);

            if (needsDropoff && !pArea.hasDropoff && !(hasCityStorage && pArea.useCityStorage)) {
                pArea.errorTimer = 1.0f;
            } else {
                TaskFinalizer::finalizeTask(registry, pArea, m_globalTaskId);
                m_taskModeActive = false;

                if (!registry.ctx().contains<ecs::TaskModeState>()) {
                    registry.ctx().emplace<ecs::TaskModeState>(ecs::TaskModeState{false});
                } else {
                    registry.ctx().get<ecs::TaskModeState>().active = false;
                }

                TaskFinalizer::cancelTask(registry);
            }
        }
    }

    m_prevQ = currentQ;
    m_prevF = currentF;
    m_prevEnter = currentEnter;
    m_prev1 = current1;
    m_prev2 = current2;
    m_prev3 = current3;
    m_prev4 = current4;
    m_prev5 = current5;
}

}