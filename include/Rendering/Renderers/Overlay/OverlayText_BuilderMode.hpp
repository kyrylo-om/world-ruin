#pragma once

#include "Rendering/RenderContext.hpp"
#include <entt/entt.hpp>
#include <SFML/Graphics/Font.hpp>

namespace wr::rendering::overlay {
    class OverlayText_BuilderMode {
    public:
        static void render(const RenderContext& ctx,
                           entt::registry& registry,
                           sf::Font& gameFont,
                           float rightEdge,
                           sf::Vector2f windowSize);
    };
}