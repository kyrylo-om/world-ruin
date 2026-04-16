#pragma once

#include "Core/Math.hpp"
#include "Rendering/Camera.hpp"
#include <SFML/Graphics/RenderWindow.hpp>
#include <entt/entt.hpp>

namespace wr::input {

    class CursorClickSystem {
    public:
        static bool handleMouseClick(entt::registry& registry, const sf::RenderWindow& window, const rendering::Camera& camera);
    };

}