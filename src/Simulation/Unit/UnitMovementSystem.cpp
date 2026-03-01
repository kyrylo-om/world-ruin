#include "../../../include/Simulation/Unit/UnitMovementSystem.hpp"
#include "ECS/Components.hpp"
#include <cmath>

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

void UnitMovementSystem::applyMovement(entt::registry& registry, entt::entity entity, const math::Vec2f& moveIntent) {
    auto& vel = registry.get<ecs::Velocity>(entity);
    auto& anim = registry.get<ecs::AnimationState>(entity);
    auto& wp = registry.get<ecs::WorldPos>(entity);

    bool isJumping = (wp.zJump > 0.0f || wp.zJumpVel != 0.0f);
    math::Vec2f finalIntent = moveIntent;

    bool hasDirectIntent = (moveIntent.x != 0.0f || moveIntent.y != 0.0f);

    if (hasDirectIntent && registry.all_of<ecs::Path>(entity)) {
        registry.remove<ecs::Path>(entity);
    }

    if (!hasDirectIntent) {
        if (auto* path = registry.try_get<ecs::Path>(entity)) {
            if (path->currentIndex < path->waypoints.size()) {
                math::Vec2f target = path->waypoints[path->currentIndex];
                float dx = target.x - wp.wx;
                float dy = target.y - wp.wy;
                float dist = std::sqrt(dx * dx + dy * dy);

                if (dist < 0.2f) {
                    path->currentIndex++;
                } else {
                    finalIntent = {dx / dist, dy / dist};
                }
            } else {
                registry.remove<ecs::Path>(entity);
            }
        }
    }

    if (!isJumping && !anim.isActionLocked) {
        float speedMultiplier = wp.isOnWater ? 0.4f : 1.0f;
        vel.dx = finalIntent.x * vel.baseSpeed * speedMultiplier;
        vel.dy = finalIntent.y * vel.baseSpeed * speedMultiplier;

        if ((vel.dx != 0.0f || vel.dy != 0.0f) && !registry.all_of<ecs::ActionTarget>(entity)) {
            vel.facingAngle = std::atan2(vel.dy, vel.dx) * 180.0f / 3.14159265f;
        }
    } else if (anim.isActionLocked) {
        vel.dx = 0.0f;
        vel.dy = 0.0f;
    }
}

}