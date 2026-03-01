#pragma once
#include "Core/Math.hpp"
#include <entt/entt.hpp>

namespace wr::input {

    class CursorTaskSystem {
    public:
        void update(entt::registry& registry);
        void startPendingTask(entt::registry& registry, math::Vec2f start, math::Vec2f end);
        void cancelTask(entt::registry& registry);

        [[nodiscard]] bool hasPendingTask() const noexcept { return m_hasPendingTask; }
        [[nodiscard]] bool isSelectingDropoff() const noexcept { return m_isSelectingDropoff; }

        void setDropoffArea(entt::registry& registry, math::Vec2f start, math::Vec2f end);

        int getNextTaskId() { return m_globalTaskId++; }

    private:
        bool m_hasPendingTask{false};
        math::Vec2f m_pendingStart{0.0f, 0.0f};
        math::Vec2f m_pendingEnd{0.0f, 0.0f};

        bool m_isSelectingDropoff{false};
        bool m_hasDropoff{false};
        math::Vec2f m_dropoffStart{0.0f, 0.0f};
        math::Vec2f m_dropoffEnd{0.0f, 0.0f};

        bool m_prevQ{false};
        bool m_prevF{false};
        bool m_prevEnter{false};

        bool m_prev1{false};
        bool m_prev2{false};
        bool m_prev3{false};
        bool m_prev4{false};
        bool m_prev5{false};

        int m_globalTaskId{1};
    };

}