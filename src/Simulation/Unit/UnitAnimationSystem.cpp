#include "../../../include/Simulation/Unit/UnitAnimationSystem.hpp"
#include "ECS/Components.hpp"
#include "ECS/Tags.hpp"
#include "Rendering/TileHandler.hpp"
#include <cmath>
#include <algorithm>

namespace wr::simulation {

void UnitAnimationSystem::update(entt::registry& registry, float dt, rendering::ViewDirection viewDir, const rendering::Camera& camera) {
    const auto* tilesPtr = registry.ctx().find<const rendering::TileHandler*>();

    auto unitGroup = registry.view<ecs::UnitTag, ecs::Velocity, ecs::AnimationState, ecs::WorldPos, ecs::SpriteComponent, ecs::UnitData>();

    math::Vec2i64 camAbs = camera.getAbsolutePosition();
    float camWx = static_cast<float>(camAbs.x) + (camera.getSubPixelX() / 64.0f);
    float camWy = static_cast<float>(camAbs.y) + (camera.getSubPixelY() / 64.0f);

    float cullRadius = (1600.0f / 64.0f) * std::max(1.0f, camera.getZoom());

    for (auto entity : unitGroup) {
        auto& wp = unitGroup.get<ecs::WorldPos>(entity);

        bool isVisible = (std::abs(wp.wx - camWx) <= cullRadius && std::abs(wp.wy - camWy) <= cullRadius);
        bool isIdle = registry.all_of<ecs::IdleTag>(entity);

        if (isIdle && !isVisible) {
            continue;
        }

        auto& vel = unitGroup.get<ecs::Velocity>(entity);
        auto& anim = unitGroup.get<ecs::AnimationState>(entity);
        auto& uData = unitGroup.get<ecs::UnitData>(entity);

        float speedSq = vel.dx * vel.dx + vel.dy * vel.dy;
        bool isMoving = speedSq > 0.01f;
        bool isJumping = (wp.zJump > 0.0f || wp.zJumpVel != 0.0f);

        anim.elapsedTime += dt;
        bool animFinished = false;

        if (anim.elapsedTime >= anim.frameDuration) {
            anim.elapsedTime -= anim.frameDuration;
            anim.currentFrame++;
            if (anim.currentFrame >= anim.numFrames) {
                anim.currentFrame = 0;
                animFinished = true;
            }
        }

        if ((anim.currentAnim == 2 || anim.currentAnim == 3) && animFinished) {
            if (anim.comboRequested && uData.type == ecs::UnitType::Warrior) {
                anim.currentAnim = (anim.currentAnim == 2) ? 3 : 2;
                anim.currentFrame = 0;
                anim.comboRequested = false;
                anim.numFrames = 4;
                anim.damageDealt = false;
            } else {
                anim.currentAnim = 0;
                anim.isActionLocked = false;
                anim.numFrames = 6;
            }
        }

        if (!anim.isActionLocked) {
            bool shouldRun = isMoving && (!wp.isOnWater || isJumping);
            if (shouldRun) {
                if (anim.currentAnim != 1) {
                    anim.currentAnim = 1;
                    anim.numFrames = 6;
                    anim.currentFrame = 0;
                    anim.elapsedTime = 0.0f;
                }
            } else {
                if (anim.currentAnim != 0) {
                    anim.currentAnim = 0;
                    anim.numFrames = 6;
                    anim.currentFrame = 0;
                    anim.elapsedTime = 0.0f;
                }
            }
        }

        if (isVisible) {
            auto& spr = unitGroup.get<ecs::SpriteComponent>(entity).sprite;

            if (tilesPtr) {
                const rendering::TileHandler& tiles = **tilesPtr;
                if (uData.type == ecs::UnitType::Warrior) {
                    if (anim.currentAnim == 0) spr.setTexture(tiles.getWarriorIdleTexture());
                    else if (anim.currentAnim == 1) spr.setTexture(tiles.getWarriorRunTexture());
                    else if (anim.currentAnim == 2) spr.setTexture(tiles.getWarriorAttack1Texture());
                    else spr.setTexture(tiles.getWarriorAttack2Texture());
                } else if (uData.type == ecs::UnitType::Lumberjack) {
                    if (anim.currentAnim == 0) spr.setTexture(tiles.getPawnIdleAxeTexture());
                    else if (anim.currentAnim == 1) spr.setTexture(tiles.getPawnRunAxeTexture());
                    else spr.setTexture(tiles.getPawnAttackAxeTexture());
                } else if (uData.type == ecs::UnitType::Miner) {
                    if (anim.currentAnim == 0) spr.setTexture(tiles.getPawnIdlePickaxeTexture());
                    else if (anim.currentAnim == 1) spr.setTexture(tiles.getPawnRunPickaxeTexture());
                    else spr.setTexture(tiles.getPawnAttackPickaxeTexture());
                } else if (uData.type == ecs::UnitType::Builder) {
                    if (anim.currentAnim == 0) spr.setTexture(tiles.getPawnIdleHammerTexture());
                    else if (anim.currentAnim == 1) spr.setTexture(tiles.getPawnRunHammerTexture());
                    else spr.setTexture(tiles.getPawnAttackHammerTexture());
                } else if (uData.type == ecs::UnitType::Courier) {
                    if (uData.heldItem == ecs::HeldItem::Wood) {
                        if (anim.currentAnim == 1) spr.setTexture(tiles.getPawnRunWoodTexture());
                        else spr.setTexture(tiles.getPawnIdleWoodTexture());
                    } else if (uData.heldItem == ecs::HeldItem::Rock) {
                        if (anim.currentAnim == 1) spr.setTexture(tiles.getPawnRunRockTexture());
                        else spr.setTexture(tiles.getPawnIdleRockTexture());
                    } else {
                        if (anim.currentAnim == 1) spr.setTexture(tiles.getPawnRunTexture());
                        else spr.setTexture(tiles.getPawnIdleTexture());
                    }
                }
            }

            if (!anim.isActionLocked && isMoving) {
                float screenDx = 0.0f;
                switch (viewDir) {
                    case rendering::ViewDirection::North: screenDx = vel.dx;  break;
                    case rendering::ViewDirection::East:  screenDx = -vel.dy; break;
                    case rendering::ViewDirection::South: screenDx = -vel.dx; break;
                    case rendering::ViewDirection::West:  screenDx = vel.dy;  break;
                }
                if (screenDx < -0.01f) spr.setScale(-1.0f, 1.0f);
                else if (screenDx > 0.01f) spr.setScale(1.0f, 1.0f);
            }

            int texX = anim.currentFrame * anim.frameWidth;
            spr.setTextureRect(sf::IntRect(texX, 0, anim.frameWidth, anim.frameHeight));
        }
    }
}

}