#pragma once

#include "../Core/Math.hpp"
#include "../Rendering/Camera.hpp"
#include "../World/ChunkManager.hpp"
#include <SFML/Graphics/RenderWindow.hpp>

namespace wr::input {

    class CursorRaycast {
    public:

        [[nodiscard]] static math::Vec2f getHoveredWorldPos(const sf::RenderWindow& window,
                                                            const rendering::Camera& camera,
                                                            world::ChunkManager& chunkManager) noexcept;

    private:
        [[nodiscard]] static math::Vec2f inverseRotateCoordinate(const math::Vec2f& relativePos, float angleDegrees) noexcept;
    };

}