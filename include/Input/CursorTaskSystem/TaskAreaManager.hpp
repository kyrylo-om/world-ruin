#pragma once

#include "Core/Math.hpp"
#include <entt/entt.hpp>

namespace wr::input {
    class TaskAreaManager {
    public:
        static void addPendingArea(entt::registry& registry, math::Vec2f start, math::Vec2f end);
        static void removeHoveredArea(entt::registry& registry, math::Vec2f hoveredPos);
        static void selectHoveredArea(entt::registry& registry, math::Vec2f hoveredPos);
        static void addBuildingTarget(entt::registry& registry, entt::entity building);
    };
}