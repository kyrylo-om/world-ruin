#pragma once

#include "Core/Math.hpp"
#include "World/ChunkManager.hpp"
#include <entt/entt.hpp>

namespace wr::input {

    class CursorBuilderSystem {
    public:
        void update(entt::registry& registry, world::ChunkManager& chunkManager, const math::Vec2f& hoveredPos);

        [[nodiscard]] bool isBuilderModeActive() const noexcept { return m_builderModeActive; }

    private:
        bool m_builderModeActive{false};
        int m_selectedBuilding{0};
        bool m_canPlace{false};
        math::Vec2f m_placementPos{0.0f, 0.0f};

        bool m_prevB{false};
        bool m_prev1{false};
        bool m_prevEnter{false};
    };

}