#include "Input/CursorDragSystem.hpp"
#include "../../include/Simulation/World/PathfindingSystem.hpp"
#include "Core/ThreadPool.hpp"
#include "ECS/Components.hpp"
#include "ECS/Tags.hpp"
#include <SFML/Window/Keyboard.hpp>
#include <algorithm>
#include <cmath>

namespace wr::input {

void CursorDragSystem::startDrag(const math::Vec2f& worldPos) noexcept {
    m_isDragging = true;
    m_dragStart = worldPos;
}

sf::Color CursorDragSystem::getDragColor(bool isTaskMode, bool isE) const noexcept {
    bool isCtrl = sf::Keyboard::isKeyPressed(sf::Keyboard::LControl) || sf::Keyboard::isKeyPressed(sf::Keyboard::RControl);

    if (isCtrl) return sf::Color(255, 215, 0, 50);
    if (isTaskMode) {
        if (isE) return sf::Color(100, 255, 100, 80);
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
    bool isEHeld = sf::Keyboard::isKeyPressed(sf::Keyboard::E);

    if (isCtrlHeld) {
        auto view = registry.view<ecs::WorldPos, ecs::UnitTag>();
        for (auto e : view) {
            const auto& wp = view.get<ecs::WorldPos>(e);
            if (wp.wx >= minX && wp.wx <= maxX && wp.wy >= minY && wp.wy <= maxY) {
                registry.emplace_or_replace<ecs::SelectedTag>(e);
                registry.remove<ecs::IdleTag>(e);
            }
        }
        return true;
    }

    else if (taskSystem.isTaskModeActive()) {
        if (isEHeld) {
            taskSystem.trySetDropoffArea(registry, m_dragStart, exactHoveredPos);
        } else {
            taskSystem.addPendingArea(registry, m_dragStart, exactHoveredPos);
        }
        return true;
    }
    else {
        auto selectedUnits = registry.view<ecs::SelectedTag, ecs::WorldPos>();

        std::vector<entt::entity> movableUnits;
        for (auto e : selectedUnits) {
            bool hasTask = registry.all_of<ecs::WorkerTag>(e) || (registry.all_of<ecs::WorkerState>(e) && registry.get<ecs::WorkerState>(e).currentTask != entt::null);
            bool isPaused = registry.all_of<ecs::PausedTag>(e);

            if (hasTask && !isPaused) continue;
            movableUnits.push_back(e);
        }

        int count = movableUnits.size();

        if (count > 0) {
            auto globalObstacles = std::make_shared<simulation::GlobalObstacleMap>();
            int64_t cMinX = 9999999, cMaxX = -9999999, cMinY = 9999999, cMaxY = -9999999;

            for (const auto& [coord, chunk] : chunkManager.getChunks()) {
                if (chunk->state.load(std::memory_order_acquire) != world::ChunkState::Active) continue;
                cMinX = std::min(cMinX, coord.x); cMaxX = std::max(cMaxX, coord.x);
                cMinY = std::min(cMinY, coord.y); cMaxY = std::max(cMaxY, coord.y);
            }

            if (cMinX <= cMaxX) {
                globalObstacles->init(cMinX * 64, cMinY * 64, (cMaxX - cMinX + 1) * 64, (cMaxY - cMinY + 1) * 64);
                auto solidViewHitbox = registry.view<ecs::SolidTag, ecs::WorldPos, ecs::Hitbox>();
                for (auto e : solidViewHitbox) {
                    auto& wp = solidViewHitbox.get<ecs::WorldPos>(e);
                    float rad = solidViewHitbox.get<ecs::Hitbox>(e).radius / 64.0f;
                    if (rad <= 0.4f) {
                        globalObstacles->set(static_cast<int64_t>(std::floor(wp.wx)), static_cast<int64_t>(std::floor(wp.wy)));
                    } else {
                        float shrink = 0.1f;
                        int pMinX = static_cast<int>(std::floor(wp.wx - rad + shrink));
                        int pMaxX = static_cast<int>(std::floor(wp.wx + rad - shrink));
                        int pMinY = static_cast<int>(std::floor(wp.wy - rad + shrink));
                        int pMaxY = static_cast<int>(std::floor(wp.wy + rad - shrink));
                        for(int x = pMinX; x <= pMaxX; ++x) {
                            for(int y = pMinY; y <= pMaxY; ++y) globalObstacles->set(x, y);
                        }
                    }
                }
                auto solidViewLogical = registry.view<ecs::SolidTag, ecs::LogicalPos>(entt::exclude<ecs::WorldPos>);
                for (auto e : solidViewLogical) {
                    auto& pos = solidViewLogical.get<ecs::LogicalPos>(e);
                    globalObstacles->set(pos.x, pos.y);
                }
            }

            math::Vec2f targetWorld = { (minX + maxX) / 2.0f, (minY + maxY) / 2.0f };

            int side = std::max(1, static_cast<int>(std::ceil(std::sqrt(count))));
            float worldBoxWidth = std::max(0.5f, maxX - minX);
            float worldBoxHeight = std::max(0.5f, maxY - minY);

            float spacingX = worldBoxWidth / side;
            float spacingY = worldBoxHeight / side;

            int idx = 0;
            auto* threadPool = registry.ctx().get<core::ThreadPool*>();

            for (auto e : movableUnits) {
                auto& wp = registry.get<ecs::WorldPos>(e);
                float offsetX = ((idx % side) - (side - 1) / 2.0f) * spacingX;
                float offsetY = ((idx / side) - (side - 1) / 2.0f) * spacingY;
                math::Vec2f dest = {targetWorld.x + offsetX, targetWorld.y + offsetY};

                auto pathFuture = threadPool->enqueue([startWp = math::Vec2f{wp.wx, wp.wy}, dest, &chunkManager, globalObstacles]() {
                    return simulation::PathfindingSystem::findPath(startWp, dest, chunkManager, *globalObstacles);
                });
                registry.emplace_or_replace<ecs::PathRequest>(e, std::move(pathFuture));

                bool hasTask2 = registry.all_of<ecs::WorkerTag>(e) || (registry.all_of<ecs::WorkerState>(e) && registry.get<ecs::WorkerState>(e).currentTask != entt::null);

                if (!hasTask2) {
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
                                    for (const auto& rect : area.areas) {
                                        float tMinX = std::min(rect.startWorld.x, rect.endWorld.x);
                                        float tMaxX = std::max(rect.startWorld.x, rect.endWorld.x);
                                        float tMinY = std::min(rect.startWorld.y, rect.endWorld.y);
                                        float tMaxY = std::max(rect.startWorld.y, rect.endWorld.y);
                                        if (rWp->wx >= tMinX && rWp->wx <= tMaxX && rWp->wy >= tMinY && rWp->wy <= tMaxY) {
                                            isInTaskArea = true; break;
                                        }
                                    }
                                    if(isInTaskArea) break;
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
                }
                registry.remove<ecs::WorkerTag>(e);
                idx++;
            }
        } else {
            registry.clear<ecs::SelectedTag>();
        }
    }
    return true;
}

}