#include "Input/CursorHandler.hpp"
#include "Input/CursorRaycast.hpp"
#include "ECS/Components.hpp"
#include "ECS/Tags.hpp"
#include "Rendering/TileHandler.hpp"
#include <SFML/Window/Keyboard.hpp>
#include <SFML/Window/Mouse.hpp>
#include <cmath>

namespace wr::input {

void CursorHandler::init(const sf::Texture& texture) noexcept {
    m_sprite.setTexture(texture);
    m_sprite.setOrigin(24.0f, 16.0f);
}

void CursorHandler::update(const sf::RenderWindow& window, const rendering::Camera& camera, world::ChunkManager& chunkManager, entt::registry& registry, float dt) noexcept {
    sf::Vector2i pixelPos = sf::Mouse::getPosition(window);
    sf::Vector2f uiPos = window.mapPixelToCoords(pixelPos, window.getDefaultView());
    m_sprite.setPosition(uiPos);

    m_exactHoveredWorldPos = CursorRaycast::getHoveredWorldPos(window, camera, chunkManager);

    m_taskSystem.update(registry, dt);
    m_builderSystem.update(registry, chunkManager, m_exactHoveredWorldPos);

    if (m_builderSystem.isBuilderModeActive() && m_taskSystem.isTaskModeActive()) {
        if (registry.ctx().contains<ecs::TaskModeState>()) {
            registry.ctx().get<ecs::TaskModeState>().active = false;
        }
    }

    bool hoveringInteractable = false;

    bool canHarvestTrees = false, canHarvestRocks = false, canHarvestBushes = false, canHarvestLogs = false, canHarvestSmallRocks = false, canBuild = false;
    bool hasSelection = false;

    auto selView = registry.view<ecs::SelectedTag, ecs::UnitData>();
    for (auto e : selView) {
        hasSelection = true;
        auto type = selView.get<ecs::UnitData>(e).type;
        if (type == ecs::UnitType::Warrior) { canHarvestBushes = true; }
        if (type == ecs::UnitType::Lumberjack) { canHarvestTrees = true; canHarvestBushes = true; }
        if (type == ecs::UnitType::Miner) { canHarvestRocks = true; }
        if (type == ecs::UnitType::Courier) { canHarvestLogs = true; canHarvestSmallRocks = true; }
        if (type == ecs::UnitType::Builder) { canBuild = true; }
    }

    sf::Vector2f viewPos = window.mapPixelToCoords(pixelPos, camera.getView());

    auto isPointInTask = [](const ecs::TaskArea& area, const math::Vec2f& p) {
        if (area.hasDropoff) {
            float minX = std::min(area.dropoffStart.x, area.dropoffEnd.x);
            float maxX = std::max(area.dropoffStart.x, area.dropoffEnd.x);
            float minY = std::min(area.dropoffStart.y, area.dropoffEnd.y);
            float maxY = std::max(area.dropoffStart.y, area.dropoffEnd.y);
            if (p.x >= minX && p.x <= maxX && p.y >= minY && p.y <= maxY) return true;
        }
        for (const auto& rect : area.areas) {
            float minX = std::min(rect.startWorld.x, rect.endWorld.x);
            float maxX = std::max(rect.startWorld.x, rect.endWorld.x);
            float minY = std::min(rect.startWorld.y, rect.endWorld.y);
            float maxY = std::max(rect.startWorld.y, rect.endWorld.y);
            if (p.x >= minX && p.x <= maxX && p.y >= minY && p.y <= maxY) return true;
        }
        return false;
    };

    if (m_builderSystem.isBuilderModeActive()) {
        auto bView = registry.view<ecs::BuildingTag, ecs::WorldPos, ecs::ConstructionData>();
        for (auto bEnt : bView) {
            auto& wp = bView.get<ecs::WorldPos>(bEnt);
            auto& cData = bView.get<ecs::ConstructionData>(bEnt);
            bool isStorage = registry.all_of<ecs::CityStorageTag>(bEnt);

            if (!cData.isBuilt || isStorage) {
                float hw = cData.resourceZoneWidth / 2.0f;
                float hh = cData.resourceZoneHeight / 2.0f;
                if (m_exactHoveredWorldPos.x >= wp.wx - hw && m_exactHoveredWorldPos.x <= wp.wx + hw &&
                    m_exactHoveredWorldPos.y >= wp.wy - hh && m_exactHoveredWorldPos.y <= wp.wy + hh) {
                    hoveringInteractable = true; break;
                }
            }
        }

        if (!hoveringInteractable) {
            auto unitView = registry.view<ecs::ScreenPos, ecs::WorldPos, ecs::UnitTag, ecs::UnitData>();
            for (auto entity : unitView) {
                if (unitView.get<ecs::UnitData>(entity).type != ecs::UnitType::Builder) continue;
                const auto& sp = unitView.get<ecs::ScreenPos>(entity);
                const auto& wp = unitView.get<ecs::WorldPos>(entity);
                float clickRad = 32.0f;
                if (const auto* ca = registry.try_get<ecs::ClickArea>(entity)) clickRad = ca->radius;
                float dx = viewPos.x - sp.x;
                float dy = viewPos.y - (sp.y - wp.wz - 32.0f);
                if (dx * dx + dy * dy <= clickRad * clickRad) { hoveringInteractable = true; break; }
            }
        }
    }
    else if (m_taskSystem.isTaskModeActive()) {
        auto pendingView = registry.view<ecs::PendingTaskArea>();
        for (auto e : pendingView) {
            const auto& pArea = pendingView.get<ecs::PendingTaskArea>(e);

            if (pArea.hasDropoff) {
                float minX = std::min(pArea.dropoffStart.x, pArea.dropoffEnd.x);
                float maxX = std::max(pArea.dropoffStart.x, pArea.dropoffEnd.x);
                float minY = std::min(pArea.dropoffStart.y, pArea.dropoffEnd.y);
                float maxY = std::max(pArea.dropoffStart.y, pArea.dropoffEnd.y);
                if (m_exactHoveredWorldPos.x >= minX && m_exactHoveredWorldPos.x <= maxX && m_exactHoveredWorldPos.y >= minY && m_exactHoveredWorldPos.y <= maxY) {
                    hoveringInteractable = true;
                }
            }

            if (!hoveringInteractable) {
                for (const auto& rect : pArea.areas) {
                    float minX = std::min(rect.startWorld.x, rect.endWorld.x);
                    float maxX = std::max(rect.startWorld.x, rect.endWorld.x);
                    float minY = std::min(rect.startWorld.y, rect.endWorld.y);
                    float maxY = std::max(rect.startWorld.y, rect.endWorld.y);
                    if (m_exactHoveredWorldPos.x >= minX && m_exactHoveredWorldPos.x <= maxX && m_exactHoveredWorldPos.y >= minY && m_exactHoveredWorldPos.y <= maxY) {
                        hoveringInteractable = true; break;
                    }
                }
            }

            if (!hoveringInteractable && canHarvestLogs) {
                auto bView = registry.view<ecs::BuildingTag, ecs::WorldPos, ecs::ConstructionData>();
                for (auto bEnt : bView) {
                    auto& wp = bView.get<ecs::WorldPos>(bEnt);
                    auto& cData = bView.get<ecs::ConstructionData>(bEnt);
                    if (!cData.isBuilt) {
                        float hw = cData.resourceZoneWidth / 2.0f;
                        float hh = cData.resourceZoneHeight / 2.0f;
                        if (m_exactHoveredWorldPos.x >= wp.wx - hw && m_exactHoveredWorldPos.x <= wp.wx + hw &&
                            m_exactHoveredWorldPos.y >= wp.wy - hh && m_exactHoveredWorldPos.y <= wp.wy + hh) {
                            hoveringInteractable = true; break;
                        }
                    }
                }
            }

            if (hoveringInteractable) break;
        }
    } else {
        auto unitView = registry.view<ecs::ScreenPos, ecs::WorldPos, ecs::UnitTag>();
        for (auto entity : unitView) {
            const auto& sp = unitView.get<ecs::ScreenPos>(entity);
            const auto& wp = unitView.get<ecs::WorldPos>(entity);
            float clickRad = 32.0f;
            if (const auto* ca = registry.try_get<ecs::ClickArea>(entity)) clickRad = ca->radius;
            float dx = viewPos.x - sp.x;
            float dy = viewPos.y - (sp.y - wp.wz - 32.0f);
            if (dx * dx + dy * dy <= clickRad * clickRad) { hoveringInteractable = true; break; }
        }

        if (!hoveringInteractable && hasSelection) {
            auto resView = registry.view<ecs::ScreenPos, ecs::WorldPos>(entt::exclude<ecs::UnitTag>);
            for (auto entity : resView) {
                const auto& sp = resView.get<ecs::ScreenPos>(entity);
                const auto& wp = resView.get<ecs::WorldPos>(entity);
                float clickRad = 19.2f;
                if (const auto* ca = registry.try_get<ecs::ClickArea>(entity)) clickRad = ca->radius;
                else if (const auto* hb = registry.try_get<ecs::Hitbox>(entity)) clickRad = hb->radius;
                float dx = viewPos.x - sp.x;
                float dy = viewPos.y - (sp.y - wp.wz);

                if (dx * dx + dy * dy <= clickRad * clickRad) {
                    bool interactable = false;
                    if (registry.all_of<ecs::TreeTag>(entity) && canHarvestTrees) interactable = true;
                    else if (registry.all_of<ecs::RockTag>(entity) && canHarvestRocks) interactable = true;
                    else if (registry.all_of<ecs::BushTag>(entity) && canHarvestBushes) interactable = true;
                    else if (registry.all_of<ecs::LogTag>(entity) && canHarvestLogs) interactable = true;
                    else if (registry.all_of<ecs::SmallRockTag>(entity) && canHarvestSmallRocks) interactable = true;

                    if (interactable) {
                        hoveringInteractable = true;
                        break;
                    }
                }
            }
        }

        if (!hoveringInteractable) {
            auto taskView = registry.view<ecs::TaskArea>();
            for (auto e : taskView) {
                if (isPointInTask(taskView.get<ecs::TaskArea>(e), m_exactHoveredWorldPos)) {
                    hoveringInteractable = true; break;
                }
            }
        }
    }

    const auto* tilesPtr = registry.ctx().find<const rendering::TileHandler*>();
    if (tilesPtr) {
        if (hoveringInteractable) m_sprite.setTexture((*tilesPtr)->getCursorHoverTexture());
        else                      m_sprite.setTexture((*tilesPtr)->getCursorTexture());
    }
}

sf::Color CursorHandler::getDragColor() const noexcept {
    bool isE = sf::Keyboard::isKeyPressed(sf::Keyboard::E);
    return m_dragSystem.getDragColor(m_taskSystem.isTaskModeActive(), isE);
}

bool CursorHandler::handleMouseClick(entt::registry& registry, const sf::RenderWindow& window, const rendering::Camera& camera, world::ChunkManager& chunkManager) noexcept {
    float clickTime = m_clickClock.getElapsedTime().asSeconds();
    float dx = m_exactHoveredWorldPos.x - m_lastClickPos.x;
    float dy = m_exactHoveredWorldPos.y - m_lastClickPos.y;
    float distSq = dx*dx + dy*dy;
    bool isDoubleClick = (clickTime < 0.4f && distSq < 2.0f);
    m_clickClock.restart();

    if (m_builderSystem.isBuilderModeActive()) {

        entt::entity clickedUnit = entt::null;
        float closestUnitDistSq = 999999.0f;
        sf::Vector2i pixelPos = sf::Mouse::getPosition(window);
        sf::Vector2f viewPos = window.mapPixelToCoords(pixelPos, camera.getView());

        auto unitView = registry.view<ecs::ScreenPos, ecs::WorldPos, ecs::UnitTag, ecs::UnitData>();
        for (auto entity : unitView) {
            if (unitView.get<ecs::UnitData>(entity).type != ecs::UnitType::Builder) continue;
            const auto& sp = unitView.get<ecs::ScreenPos>(entity);
            const auto& wp = unitView.get<ecs::WorldPos>(entity);
            float clickRad = 32.0f;
            if (const auto* ca = registry.try_get<ecs::ClickArea>(entity)) clickRad = ca->radius;
            float cx = viewPos.x - sp.x;
            float cy = viewPos.y - (sp.y - wp.wz - 32.0f);
            float cDistSq = cx * cx + cy * cy;
            if (cDistSq <= clickRad * clickRad && cDistSq < closestUnitDistSq) {
                closestUnitDistSq = cDistSq; clickedUnit = entity;
            }
        }

        if (clickedUnit != entt::null) {
            bool isCtrl = sf::Keyboard::isKeyPressed(sf::Keyboard::LControl) || sf::Keyboard::isKeyPressed(sf::Keyboard::RControl);
            if (!isCtrl) registry.clear<ecs::SelectedTag>();

            if (isCtrl && registry.all_of<ecs::SelectedTag>(clickedUnit)) {
                registry.remove<ecs::SelectedTag>(clickedUnit);
            } else {
                registry.emplace_or_replace<ecs::SelectedTag>(clickedUnit);
                registry.remove<ecs::IdleTag>(clickedUnit);
            }
            m_lastClickPos = m_exactHoveredWorldPos;
            return true;
        }

        entt::entity clickedBuilding = entt::null;
        auto bView = registry.view<ecs::BuildingTag, ecs::WorldPos, ecs::ConstructionData>();
        for (auto e : bView) {
            auto& wp = bView.get<ecs::WorldPos>(e);
            auto& cData = bView.get<ecs::ConstructionData>(e);
            bool isStorage = registry.all_of<ecs::CityStorageTag>(e);

            if (!cData.isBuilt || isStorage) {
                float hw = cData.resourceZoneWidth / 2.0f;
                float hh = cData.resourceZoneHeight / 2.0f;
                if (m_exactHoveredWorldPos.x >= wp.wx - hw && m_exactHoveredWorldPos.x <= wp.wx + hw &&
                    m_exactHoveredWorldPos.y >= wp.wy - hh && m_exactHoveredWorldPos.y <= wp.wy + hh) {
                    clickedBuilding = e; break;
                }
            }
        }

        if (isDoubleClick) {
            if (clickedBuilding != entt::null) {
                bool removed = false;
                auto selView = registry.view<ecs::SelectedTag, ecs::UnitData, ecs::WorkerState>();
                for (auto selEnt : selView) {
                    auto& uData = selView.get<ecs::UnitData>(selEnt);
                    auto& wState = selView.get<ecs::WorkerState>(selEnt);
                    if (uData.type == ecs::UnitType::Builder) {
                        auto it = std::find(wState.taskQueue.begin(), wState.taskQueue.end(), clickedBuilding);
                        if (it != wState.taskQueue.end()) {
                            wState.taskQueue.erase(it);
                            removed = true;
                            if (wState.currentTask == clickedBuilding) {
                                wState.currentTask = wState.taskQueue.empty() ? entt::null : wState.taskQueue.front();
                                if (wState.currentTask == entt::null) {
                                    registry.remove<ecs::ActionTarget>(selEnt);
                                    registry.remove<ecs::Path>(selEnt);
                                    registry.remove<ecs::WorkerTag>(selEnt);
                                } else {
                                    registry.emplace_or_replace<ecs::ActionTarget>(selEnt, wState.currentTask);
                                    registry.remove<ecs::IdleTag>(selEnt);
                                    registry.remove<ecs::Path>(selEnt);
                                }
                            }
                        }
                    }
                }

                auto& cData = registry.get<ecs::ConstructionData>(clickedBuilding);
                if (registry.all_of<ecs::CityStorageTag>(clickedBuilding)) {
                    bool isEmpty = true;
                    auto& bWp = registry.get<ecs::WorldPos>(clickedBuilding);
                    float hw = cData.resourceZoneWidth / 2.0f;
                    float hh = cData.resourceZoneHeight / 2.0f;
                    float minX = bWp.wx - hw; float maxX = bWp.wx + hw;
                    float minY = bWp.wy - hh; float maxY = bWp.wy + hh;

                    auto resView = registry.view<ecs::WorldPos, ecs::ResourceTag>();
                    for (auto r : resView) {
                        if (registry.all_of<ecs::LogTag>(r) || registry.all_of<ecs::SmallRockTag>(r)) {
                            auto& rWp = resView.get<ecs::WorldPos>(r);
                            if (rWp.wx >= minX && rWp.wx <= maxX && rWp.wy >= minY && rWp.wy <= maxY) {
                                isEmpty = false; break;
                            }
                        }
                    }
                    if (isEmpty) registry.destroy(clickedBuilding);
                } else if (cData.buildProgress <= 0.0f) {
                    registry.destroy(clickedBuilding);
                }
            }
            m_lastClickPos = {-9999.0f, -9999.0f};
        } else {
            if (clickedBuilding != entt::null && !registry.all_of<ecs::CityStorageTag>(clickedBuilding)) {
                auto selView = registry.view<ecs::SelectedTag, ecs::UnitData, ecs::WorkerState>();
                for (auto selEnt : selView) {
                    auto& uData = selView.get<ecs::UnitData>(selEnt);
                    auto& wState = selView.get<ecs::WorkerState>(selEnt);
                    if (uData.type == ecs::UnitType::Builder) {
                        if (std::find(wState.taskQueue.begin(), wState.taskQueue.end(), clickedBuilding) == wState.taskQueue.end()) {
                            wState.taskQueue.push_back(clickedBuilding);
                        }
                        if (wState.currentTask == entt::null) {
                            wState.currentTask = wState.taskQueue.front();
                            registry.emplace_or_replace<ecs::ActionTarget>(selEnt, wState.currentTask);
                            registry.emplace_or_replace<ecs::WorkerTag>(selEnt);
                            registry.remove<ecs::IdleTag>(selEnt);
                            registry.remove<ecs::Path>(selEnt);
                        }
                    }
                }
            }
            m_lastClickPos = m_exactHoveredWorldPos;
        }
        return true;
    }

    if (m_taskSystem.isTaskModeActive()) {
        if (!isDoubleClick) {
            bool canLogs = false;
            auto selectedUnits = registry.view<ecs::SelectedTag, ecs::UnitData>();
            for (auto se : selectedUnits) {
                if (selectedUnits.get<ecs::UnitData>(se).type == ecs::UnitType::Courier) canLogs = true;
            }

            if (canLogs) {
                entt::entity clickedBuilding = entt::null;
                auto bView = registry.view<ecs::BuildingTag, ecs::WorldPos, ecs::ConstructionData>();
                for (auto e : bView) {
                    auto& wp = bView.get<ecs::WorldPos>(e);
                    auto& cData = bView.get<ecs::ConstructionData>(e);
                    if (!cData.isBuilt) {
                        float hw = cData.resourceZoneWidth / 2.0f;
                        float hh = cData.resourceZoneHeight / 2.0f;
                        if (m_exactHoveredWorldPos.x >= wp.wx - hw && m_exactHoveredWorldPos.x <= wp.wx + hw &&
                            m_exactHoveredWorldPos.y >= wp.wy - hh && m_exactHoveredWorldPos.y <= wp.wy + hh) {
                            clickedBuilding = e; break;
                        }
                    }
                }

                if (clickedBuilding != entt::null) {
                    m_taskSystem.addBuildingTarget(registry, clickedBuilding);
                    m_lastClickPos = m_exactHoveredWorldPos;
                    return true;
                }
            }

            m_taskSystem.selectHoveredArea(registry, m_exactHoveredWorldPos);
            m_lastClickPos = m_exactHoveredWorldPos;
            return true;
        } else {
            m_taskSystem.removeHoveredArea(registry, m_exactHoveredWorldPos);
            m_lastClickPos = {-9999.0f, -9999.0f};
            return true;
        }
    } else {
        auto isPointInTaskRect = [](const ecs::TaskArea& area, const math::Vec2f& p, int& outIdx) {
            for (size_t i = 0; i < area.areas.size(); ++i) {
                const auto& rect = area.areas[i];
                float minX = std::min(rect.startWorld.x, rect.endWorld.x);
                float maxX = std::max(rect.startWorld.x, rect.endWorld.x);
                float minY = std::min(rect.startWorld.y, rect.endWorld.y);
                float maxY = std::max(rect.startWorld.y, rect.endWorld.y);
                if (p.x >= minX && p.x <= maxX && p.y >= minY && p.y <= maxY) {
                    outIdx = static_cast<int>(i);
                    return true;
                }
            }
            return false;
        };

        if (isDoubleClick) {
            entt::entity clickedTask = entt::null;
            auto taskView = registry.view<ecs::TaskArea>();
            for (auto e : taskView) {
                int dummy;
                if (isPointInTaskRect(taskView.get<ecs::TaskArea>(e), m_exactHoveredWorldPos, dummy)) {
                    clickedTask = e; break;
                }
            }

            if (clickedTask != entt::null) {
                registry.clear<ecs::FocusedTaskTag>();
                registry.clear<ecs::FocusedAreaTag>();
                registry.clear<ecs::SelectedTag>();
                registry.emplace<ecs::FocusedTaskTag>(clickedTask);
                m_lastClickPos = {-9999.0f, -9999.0f};
                return true;
            }
        }

        m_lastClickPos = m_exactHoveredWorldPos;
        bool handledUnitClick = CursorClickSystem::handleMouseClick(registry, window, camera);

        if (handledUnitClick) {
            registry.clear<ecs::FocusedTaskTag>();
            registry.clear<ecs::FocusedAreaTag>();
            return true;
        }

        entt::entity clickedTask = entt::null;
        int clickedIdx = -1;
        auto taskView = registry.view<ecs::TaskArea>();
        for (auto e : taskView) {
            if (isPointInTaskRect(taskView.get<ecs::TaskArea>(e), m_exactHoveredWorldPos, clickedIdx)) {
                clickedTask = e; break;
            }
        }

        bool hadFocusedArea = false;
        entt::entity prevAreaTask = entt::null;
        int prevAreaIdx = -1;
        auto fAreaView = registry.view<ecs::FocusedAreaTag>();
        if (!fAreaView.empty()) {
            hadFocusedArea = true;
            prevAreaTask = fAreaView.front();
            prevAreaIdx = fAreaView.get<ecs::FocusedAreaTag>(prevAreaTask).areaIndex;
        }

        registry.clear<ecs::FocusedTaskTag>();
        registry.clear<ecs::FocusedAreaTag>();

        if (clickedTask != entt::null) {
            if (hadFocusedArea && prevAreaTask == clickedTask && prevAreaIdx == clickedIdx) {
                registry.emplace<ecs::FocusedTaskTag>(clickedTask);
            } else {
                registry.emplace<ecs::FocusedAreaTag>(clickedTask, clickedIdx);
            }
            return true;
        }

        return false;
    }
}

void CursorHandler::startDrag() noexcept {
    if (m_builderSystem.isBuilderModeActive()) return;
    m_dragSystem.startDrag(m_exactHoveredWorldPos);
}

bool CursorHandler::endDrag(entt::registry& registry, const rendering::Camera& camera, world::ChunkManager& chunkManager) noexcept {
    if (m_builderSystem.isBuilderModeActive()) return false;
    return m_dragSystem.endDrag(registry, chunkManager, m_exactHoveredWorldPos, m_taskSystem);
}

void CursorHandler::draw(sf::RenderWindow& window) const {
    window.setView(window.getDefaultView());
    window.draw(m_sprite);
}

void CursorHandler::setSystemCursorVisible(sf::RenderWindow& window, bool visible) const noexcept {
    window.setMouseCursorVisible(visible);
}

}