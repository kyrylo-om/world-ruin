#include "Input/CursorTaskSystem.hpp"
#include "ECS/Components.hpp"
#include "ECS/Tags.hpp"
#include <SFML/Window/Keyboard.hpp>
#include <algorithm>

namespace wr::input {

void CursorTaskSystem::setDropoffArea(entt::registry& registry, math::Vec2f start, math::Vec2f end) {
    m_hasDropoff = true;
    m_dropoffStart = start;
    m_dropoffEnd = end;
    m_isSelectingDropoff = false;

    auto view = registry.view<ecs::PendingTaskArea>();
    for (auto e : view) {
        auto& area = view.get<ecs::PendingTaskArea>(e);
        area.hasDropoff = true;
        area.dropoffStart = start;
        area.dropoffEnd = end;
        area.isSelectingDropoff = false;
    }
}

void CursorTaskSystem::startPendingTask(entt::registry& registry, math::Vec2f start, math::Vec2f end) {
    m_hasPendingTask = true;
    m_isSelectingDropoff = false;
    m_hasDropoff = false;
    m_pendingStart = start;
    m_pendingEnd = end;

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

    auto pendingEnt = registry.create();
    registry.emplace<ecs::PendingTaskArea>(pendingEnt, m_pendingStart, m_pendingEnd,
                                           canTrees, canRocks, canSmallRocks, canBushes, canLogs,
                                           canTrees, canRocks, canSmallRocks, canBushes, canLogs,
                                           false, false, math::Vec2f{0,0}, math::Vec2f{0,0}, false);
}

void CursorTaskSystem::cancelTask(entt::registry& registry) {
    m_hasPendingTask = false;
    m_isSelectingDropoff = false;
    m_hasDropoff = false;
    registry.clear<ecs::PreviewHarvestTag>();
    registry.clear<ecs::PendingTaskArea>();
}

void CursorTaskSystem::update(entt::registry& registry) {
    bool currentQ = sf::Keyboard::isKeyPressed(sf::Keyboard::Q);
    bool currentF = sf::Keyboard::isKeyPressed(sf::Keyboard::F);
    bool currentEnter = sf::Keyboard::isKeyPressed(sf::Keyboard::Enter);

    bool current1 = sf::Keyboard::isKeyPressed(sf::Keyboard::Num1);
    bool current2 = sf::Keyboard::isKeyPressed(sf::Keyboard::Num2);
    bool current3 = sf::Keyboard::isKeyPressed(sf::Keyboard::Num3);
    bool current4 = sf::Keyboard::isKeyPressed(sf::Keyboard::Num4);
    bool current5 = sf::Keyboard::isKeyPressed(sf::Keyboard::Num5);

    bool isCtrl = sf::Keyboard::isKeyPressed(sf::Keyboard::LControl) || sf::Keyboard::isKeyPressed(sf::Keyboard::RControl);

    bool exitCommand = isCtrl && currentQ;

    if (!m_hasPendingTask) {
        m_prevQ = currentQ;
        m_prevF = currentF;
        m_prevEnter = currentEnter;
        m_prev1 = current1;
        m_prev2 = current2;
        m_prev3 = current3;
        m_prev4 = current4;
        m_prev5 = current5;
        return;
    }

    registry.clear<ecs::PreviewHarvestTag>();

    if (exitCommand && !m_prevQ) {
        if (m_hasDropoff || m_isSelectingDropoff) {
            m_hasDropoff = false;
            m_isSelectingDropoff = false;
            auto view = registry.view<ecs::PendingTaskArea>();
            for (auto e : view) {
                auto& area = view.get<ecs::PendingTaskArea>(e);
                area.hasDropoff = false;
                area.isSelectingDropoff = false;
            }
        } else {
            cancelTask(registry);
        }
        m_prevQ = currentQ;
        return;
    }

    if (currentQ && !m_prevQ && !isCtrl) {
        m_isSelectingDropoff = !m_isSelectingDropoff;
        auto view = registry.view<ecs::PendingTaskArea>();
        for (auto e : view) {
            view.get<ecs::PendingTaskArea>(e).isSelectingDropoff = m_isSelectingDropoff;
        }
    }

    if (currentF && !m_prevF && !isCtrl) {
        auto view = registry.view<ecs::PendingTaskArea>();
        for (auto e : view) {
            auto& pArea = view.get<ecs::PendingTaskArea>(e);
            bool hasCourier = false;
            auto selectedUnits = registry.view<ecs::SelectedTag, ecs::UnitData>();
            for (auto se : selectedUnits) {
                if (selectedUnits.get<ecs::UnitData>(se).type == ecs::UnitType::Courier) {
                    hasCourier = true;
                    break;
                }
            }
            if (hasCourier) {
                pArea.collectFutureDrops = !pArea.collectFutureDrops;
            }
        }
    }

    auto pendingView = registry.view<ecs::PendingTaskArea>();
    for (auto pEnt : pendingView) {
        auto& pArea = pendingView.get<ecs::PendingTaskArea>(pEnt);

        if (current1 && !m_prev1 && pArea.canHarvestTrees) pArea.includeTrees = !pArea.includeTrees;
        if (current2 && !m_prev2 && pArea.canHarvestRocks) pArea.includeRocks = !pArea.includeRocks;
        if (current3 && !m_prev3 && pArea.canHarvestBushes) pArea.includeBushes = !pArea.includeBushes;
        if (current4 && !m_prev4 && pArea.canHarvestLogs) pArea.includeLogs = !pArea.includeLogs;
        if (current5 && !m_prev5 && pArea.canHarvestSmallRocks) pArea.includeSmallRocks = !pArea.includeSmallRocks;

        float minX = std::min(pArea.startWorld.x, pArea.endWorld.x);
        float maxX = std::max(pArea.startWorld.x, pArea.endWorld.x);
        float minY = std::min(pArea.startWorld.y, pArea.endWorld.y);
        float maxY = std::max(pArea.startWorld.y, pArea.endWorld.y);

        auto resView = registry.view<ecs::WorldPos, ecs::ResourceTag>();
        for (auto e : resView) {
            auto& wp = resView.get<ecs::WorldPos>(e);
            if (wp.wx >= minX && wp.wx <= maxX && wp.wy >= minY && wp.wy <= maxY) {
                bool match = false;

                if (pArea.includeTrees && registry.all_of<ecs::TreeTag>(e)) match = true;
                if (pArea.includeRocks && registry.all_of<ecs::RockTag>(e)) match = true;
                if (pArea.includeSmallRocks && registry.all_of<ecs::SmallRockTag>(e)) match = true;
                if (pArea.includeBushes && registry.all_of<ecs::BushTag>(e)) match = true;
                if (pArea.includeLogs && registry.all_of<ecs::LogTag>(e)) match = true;

                if (match) registry.emplace<ecs::PreviewHarvestTag>(e, sf::Color(255, 255, 255, 255));
            }
        }
    }

    if (currentEnter && !m_prevEnter) {
        auto previewView = registry.view<ecs::PreviewHarvestTag>();
        bool markedAny = false;

        sf::Color randomColor(rand() % 155 + 100, rand() % 155 + 100, rand() % 155 + 100, 80);
        sf::Color solidColor(randomColor.r, randomColor.g, randomColor.b, 255);

        auto areaEntity = registry.create();
        auto& pArea = pendingView.get<ecs::PendingTaskArea>(*pendingView.begin());

        registry.emplace<ecs::TaskArea>(areaEntity, m_pendingStart, m_pendingEnd, randomColor, m_globalTaskId++,
                                        pArea.hasDropoff, pArea.dropoffStart, pArea.dropoffEnd, pArea.collectFutureDrops);

        for (auto e : previewView) {
            registry.emplace_or_replace<ecs::MarkedForHarvestTag>(e, solidColor, areaEntity);
            markedAny = true;
        }

        if (markedAny || pArea.hasDropoff) {
            auto selectedUnits = registry.view<ecs::SelectedTag>();
            for (auto e : selectedUnits) {
                auto& uData = registry.get<ecs::UnitData>(e);
                uData.currentTask = areaEntity;
                registry.emplace_or_replace<ecs::WorkerTag>(e);
            }
        } else {
            registry.destroy(areaEntity);
        }

        cancelTask(registry);
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