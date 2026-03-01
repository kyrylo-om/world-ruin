#include "Rendering/Perspective.hpp"
#include "ECS/Components.hpp"
#include <cmath>

namespace wr::rendering {

    void Perspective::updateScreenPositions(entt::registry& registry, const Camera& camera, ViewDirection dir) noexcept {
        auto view = registry.view<ecs::WorldPos, ecs::ScreenPos>();

        float currentAngle = camera.getRotation();
        math::Vec2i64 camAbs = camera.getAbsolutePosition();
        float camSubX = camera.getSubPixelX();
        float camSubY = camera.getSubPixelY();

        for (auto entity : view) {
            const auto& wPos = view.get<ecs::WorldPos>(entity);
            auto& screen = view.get<ecs::ScreenPos>(entity);

            float dx = (wPos.wx - static_cast<float>(camAbs.x)) * 64.0f;
            float dy = (wPos.wy - static_cast<float>(camAbs.y)) * 64.0f;

            math::Vec2f relativePos = { dx - camSubX, dy - camSubY };

            math::Vec2f rotatedPos = rotateCoordinate(relativePos, currentAngle);

            screen.x = rotatedPos.x;
            screen.y = rotatedPos.y;
        }
    }

    math::Vec2f Perspective::rotateCoordinate(const math::Vec2f& relativePos, float angleDegrees) noexcept {
        float rad = angleDegrees * 3.14159265f / 180.0f;
        float cosA = std::cos(rad);
        float sinA = std::sin(rad);

        return {
            relativePos.x * cosA - relativePos.y * sinA,
            relativePos.x * sinA + relativePos.y * cosA
        };
    }

    math::Vec2f Perspective::rotateCoordinate(const math::Vec2f& relativePos, ViewDirection dir) noexcept {
        return rotateCoordinate(relativePos, static_cast<float>(static_cast<int>(dir) * 90));
    }

}