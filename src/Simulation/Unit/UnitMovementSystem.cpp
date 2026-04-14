#include "../../../include/Simulation/Unit/UnitMovementSystem.hpp"
#include <cmath>
#include <chrono>
#include <SFML/Window/Keyboard.hpp>

namespace wr::simulation {

math::Vec2f UnitMovementSystem::getKeyboardWorldVector(rendering::ViewDirection viewDir) {
    float dxScreen = 0.0f, dyScreen = 0.0f;
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::W)) dyScreen -= 1.0f;
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::S)) dyScreen += 1.0f;
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::A)) dxScreen -= 1.0f;
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::D)) dxScreen += 1.0f;

    float length = std::sqrt(dxScreen * dxScreen + dyScreen * dyScreen);
    if (length > 0.0f) { dxScreen /= length; dyScreen /= length; }

    math::Vec2f worldVec{0.0f, 0.0f};
    switch (viewDir) {
        case rendering::ViewDirection::North: worldVec = {dxScreen, dyScreen}; break;
        case rendering::ViewDirection::East:  worldVec = {dyScreen, -dxScreen}; break;
        case rendering::ViewDirection::South: worldVec = {-dxScreen, -dyScreen}; break;
        case rendering::ViewDirection::West:  worldVec = {-dyScreen, dxScreen}; break;
    }
    return worldVec;
}

void UnitMovementSystem::applyMovement(entt::entity entity, ecs::WorldPos& wp, ecs::Velocity& vel, ecs::AnimationState& anim, ecs::WorkerState& wState, ecs::Path* path, ecs::PathRequest* req, const math::Vec2f& moveIntent, DeferredQueues& dq) {
    bool isJumping = (wp.zJump > 0.0f || wp.zJumpVel != 0.0f);
    math::Vec2f finalIntent = moveIntent;

    bool hasDirectIntent = (moveIntent.x != 0.0f || moveIntent.y != 0.0f);

    if (hasDirectIntent) {
        if (path) dq.removePath.push_back(entity);
        if (req) dq.removePathRequest.push_back(entity);
    }

    if (!hasDirectIntent) {
        if (req) {
            if (req->futurePath.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                auto newPath = req->futurePath.get();
                if (newPath.empty()) {
                    dq.removeActionTarget.push_back(entity);
                    wState.currentTask = entt::null;
                    dq.removeWorkerTag.push_back(entity);
                } else {
                    dq.setPath.push_back({entity, ecs::Path{newPath, 0}});
                }
                dq.removePathRequest.push_back(entity);
            } else {
                vel.dx = 0.0f;
                vel.dy = 0.0f;
                return;
            }
        }

        if (path) {
            while (path->currentIndex < path->waypoints.size()) {
                math::Vec2f target = path->waypoints[path->currentIndex];
                float dx = target.x - wp.wx;
                float dy = target.y - wp.wy;
                float dist = std::sqrt(dx * dx + dy * dy);

                if (dist < 0.65f) {
                    path->currentIndex++;
                } else {
                    finalIntent = {dx / dist, dy / dist};
                    if (path->currentIndex == path->waypoints.size() - 1 && dist < 1.5f) {
                        finalIntent.x *= (dist / 1.5f);
                        finalIntent.y *= (dist / 1.5f);
                    }
                    break;
                }
            }
            if (path->currentIndex >= path->waypoints.size()) {
                dq.removePath.push_back(entity);
            }
        }
    }

    if (!isJumping && !anim.isActionLocked) {
        float speedMultiplier = wp.isOnWater ? 0.4f : 1.0f;
        vel.dx = finalIntent.x * vel.baseSpeed * speedMultiplier;
        vel.dy = finalIntent.y * vel.baseSpeed * speedMultiplier;

        if ((vel.dx != 0.0f || vel.dy != 0.0f)) {
            vel.facingAngle = std::atan2(vel.dy, vel.dx) * 180.0f / 3.14159265f;
        }
    } else if (anim.isActionLocked) {
        vel.dx = 0.0f;
        vel.dy = 0.0f;
    }
}

}