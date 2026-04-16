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
                    pArea.collectFutureDrops = true;
                    if (!pArea.hasDropoff && hasCityStorage) {
                        pArea.useCityStorage = true;
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

        TaskFinalizer::scanResources(registry, pArea);
    }

    if (currentEnter && !m_prevEnter) {
        if (!pendingView.empty()) {
            auto pEnt = pendingView.front();
            auto& pArea = pendingView.get<ecs::PendingTaskArea>(pEnt);

            bool needsDropoff = pArea.collectFutureDrops || TaskDropoffManager::hasGroundRes(registry, pArea);

            if (needsDropoff && !pArea.hasDropoff && !(hasCityStorage && pArea.useCityStorage)) {
                pArea.errorTimer = 1.0f;
            } else {
                std::vector<entt::entity> workers;
                auto selectedUnits = registry.view<ecs::SelectedTag, ecs::UnitData, ecs::WorkerState>();
                for (auto e : selectedUnits) workers.push_back(e);

                TaskFinalizer::finalizeTask(registry, pArea, m_globalTaskId, workers);
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