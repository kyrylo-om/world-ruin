#pragma once

#include "Core/Math.hpp"
#include "ECS/Components.hpp"
#include <entt/entt.hpp>

namespace wr::input {

    class CursorTaskSystem {
    public:
        void update(entt::registry& registry, float dt);
        void addPendingArea(entt::registry& registry, math::Vec2f start, math::Vec2f end);
        void addBuildingTarget(entt::registry& registry, entt::entity building);
        void removeHoveredArea(entt::registry& registry, math::Vec2f hoveredPos);
        void selectHoveredArea(entt::registry& registry, math::Vec2f hoveredPos);
        void trySetDropoffArea(entt::registry& registry, math::Vec2f start, math::Vec2f end);

        [[nodiscard]] bool isTaskModeActive() const noexcept { return m_taskModeActive; }
        [[nodiscard]] bool hasPendingTask(entt::registry& registry) const noexcept;

    private:
        bool m_taskModeActive{false};
        int m_globalTaskId{1};

        bool m_prevQ{false};
        bool m_prevF{false};
        bool m_prevEnter{false};

        bool m_prev1{false}, m_prev2{false}, m_prev3{false}, m_prev4{false}, m_prev5{false};
    };

}