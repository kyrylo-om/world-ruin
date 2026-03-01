#include "../../../include/Simulation/Unit/UnitCombatSystem.hpp"
#include "ECS/Components.hpp"
#include "ECS/Tags.hpp"
#include "../../../include/Simulation/Environment/ResourceSystem.hpp"
#include "Rendering/TileHandler.hpp"
#include <SFML/Window/Keyboard.hpp>
#include <cmath>
#include <cstdlib>
#include <algorithm>

namespace wr::simulation {

math::Vec2f UnitCombatSystem::processCombat(entt::registry& registry, entt::entity entity,
                                            const std::vector<entt::entity>& nearbyEntities,
                                            const math::Vec2f& mouseWorldPos,
                                            bool isRightClicking,
                                            bool isSpacePressed,
                                            bool spaceJustPressed,
                                            float dt,
                                            world::ChunkManager& chunkManager) {

    auto& vel = registry.get<ecs::Velocity>(entity);
    auto& anim = registry.get<ecs::AnimationState>(entity);
    auto& wp = registry.get<ecs::WorldPos>(entity);
    auto& uData = registry.get<ecs::UnitData>(entity);

    math::Vec2f moveIntent{0.0f, 0.0f};
    bool isAttackingCommand = false;

    auto selectedView = registry.view<ecs::SelectedTag>();
    bool isHero = (std::distance(selectedView.begin(), selectedView.end()) == 1 && registry.all_of<ecs::SelectedTag>(entity));

    if (isHero) {
        if (uData.type == ecs::UnitType::Courier) {

            if (spaceJustPressed) isAttackingCommand = true;
        } else {

            if (isSpacePressed) isAttackingCommand = true;
        }
    }

    bool hasTarget = false;
    math::Vec2f targetPos;
    float targetRadBlocks = 0.0f;
    auto* action = registry.try_get<ecs::ActionTarget>(entity);

    if (action && registry.valid(action->target)) {
        if (registry.all_of<ecs::Health>(action->target)) {
            auto* health = registry.try_get<ecs::Health>(action->target);
            if (health && health->current > 0) {
                hasTarget = true;
                auto& tWp = registry.get<ecs::WorldPos>(action->target);
                targetPos = {tWp.wx, tWp.wy};

                if (auto* hb = registry.try_get<ecs::Hitbox>(action->target)) targetRadBlocks = hb->radius / 64.0f;
                else if (auto* ca = registry.try_get<ecs::ClickArea>(action->target)) targetRadBlocks = ca->radius / 64.0f;
            } else {
                registry.remove<ecs::ActionTarget>(entity);
            }
        } else if (registry.all_of<ecs::TaskArea>(action->target)) {
            hasTarget = true;
            auto& task = registry.get<ecs::TaskArea>(action->target);
            targetPos = {(task.dropoffStart.x + task.dropoffEnd.x)/2.f, (task.dropoffStart.y + task.dropoffEnd.y)/2.f};
            targetRadBlocks = 1.0f;
        } else {
            registry.remove<ecs::ActionTarget>(entity);
        }
    }

    if (hasTarget) {
        float dx = targetPos.x - wp.wx;
        float dy = targetPos.y - wp.wy;
        float dist = std::sqrt(dx * dx + dy * dy);

        float effectiveRadius = uData.attackRadius + targetRadBlocks;

        if (dist > effectiveRadius - 0.1f) {
            if (!registry.all_of<ecs::Path>(entity)) moveIntent = {dx / dist, dy / dist};
            vel.facingAngle = std::atan2(dy, dx) * 180.0f / 3.14159265f;
        } else {
            isAttackingCommand = true;
            registry.remove<ecs::Path>(entity);
            vel.facingAngle = std::atan2(dy, dx) * 180.0f / 3.14159265f;
        }
    }

    if (isAttackingCommand) {
        if (anim.currentAnim == 0 || anim.currentAnim == 1) {
            anim.currentAnim = 2; anim.currentFrame = 0; anim.elapsedTime = 0.0f;
            anim.isActionLocked = true;
            anim.numFrames = (uData.type == ecs::UnitType::Warrior) ? 4 : 6;
            if (uData.type == ecs::UnitType::Courier) anim.numFrames = 4;
            anim.damageDealt = false;
        } else if ((anim.currentAnim == 2 || anim.currentAnim == 3) && anim.currentFrame >= 2) {
            if (uData.type == ecs::UnitType::Warrior) anim.comboRequested = true;
        }
    }

    if (anim.currentAnim == 2 || anim.currentAnim == 3) {
        if (anim.currentFrame == 0) anim.damageDealt = false;
        int damageFrame = (uData.type == ecs::UnitType::Warrior) ? 2 : 3;

        if (uData.type == ecs::UnitType::Courier) damageFrame = 1;

        if (anim.currentFrame == damageFrame && !anim.damageDealt) {
            anim.damageDealt = true;

            auto* actionTarget = registry.try_get<ecs::ActionTarget>(entity);
            if (actionTarget && registry.valid(actionTarget->target)) {

                if (registry.all_of<ecs::TaskArea>(actionTarget->target)) {
                    if (uData.type == ecs::UnitType::Courier && uData.heldItem != ecs::HeldItem::None) {
                        auto itemToDrop = uData.heldItem;
                        auto itemSubtype = uData.heldItemSubtype;

                        uData.heldItem = ecs::HeldItem::None;
                        uData.heldItemSubtype = 0;

                        anim.isActionLocked = false;

                        auto& task = registry.get<ecs::TaskArea>(actionTarget->target);

                        float minX = std::min(task.dropoffStart.x, task.dropoffEnd.x);
                        float maxX = std::max(task.dropoffStart.x, task.dropoffEnd.x);
                        float minY = std::min(task.dropoffStart.y, task.dropoffEnd.y);
                        float maxY = std::max(task.dropoffStart.y, task.dropoffEnd.y);

                        int itemsInZone = 0;
                        if (itemToDrop == ecs::HeldItem::Wood) {
                            auto logView = registry.view<ecs::LogTag, ecs::WorldPos>();
                            for (auto l : logView) {
                                auto& lWp = logView.get<ecs::WorldPos>(l);
                                if (lWp.wx >= minX && lWp.wx <= maxX && lWp.wy >= minY && lWp.wy <= maxY) itemsInZone++;
                            }
                        } else if (itemToDrop == ecs::HeldItem::Rock) {
                            auto rockView = registry.view<ecs::SmallRockTag, ecs::WorldPos>();
                            for (auto r : rockView) {
                                auto& rWp = rockView.get<ecs::WorldPos>(r);
                                if (rWp.wx >= minX && rWp.wx <= maxX && rWp.wy >= minY && rWp.wy <= maxY) itemsInZone++;
                            }
                        }

                        float gridSpace = 0.5f;
                        int cols = std::max(1, static_cast<int>((maxX - minX) / gridSpace));
                        int rows = std::max(1, static_cast<int>((maxY - minY) / gridSpace));

                        int cellX = itemsInZone % cols;
                        int cellY = (itemsInZone / cols) % rows;

                        float dropX = minX + cellX * gridSpace + gridSpace / 2.0f;
                        float dropY = minY + cellY * gridSpace + gridSpace / 2.0f;

                        dropX = std::clamp(dropX, minX + 0.1f, maxX - 0.1f);
                        dropY = std::clamp(dropY, minY + 0.1f, maxY - 0.1f);

                        registry.remove<ecs::ActionTarget>(entity);

                        auto* tilesPtr = registry.ctx().find<const rendering::TileHandler*>();
                        if (tilesPtr) {
                            if (itemToDrop == ecs::HeldItem::Wood) {
                                world::ResourceSystem::spawnLogs(registry, **tilesPtr, {dropX, dropY}, wp.wz, 1, &chunkManager, entt::null, true, itemSubtype);
                            } else if (itemToDrop == ecs::HeldItem::Rock) {
                                world::ResourceSystem::spawnSmallRocks(registry, **tilesPtr, {dropX, dropY}, wp.wz, 1, &chunkManager, entt::null, true, itemSubtype);
                            }
                        }
                    }
                }
                else {
                    bool canHit = true;
                    if (registry.all_of<ecs::RockTag>(actionTarget->target) && uData.type != ecs::UnitType::Miner) canHit = false;
                    if (registry.all_of<ecs::TreeTag>(actionTarget->target) && uData.type != ecs::UnitType::Lumberjack) canHit = false;
                    if (registry.all_of<ecs::BushTag>(actionTarget->target) && uData.type != ecs::UnitType::Lumberjack && uData.type != ecs::UnitType::Warrior) canHit = false;
                    if (registry.all_of<ecs::LogTag>(actionTarget->target) && (uData.type != ecs::UnitType::Courier || uData.heldItem != ecs::HeldItem::None)) canHit = false;
                    if (registry.all_of<ecs::SmallRockTag>(actionTarget->target) && (uData.type != ecs::UnitType::Courier || uData.heldItem != ecs::HeldItem::None)) canHit = false;

                    if (canHit) {
                        auto& targetHealth = registry.get<ecs::Health>(actionTarget->target);
                        if (targetHealth.current > 0) {
                            targetHealth.current -= 1;

                            auto* tilesPtr = registry.ctx().find<const rendering::TileHandler*>();
                            if (tilesPtr && !registry.all_of<ecs::LogTag>(actionTarget->target) && !registry.all_of<ecs::SmallRockTag>(actionTarget->target)) {
                                world::ResourceSystem::onResourceHit(registry, actionTarget->target, **tilesPtr);
                            }

                            if (targetHealth.current <= 0) {
                                anim.isActionLocked = false; anim.currentAnim = 0; anim.currentFrame = 0; anim.comboRequested = false;
                                entt::entity destroyedRes = actionTarget->target;
                                registry.remove<ecs::ActionTarget>(entity);

                                bool destroyedByCourier = false;

                                if (registry.all_of<ecs::LogTag>(destroyedRes) && uData.type == ecs::UnitType::Courier) {
                                    uData.heldItem = ecs::HeldItem::Wood;
                                    uData.heldItemSubtype = registry.get<ecs::ResourceData>(destroyedRes).type;
                                    registry.destroy(destroyedRes);
                                    destroyedByCourier = true;
                                } else if (registry.all_of<ecs::SmallRockTag>(destroyedRes) && uData.type == ecs::UnitType::Courier) {
                                    uData.heldItem = ecs::HeldItem::Rock;
                                    uData.heldItemSubtype = registry.get<ecs::ResourceData>(destroyedRes).type;
                                    registry.destroy(destroyedRes);
                                    destroyedByCourier = true;
                                }

                                if (!destroyedByCourier) {
                                    registry.remove<ecs::ClaimedTag>(destroyedRes);

                                }
                            }
                        }
                    } else {
                        anim.isActionLocked = false; anim.currentAnim = 0; anim.currentFrame = 0; anim.comboRequested = false;
                        registry.remove<ecs::ActionTarget>(entity);
                    }
                }
            } else if (isHero) {
                auto resourceGroup = registry.view<ecs::ResourceTag, ecs::WorldPos>();
                for (auto targetEnt : resourceGroup) {
                    if (!registry.all_of<ecs::Health>(targetEnt)) continue;

                    if (registry.all_of<ecs::RockTag>(targetEnt) && uData.type != ecs::UnitType::Miner) continue;
                    if (registry.all_of<ecs::TreeTag>(targetEnt) && uData.type != ecs::UnitType::Lumberjack) continue;
                    if (registry.all_of<ecs::BushTag>(targetEnt) && uData.type != ecs::UnitType::Lumberjack && uData.type != ecs::UnitType::Warrior) continue;
                    if (registry.all_of<ecs::LogTag>(targetEnt) && (uData.type != ecs::UnitType::Courier || uData.heldItem != ecs::HeldItem::None)) continue;
                    if (registry.all_of<ecs::SmallRockTag>(targetEnt) && (uData.type != ecs::UnitType::Courier || uData.heldItem != ecs::HeldItem::None)) continue;

                    auto& tWp = resourceGroup.get<ecs::WorldPos>(targetEnt);
                    float tRad = 19.2f;
                    if (auto* hb = registry.try_get<ecs::Hitbox>(targetEnt)) tRad = hb->radius;
                    else if (auto* ca = registry.try_get<ecs::ClickArea>(targetEnt)) tRad = ca->radius;

                    float dx = tWp.wx - wp.wx, dy = tWp.wy - wp.wy;
                    float distSq = dx * dx + dy * dy;
                    float effectiveRadius = uData.attackRadius + (tRad / 64.0f);

                    if (distSq <= effectiveRadius * effectiveRadius) {
                        float angleToTarget = std::atan2(dy, dx) * 180.0f / 3.14159265f;
                        float angleDiff = angleToTarget - vel.facingAngle;
                        while (angleDiff > 180.0f) angleDiff -= 360.0f;
                        while (angleDiff < -180.0f) angleDiff += 360.0f;

                        if (std::abs(angleDiff) <= uData.arcDegrees / 2.0f) {
                            auto& th = registry.get<ecs::Health>(targetEnt);
                            th.current -= 1;

                            auto* tilesPtr = registry.ctx().find<const rendering::TileHandler*>();
                            if (tilesPtr && !registry.all_of<ecs::LogTag>(targetEnt) && !registry.all_of<ecs::SmallRockTag>(targetEnt)) {
                                world::ResourceSystem::onResourceHit(registry, targetEnt, **tilesPtr);
                            }

                            if (th.current <= 0) {
                                if (registry.all_of<ecs::LogTag>(targetEnt) && uData.type == ecs::UnitType::Courier) {
                                    uData.heldItem = ecs::HeldItem::Wood;
                                    uData.heldItemSubtype = registry.get<ecs::ResourceData>(targetEnt).type;
                                    registry.destroy(targetEnt);
                                } else if (registry.all_of<ecs::SmallRockTag>(targetEnt) && uData.type == ecs::UnitType::Courier) {
                                    uData.heldItem = ecs::HeldItem::Rock;
                                    uData.heldItemSubtype = registry.get<ecs::ResourceData>(targetEnt).type;
                                    registry.destroy(targetEnt);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    return moveIntent;
}

}