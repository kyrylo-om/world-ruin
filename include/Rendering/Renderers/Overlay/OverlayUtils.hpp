#pragma once

#include "Rendering/RenderContext.hpp"
#include "Core/Math.hpp"
#include "World/Chunk.hpp"
#include <unordered_map>
#include <memory>
#include <SFML/System/Vector2.hpp>

namespace wr::rendering::overlay {
    std::pair<uint8_t, float> getElevationInfo(const std::unordered_map<math::Vec2i64, std::unique_ptr<world::Chunk>, math::SpatialHash>& chunks, math::Vec2f start, math::Vec2f end);
    sf::Vector2f toScreen(const RenderContext& ctx, math::Vec2f w, float zHeight);
}