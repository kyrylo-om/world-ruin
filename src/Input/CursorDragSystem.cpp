#include "Input/CursorDragSystem.hpp"
#include "../../include/Simulation/World/PathfindingSystem.hpp"
#include "ECS/Components.hpp"
#include "ECS/Tags.hpp"
#include <SFML/Window/Keyboard.hpp>
#include <algorithm>
#include <unordered_set>
#include <cmath>

namespace wr::input {

void CursorDragSystem::startDrag(const math::Vec2f& worldPos) noexcept {
    m_isDragging = true;
    m_dragStart = worldPos;
}

sf::Color CursorDragSystem::getDragColor(bool isSelectingDropoff) const noexcept {
    bool isCtrl = sf::Keyboard::isKeyPressed(sf::Keyboard::LControl) || sf::Keyboard::isKeyPressed(sf::Keyboard::RControl);
    bool isQ = sf::Keyboard::isKeyPressed(sf::Keyboard::Q);

    if (isCtrl) return sf::Color(255, 215, 0, 50);
    if (isQ) {
        if (isSelectingDropoff) return sf::Color(100, 255, 100, 80);
        return sf::Color(255, 255, 255, 80);
    }

    return sf::Color(100, 150, 255, 60);
}

bool CursorDragSystem::endDrag(entt::registry& registry, world::ChunkManager& chunkManager, const math::Vec2f& exactHoveredPos, CursorTaskSystem& taskSystem) noexcept {
    if (!m_isDragging) return false;
    m_isDragging = false;

    float minX = std::min(m_dragStart.x, exactHoveredPos.x);
    float maxX = std::max(m_dragStart.x, exactHoveredPos.x);
    float minY = std::min(m_dragStart.y, exactHoveredPos.y);
    float maxY = std::max(m_dragStart.y, exactHoveredPos.y);

    if (std::abs(maxX - minX) < 0.2f && std::abs(maxY - minY) < 0.2f) return false;

    bool isCtrlHeld = sf::Keyboard::isKeyPressed(sf::Keyboard::LControl) || sf::Keyboard::isKeyPressed(sf::Keyboard::RControl);
    bool isQHeld = sf::Keyboard::isKeyPressed(sf::Keyboard::Q) && !isCtrlHeld;

    if (isQHeld) {
        if (taskSystem.hasPendingTask()) {
            if (taskSystem.isSelectingDropoff()) {
                taskSystem.setDropoffArea(registry, m_dragStart, exactHoveredPos);
            } else {
                taskSystem.startPendingTask(registry, m_dragStart, exactHoveredPos);
            }
            return true;
        }

        auto selectedUnits = registry.view<ecs::SelectedTag, ecs::UnitData>();
        bool onlyCouriersWithItem = true;
        bool hasAnyCourierWithItem = false;

        if (selectedUnits.begin() != selectedUnits.end()) {
            for (auto e : selectedUnits) {
                const auto& uData = selectedUnits.get<ecs::UnitData>(e);
                if (uData.type == ecs::UnitType::Courier && uData.heldItem != ecs::HeldItem::None) {
                    hasAnyCourierWithItem = true;
                } else {
                    onlyCouriersWithItem = false;
                    break;
                }
            }
        } else {
            onlyCouriersWithItem = false;
        }

        if (hasAnyCourierWithItem && onlyCouriersWithItem) {
            auto areaEntity = registry.create();
            sf::Color invisibleColor(0, 0, 0, 0);
            registry.emplace<ecs::TaskArea>(areaEntity, exactHoveredPos, exactHoveredPos, invisibleColor, taskSystem.getNextTaskId(),
                                            true, m_dragStart, exactHoveredPos);

            for (auto e : selectedUnits) {
                auto& uData = registry.get<ecs::UnitData>(e);
                uData.currentTask = areaEntity;
                registry.emplace_or_replace<ecs::WorkerTag>(e);
                registry.remove<ecs::ActionTarget>(e);
                registry.remove<ecs::Path>(e);
            }
            return true;
        } else {
            taskSystem.startPendingTask(registry, m_dragStart, exactHoveredPos);
            return true;
        }
    }
    else if (isCtrlHeld) {

        registry.clear<ecs::SelectedTag>();
        auto view = registry.view<ecs::WorldPos, ecs::UnitTag>();
        for (auto e : view) {
            const auto& wp = view.get<ecs::WorldPos>(e);
            if (wp.wx >= minX && wp.wx <= maxX && wp.wy >= minY && wp.wy <= maxY) {
                registry.emplace_or_replace<ecs::SelectedTag>(e);
            }
        }
    }
    else {

        auto selectedUnits = registry.view<ecs::SelectedTag, ecs::WorldPos>();
        int count = std::distance(selectedUnits.begin(), selectedUnits.end());

        if (count > 0) {
            std::unordered_set<math::Vec2i64, math::SpatialHash> obstacles;
            auto solidView = registry.view<ecs::SolidTag, ecs::LogicalPos>();
            for (auto e : solidView) obstacles.insert({solidView.get<ecs::LogicalPos>(e).x, solidView.get<ecs::LogicalPos>(e).y});

            math::Vec2f targetWorld = { (minX + maxX) / 2.0f, (minY + maxY) / 2.0f };

            int side = std::max(1, static_cast<int>(std::ceil(std::sqrt(count))));
            float worldBoxWidth = std::max(0.5f, maxX - minX);
            float worldBoxHeight = std::max(0.5f, maxY - minY);

            float spacingX = worldBoxWidth / side;
            float spacingY = worldBoxHeight / side;

            int idx = 0;
            for (auto e : selectedUnits) {
                auto& wp = selectedUnits.get<ecs::WorldPos>(e);
                float offsetX = ((idx % side) - (side - 1) / 2.0f) * spacingX;
                float offsetY = ((idx / side) - (side - 1) / 2.0f) * spacingY;
                math::Vec2f dest = {targetWorld.x + offsetX, targetWorld.y + offsetY};

                auto path = simulation::PathfindingSystem::findPath({wp.wx, wp.wy}, dest, chunkManager, obstacles);
                registry.emplace_or_replace<ecs::Path>(e, path, size_t{0});

                if (auto* action = registry.try_get<ecs::ActionTarget>(e)) {
                    entt::entity targetRes = action->target;
                    registry.remove<ecs::ActionTarget>(e);

                    if (registry.valid(targetRes)) {
                        registry.remove<ecs::ClaimedTag>(targetRes);

                        bool isInTaskArea = false;
                        if (auto* rWp = registry.try_get<ecs::WorldPos>(targetRes)) {
                            auto taskView = registry.view<ecs::TaskArea>();
                            for (auto tEnt : taskView) {
                                auto& area = taskView.get<ecs::TaskArea>(tEnt);
                                float tMinX = std::min(area.startWorld.x, area.endWorld.x);
                                float tMaxX = std::max(area.startWorld.x, area.endWorld.x);
                                float tMinY = std::min(area.startWorld.y, area.endWorld.y);
                                float tMaxY = std::max(area.startWorld.y, area.endWorld.y);
                                if (rWp->wx >= tMinX && rWp->wx <= tMaxX && rWp->wy >= tMinY && rWp->wy <= tMaxY) {
                                    isInTaskArea = true; break;
                                }
                            }
                        }

                        if (!isInTaskArea) {
                            bool shared = false;
                            auto seekers = registry.view<ecs::ActionTarget>();
                            for (auto other : seekers) {
                                if (seekers.get<ecs::ActionTarget>(other).target == targetRes) { shared = true; break; }
                            }
                            if (!shared) registry.remove<ecs::MarkedForHarvestTag>(targetRes);
                        }
                    }
                }
                registry.remove<ecs::WorkerTag>(e);

                auto& uData = registry.get<ecs::UnitData>(e);
                uData.currentTask = entt::null;

                idx++;
            }
        } else {
            registry.clear<ecs::SelectedTag>();
        }
    }
    return true;
}

}