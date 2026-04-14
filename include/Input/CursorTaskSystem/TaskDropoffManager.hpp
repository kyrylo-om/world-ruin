#pragma once

#include "Core/Math.hpp"
#include "ECS/Components.hpp"
#include <entt/entt.hpp>

namespace wr::input {
    class TaskDropoffManager {
    public:
        static void trySetDropoffArea(entt::registry& registry, math::Vec2f start, math::Vec2f end);
        static bool hasGroundRes(entt::registry& registry, const ecs::PendingTaskArea& pArea);
    };
}