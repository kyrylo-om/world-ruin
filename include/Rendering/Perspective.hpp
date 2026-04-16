#pragma once

#include "../Core/Types.hpp"
#include "Camera.hpp"
#include <entt/entt.hpp>

namespace wr::rendering {

    enum class ViewDirection : uint8_t {
        North = 0,
        East  = 1,
        South = 2,
        West  = 3
    };

    class Perspective {
    public:

        static void updateScreenPositions(entt::registry& registry, const Camera& camera, ViewDirection dir) noexcept;

        [[nodiscard]] static math::Vec2f rotateCoordinate(const math::Vec2f& relativePos, float angleDegrees) noexcept;

        [[nodiscard]] static math::Vec2f rotateCoordinate(const math::Vec2f& relativePos, ViewDirection dir) noexcept;
    };

}