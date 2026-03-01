#pragma once
#include "Rendering/Perspective.hpp"
#include <entt/entt.hpp>

namespace wr::simulation {
    class UnitAnimationSystem {
    public:
        static void update(entt::registry& registry, float dt, rendering::ViewDirection viewDir);
    };
}