#pragma once

#include "Rendering/RenderContext.hpp"
#include "Core/Math.hpp"
#include "World/Chunk.hpp"
#include <entt/entt.hpp>
#include <unordered_map>
#include <memory>
#include <SFML/Graphics/Font.hpp>

namespace wr::rendering::overlay {
    class OverlayText_TaskMode {
    public:
        static void render(const RenderContext& ctx,
                           entt::registry& registry,
                           const std::unordered_map<math::Vec2i64, std::unique_ptr<world::Chunk>, math::SpatialHash>& chunks,
                           sf::Font& gameFont,
                           float rightEdge,
                           sf::Vector2f windowSize);
    };
}