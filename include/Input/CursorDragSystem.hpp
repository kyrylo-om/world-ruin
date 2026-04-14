#pragma once
#include "Core/Math.hpp"
#include "World/ChunkManager.hpp"
#include "Input/CursorTaskSystem.hpp"
#include <SFML/Graphics/Color.hpp>
#include <entt/entt.hpp>

namespace wr::input {

    class CursorDragSystem {
    public:
        void startDrag(const math::Vec2f& worldPos) noexcept;
        bool endDrag(entt::registry& registry, world::ChunkManager& chunkManager, const math::Vec2f& exactHoveredPos, CursorTaskSystem& taskSystem) noexcept;

        [[nodiscard]] bool isDragging() const noexcept { return m_isDragging; }
        [[nodiscard]] math::Vec2f getDragStart() const noexcept { return m_dragStart; }
        [[nodiscard]] sf::Color getDragColor(bool isTaskMode, bool isE) const noexcept;

    private:
        bool m_isDragging{false};
        math::Vec2f m_dragStart{0.0f, 0.0f};
    };

}