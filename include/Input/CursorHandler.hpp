#pragma once

#include "Input/CursorTaskSystem.hpp"
#include "Input/CursorDragSystem.hpp"
#include "Input/CursorClickSystem.hpp"
#include "Core/Types.hpp"
#include "Core/Math.hpp"
#include "Rendering/Camera.hpp"
#include "World/ChunkManager.hpp"
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <entt/entt.hpp>
#include <cmath>

namespace wr::input {

    class CursorHandler {
    public:
        CursorHandler() = default;

        void init(const sf::Texture& texture) noexcept;
        void update(const sf::RenderWindow& window, const rendering::Camera& camera, world::ChunkManager& chunkManager, entt::registry& registry) noexcept;
        bool handleMouseClick(entt::registry& registry, const sf::RenderWindow& window, const rendering::Camera& camera, world::ChunkManager& chunkManager) noexcept;

        void startDrag() noexcept;
        bool endDrag(entt::registry& registry, const rendering::Camera& camera, world::ChunkManager& chunkManager) noexcept;

        void draw(sf::RenderWindow& window) const;
        void setSystemCursorVisible(sf::RenderWindow& window, bool visible) const noexcept;

        [[nodiscard]] math::Vec2i64 getHoveredWorldPos() const noexcept;
        [[nodiscard]] math::Vec2f getExactHoveredWorldPos() const noexcept { return m_exactHoveredWorldPos; }
        [[nodiscard]] bool isDragging() const noexcept { return m_dragSystem.isDragging(); }
        [[nodiscard]] math::Vec2f getDragStartWorldPos() const noexcept { return m_dragSystem.getDragStart(); }
        [[nodiscard]] sf::Color getDragColor() const noexcept { return m_dragSystem.getDragColor(m_taskSystem.isSelectingDropoff()); }

    private:
        sf::Sprite m_sprite;
        math::Vec2f m_exactHoveredWorldPos{0.0f, 0.0f};

        CursorTaskSystem m_taskSystem;
        CursorDragSystem m_dragSystem;
    };

    inline math::Vec2i64 CursorHandler::getHoveredWorldPos() const noexcept {
        return {static_cast<core::GlobalCoord>(std::floor(m_exactHoveredWorldPos.x)),
                static_cast<core::GlobalCoord>(std::floor(m_exactHoveredWorldPos.y))};
    }

}