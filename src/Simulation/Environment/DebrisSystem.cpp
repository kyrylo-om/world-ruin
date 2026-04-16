#include "../../../include/Simulation/Environment/DebrisSystem.hpp"
#include "ECS/Components.hpp"
#include "ECS/Tags.hpp"
#include <cmath>
#include <vector>

namespace wr::simulation {

void DebrisSystem::update(entt::registry& registry, float dt, const rendering::Camera& camera) {
    auto debrisView = registry.view<ecs::DebrisTag, ecs::DebrisData, ecs::WorldPos, ecs::Velocity>();
    std::vector<entt::entity> deadDebris;

    for (auto e : debrisView) {
        auto& d = debrisView.get<ecs::DebrisData>(e);
        auto& dWp = debrisView.get<ecs::WorldPos>(e);
        auto& dVel = debrisView.get<ecs::Velocity>(e);

        d.lifeTimer -= dt;
        if (d.lifeTimer <= 0.0f) {
            deadDebris.push_back(e);
            continue;
        }

        if (d.isLeaf) {
            float timeActive = d.maxLife - d.lifeTimer;
            dWp.zJumpVel -= d.gravity * dt;
            if (dWp.zJumpVel < -120.0f) dWp.zJumpVel = -120.0f;
            dVel.dx *= (1.0f - 2.5f * dt);
            dVel.dy *= (1.0f - 2.5f * dt);

            float swayDx = 0.0f, swayDy = 0.0f;
            if (dWp.zJump > 0.0f) {
                float swayPhase = timeActive * 3.5f + d.randomSeed;
                swayDx = std::cos(swayPhase) * 0.8f;
                swayDy = std::sin(swayPhase * 1.1f) * 0.8f;
            }

            dWp.wx += (dVel.dx + swayDx) * dt;
            dWp.wy += (dVel.dy + swayDy) * dt;
        } else {
            dWp.wx += dVel.dx * dt;
            dWp.wy += dVel.dy * dt;
            dWp.zJumpVel -= d.gravity * dt;

            if (dWp.zJump <= 0.0f) {
                dVel.dx *= (1.0f - 8.0f * dt);
                dVel.dy *= (1.0f - 8.0f * dt);
            } else {
                dVel.dx *= (1.0f - 0.5f * dt);
                dVel.dy *= (1.0f - 0.5f * dt);
            }

            bool isMoving = (dWp.zJump > 0.0f || std::abs(dVel.dx) > 0.1f || std::abs(dVel.dy) > 0.1f);
            if (isMoving) {
                float rotSpeed = 300.0f + std::fmod(d.randomSeed, 200.0f);
                float direction = (dVel.dx + dVel.dy > 0) ? 1.0f : -1.0f;
                dVel.facingAngle += rotSpeed * direction * dt;
            }
        }

        dWp.zJump += dWp.zJumpVel * dt;

        if (dWp.zJump <= 0.0f) {
            dWp.zJump = 0.0f;
            if (d.isLeaf) {
                if (dWp.zJumpVel != 0.0f) {
                    float timeActive = d.maxLife - d.lifeTimer;
                    dVel.facingAngle = std::sin(timeActive * 3.5f + d.randomSeed) * 45.0f;
                }
                dWp.zJumpVel = 0.0f;
                dVel.dx = 0.0f;
                dVel.dy = 0.0f;
            } else {
                if (dWp.zJumpVel < -40.0f) {
                    dWp.zJumpVel *= -0.45f;
                    dVel.dx *= 0.7f;
                    dVel.dy *= 0.7f;
                    dVel.dx += (rand() % 100 / 100.0f - 0.5f) * 0.5f;
                    dVel.dy += (rand() % 100 / 100.0f - 0.5f) * 0.5f;
                    dVel.facingAngle += (rand() % 100 / 100.0f - 0.5f) * 90.0f;
                } else {
                    dWp.zJumpVel = 0.0f;
                    dVel.dx = 0.0f;
                    dVel.dy = 0.0f;
                }
            }
        }
    }

    for (auto e : deadDebris) registry.destroy(e);

    math::Vec2i64 camAbs = camera.getAbsolutePosition();
    float camWx = static_cast<float>(camAbs.x) + (camera.getSubPixelX() / 64.0f);
    float camWy = static_cast<float>(camAbs.y) + (camera.getSubPixelY() / 64.0f);
    float cullRadius = (1000.0f / 64.0f) * std::max(1.0f, camera.getZoom());

    auto oneShotView = registry.view<ecs::OneShotAnimTag, ecs::AnimationState, ecs::SpriteComponent, ecs::WorldPos>();
    std::vector<entt::entity> toDestroy;

    for (auto entity : oneShotView) {
        auto& anim = oneShotView.get<ecs::AnimationState>(entity);
        anim.elapsedTime += dt;
        if (anim.elapsedTime >= anim.frameDuration) {
            anim.elapsedTime -= anim.frameDuration;
            anim.currentFrame++;
            if (anim.currentFrame >= anim.numFrames) {
                toDestroy.push_back(entity);
            } else {
                auto& wp = oneShotView.get<ecs::WorldPos>(entity);
                if (std::abs(wp.wx - camWx) <= cullRadius && std::abs(wp.wy - camWy) <= cullRadius) {
                    auto& spr = oneShotView.get<ecs::SpriteComponent>(entity).sprite;
                    spr.setTextureRect(sf::IntRect(anim.currentFrame * anim.frameWidth, 0, anim.frameWidth, anim.frameHeight));
                }
            }
        }
    }

    for (auto e : toDestroy) registry.destroy(e);
}

}