#include "../../../include/Simulation/Unit/UnitControlSystem.hpp"
#include "../../../include/Simulation/Unit/UnitCombatSystem.hpp"
#include "../../../include/Simulation/Unit/UnitMovementSystem.hpp"
#include "../../../include/Simulation/Unit/UnitPhysicsSystem.hpp"
#include "../../../include/Simulation/World/PathfindingSystem.hpp"
#include "../../../include/Simulation/Environment/ResourceSystem.hpp"
#include "ECS/Components.hpp"
#include "ECS/Tags.hpp"
#include <SFML/Window/Keyboard.hpp>
#include <algorithm>
#include <cmath>
#include <unordered_set>

namespace wr::simulation {

UnitControlSystem::UnitControlSystem(world::ChunkManager& chunkManager)
    : m_chunkManager(chunkManager) {}

void UnitControlSystem::update(entt::registry& registry, float dt, rendering::ViewDirection viewDir, const math::Vec2f& mouseWorldPos, bool isRightClicking) noexcept {

    auto canHarvest = [&](ecs::UnitType type, ecs::HeldItem heldItem, entt::entity targetRes) -> bool {
        if (registry.all_of<ecs::Health>(targetRes) && registry.get<ecs::Health>(targetRes).current <= 0) return false;
        if (registry.all_of<ecs::RockTag>(targetRes)) return type == ecs::UnitType::Miner;
        if (registry.all_of<ecs::TreeTag>(targetRes)) return type == ecs::UnitType::Lumberjack;
        if (registry.all_of<ecs::BushTag>(targetRes)) return type == ecs::UnitType::Lumberjack || type == ecs::UnitType::Warrior;
        if (registry.all_of<ecs::LogTag>(targetRes)) return type == ecs::UnitType::Courier && heldItem == ecs::HeldItem::None;
        if (registry.all_of<ecs::SmallRockTag>(targetRes)) return type == ecs::UnitType::Courier && heldItem == ecs::HeldItem::None;
        return true;
    };

    auto selectedView = registry.view<ecs::SelectedTag>();
    int selectedCount = std::distance(selectedView.begin(), selectedView.end());

    math::Vec2f kbVector = UnitMovementSystem::getKeyboardWorldVector(viewDir);

    bool isSpacePressed = sf::Keyboard::isKeyPressed(sf::Keyboard::Space);
    bool spaceJustPressed = isSpacePressed && !m_prevSpacePressed;

    bool isMovingManual = (kbVector.x != 0.0f || kbVector.y != 0.0f) && (selectedCount == 1);

    bool isCtrlHeld = sf::Keyboard::isKeyPressed(sf::Keyboard::LControl) || sf::Keyboard::isKeyPressed(sf::Keyboard::RControl);
    bool isCancelCommand = isCtrlHeld && sf::Keyboard::isKeyPressed(sf::Keyboard::Q);

    auto unitGroup = registry.view<ecs::UnitTag>();

    std::unordered_set<math::Vec2i64, math::SpatialHash> obstacles;
    auto solidView = registry.view<ecs::SolidTag, ecs::LogicalPos>();
    for (auto e : solidView) {
        const auto& lp = solidView.get<ecs::LogicalPos>(e);
        obstacles.insert({lp.x, lp.y});
    }

    auto releaseUnitAction = [&](entt::entity e) {
        registry.remove<ecs::WorkerTag>(e);
        registry.remove<ecs::Path>(e);

        if (auto* action = registry.try_get<ecs::ActionTarget>(e)) {
            entt::entity targetRes = action->target;
            registry.remove<ecs::ActionTarget>(e);

            if (registry.valid(targetRes) && !registry.all_of<ecs::TaskArea>(targetRes)) {
                registry.remove<ecs::ClaimedTag>(targetRes);

                bool isInTaskArea = false;
                if (auto* rWp = registry.try_get<ecs::WorldPos>(targetRes)) {
                    auto taskView = registry.view<ecs::TaskArea>();
                    for (auto tEnt : taskView) {
                        auto& area = taskView.get<ecs::TaskArea>(tEnt);
                        float minX = std::min(area.startWorld.x, area.endWorld.x);
                        float maxX = std::max(area.startWorld.x, area.endWorld.x);
                        float minY = std::min(area.startWorld.y, area.endWorld.y);
                        float maxY = std::max(area.startWorld.y, area.endWorld.y);
                        if (rWp->wx >= minX && rWp->wx <= maxX && rWp->wy >= minY && rWp->wy <= maxY) {
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

        auto& anim = registry.get<ecs::AnimationState>(e);
        if (anim.isActionLocked) {
            anim.isActionLocked = false;
            anim.currentAnim = 0;
            anim.currentFrame = 0;
        }

        auto& uData = registry.get<ecs::UnitData>(e);
        uData.currentTask = entt::null;
    };

    if (isRightClicking && selectedCount > 0) {
        int side = std::max(1, static_cast<int>(std::ceil(std::sqrt(selectedCount))));
        int idx = 0;
        float spacing = 1.0f;

        for (auto entity : unitGroup) {
            if (!registry.all_of<ecs::SelectedTag>(entity)) continue;
            auto& wp = registry.get<ecs::WorldPos>(entity);

            float offsetX = ((idx % side) - (side - 1) / 2.0f) * spacing;
            float offsetY = ((idx / side) - (side - 1) / 2.0f) * spacing;
            math::Vec2f targetWorld = {mouseWorldPos.x + offsetX, mouseWorldPos.y + offsetY};

            releaseUnitAction(entity);

            auto path = PathfindingSystem::findPath({wp.wx, wp.wy}, targetWorld, m_chunkManager, obstacles);
            registry.emplace_or_replace<ecs::Path>(entity, path, size_t{0});
            idx++;
        }
    }

    for (auto entity : unitGroup) {
        auto& uData = registry.get<ecs::UnitData>(entity);
        auto nearbyEntities = UnitPhysicsSystem::getNearbyEntities(registry, entity, m_chunkManager);
        auto& wp = registry.get<ecs::WorldPos>(entity);
        math::Vec2f moveIntent{0.0f, 0.0f};
        bool isSelected = registry.all_of<ecs::SelectedTag>(entity);

        if (isSelected && (isMovingManual || isCancelCommand)) {
            releaseUnitAction(entity);
            if (isCancelCommand) continue;
        }

        bool isWorker = registry.all_of<ecs::WorkerTag>(entity);
        bool isIdle = !registry.all_of<ecs::ActionTarget>(entity) && !registry.all_of<ecs::Path>(entity);

        if (isWorker && isIdle) {
            if (uData.type == ecs::UnitType::Courier && uData.heldItem != ecs::HeldItem::None && registry.valid(uData.currentTask)) {
                if (registry.all_of<ecs::TaskArea>(uData.currentTask)) {
                    auto& task = registry.get<ecs::TaskArea>(uData.currentTask);
                    if (task.hasDropoff) {
                        registry.emplace_or_replace<ecs::ActionTarget>(entity, uData.currentTask);
                        math::Vec2f dropCenter = {(task.dropoffStart.x + task.dropoffEnd.x)/2.f, (task.dropoffStart.y + task.dropoffEnd.y)/2.f};
                        auto path = PathfindingSystem::findPath({wp.wx, wp.wy}, dropCenter, m_chunkManager, obstacles);
                        registry.emplace_or_replace<ecs::Path>(entity, path, size_t{0});
                    } else {
                        auto itemToDrop = uData.heldItem;
                        auto itemSubtype = uData.heldItemSubtype;

                        uData.heldItem = ecs::HeldItem::None;
                        uData.heldItemSubtype = 0;
                        uData.currentTask = entt::null;

                        auto* tilesPtr = registry.ctx().find<const rendering::TileHandler*>();
                        if (tilesPtr) {
                            if (itemToDrop == ecs::HeldItem::Wood) world::ResourceSystem::spawnLogs(registry, **tilesPtr, {wp.wx, wp.wy}, wp.wz, 1, &m_chunkManager, entt::null, true, itemSubtype);
                            else world::ResourceSystem::spawnSmallRocks(registry, **tilesPtr, {wp.wx, wp.wy}, wp.wz, 1, &m_chunkManager, entt::null, true, itemSubtype);
                        }
                    }
                } else {
                    uData.currentTask = entt::null;
                }
            } else {
                float closestDist = 9999999.0f;
                entt::entity bestTarget = entt::null;

                auto markedResources = registry.view<ecs::MarkedForHarvestTag>(entt::exclude<ecs::ClaimedTag>);
                for (auto res : markedResources) {
                    if (!canHarvest(uData.type, uData.heldItem, res)) continue;

                    auto& marked = markedResources.get<ecs::MarkedForHarvestTag>(res);
                    if (uData.currentTask != entt::null && marked.taskEntity != uData.currentTask) continue;

                    auto& rWp = registry.get<ecs::WorldPos>(res);
                    float dx = rWp.wx - wp.wx;
                    float dy = rWp.wy - wp.wy;
                    float distSq = dx*dx + dy*dy;

                    if (distSq < closestDist) {
                        closestDist = distSq;
                        bestTarget = res;
                    }
                }

                if (bestTarget != entt::null) {
                    registry.emplace_or_replace<ecs::ActionTarget>(entity, bestTarget);
                    registry.emplace_or_replace<ecs::ClaimedTag>(bestTarget);

                    auto& marked = registry.get<ecs::MarkedForHarvestTag>(bestTarget);
                    uData.currentTask = marked.taskEntity;

                    auto& tWp = registry.get<ecs::WorldPos>(bestTarget);
                    auto path = PathfindingSystem::findPath({wp.wx, wp.wy}, {tWp.wx, tWp.wy}, m_chunkManager, obstacles);
                    registry.emplace_or_replace<ecs::Path>(entity, path, size_t{0});
                } else {

                    if (uData.currentTask != entt::null) {
                        if (!registry.valid(uData.currentTask) || !registry.all_of<ecs::TaskArea>(uData.currentTask)) {
                            registry.remove<ecs::WorkerTag>(entity);
                            uData.currentTask = entt::null;
                        }
                    } else {
                        registry.remove<ecs::WorkerTag>(entity);
                    }
                }
            }
        }

        if (isSelected && selectedCount == 1) {
            moveIntent = kbVector;

            if (uData.type == ecs::UnitType::Courier) {
                if (spaceJustPressed) {
                    if (uData.heldItem != ecs::HeldItem::None) {
                        auto itemToDrop = uData.heldItem;
                        auto itemSubtype = uData.heldItemSubtype;

                        uData.heldItem = ecs::HeldItem::None;
                        uData.heldItemSubtype = 0;

                        auto* tilesPtr = registry.ctx().find<const rendering::TileHandler*>();
                        if (tilesPtr) {
                            if (itemToDrop == ecs::HeldItem::Wood) world::ResourceSystem::spawnLogs(registry, **tilesPtr, {wp.wx, wp.wy}, wp.wz, 1, &m_chunkManager, entt::null, true, itemSubtype);
                            else world::ResourceSystem::spawnSmallRocks(registry, **tilesPtr, {wp.wx, wp.wy}, wp.wz, 1, &m_chunkManager, entt::null, true, itemSubtype);
                        }

                        auto& anim = registry.get<ecs::AnimationState>(entity);
                        anim.currentAnim = 2;
                        anim.currentFrame = 0;
                        anim.elapsedTime = 0.0f;
                        anim.isActionLocked = true;
                        anim.damageDealt = true;
                        anim.numFrames = 4;
                    } else {
                        auto& anim = registry.get<ecs::AnimationState>(entity);
                        if (anim.currentAnim <= 1) {
                            anim.currentAnim = 2;
                            anim.currentFrame = 0;
                            anim.elapsedTime = 0.0f;
                            anim.isActionLocked = true;
                            anim.damageDealt = false;
                            anim.numFrames = 4;

                            releaseUnitAction(entity);
                        }
                    }
                }
            } else {
                if (isSpacePressed) {
                    auto& anim = registry.get<ecs::AnimationState>(entity);
                    if (anim.currentAnim <= 1) {
                        anim.currentAnim = 2;
                        anim.currentFrame = 0;
                        anim.elapsedTime = 0.0f;
                        anim.isActionLocked = true;
                        anim.damageDealt = false;
                        anim.numFrames = (uData.type == ecs::UnitType::Warrior) ? 4 : 6;

                        releaseUnitAction(entity);
                    }
                }
            }
        }

        auto* action = registry.try_get<ecs::ActionTarget>(entity);
        if (action && registry.valid(action->target)) {
            if (!registry.all_of<ecs::TaskArea>(action->target) && !canHarvest(uData.type, uData.heldItem, action->target)) {
                releaseUnitAction(entity);
            } else if (!registry.all_of<ecs::Path>(entity) && moveIntent.x == 0 && moveIntent.y == 0) {

                math::Vec2f tPos;
                if (registry.all_of<ecs::TaskArea>(action->target)) {
                    auto& t = registry.get<ecs::TaskArea>(action->target);
                    tPos = {(t.dropoffStart.x + t.dropoffEnd.x)/2.f, (t.dropoffStart.y + t.dropoffEnd.y)/2.f};
                } else {
                    auto& w = registry.get<ecs::WorldPos>(action->target);
                    tPos = {w.wx, w.wy};
                }

                auto path = PathfindingSystem::findPath({wp.wx, wp.wy}, tPos, m_chunkManager, obstacles);
                registry.emplace<ecs::Path>(entity, path, size_t{0});
            }
        }

        math::Vec2f combatMove = UnitCombatSystem::processCombat(registry, entity, nearbyEntities, mouseWorldPos, isRightClicking, isSpacePressed, spaceJustPressed, dt, m_chunkManager);
        if (moveIntent.x == 0 && moveIntent.y == 0) moveIntent = combatMove;

        UnitMovementSystem::applyMovement(registry, entity, moveIntent);
        UnitPhysicsSystem::applyPhysicsAndCollisions(registry, entity, dt, m_chunkManager, nearbyEntities);
    }

    m_prevSpacePressed = isSpacePressed;
}

}