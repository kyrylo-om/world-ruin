#pragma once

#include "Core/Math.hpp"
#include "Rendering/Perspective.hpp"
#include <entt/entt.hpp>

namespace wr::simulation {

    class UnitMovementSystem {
    public:

        static math::Vec2f getKeyboardWorldVector(rendering::ViewDirection viewDir);

        static void applyMovement(entt::registry& registry, entt::entity entity, const math::Vec2f& moveIntent);
    };

}