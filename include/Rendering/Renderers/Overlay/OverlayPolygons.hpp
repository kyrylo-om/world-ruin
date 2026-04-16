#pragma once

#include "Rendering/RenderContext.hpp"
#include "Rendering/Renderers/OverlayRenderer.hpp"
#include <entt/entt.hpp>
#include <unordered_map>
#include <memory>
#include <vector>

namespace wr::rendering::overlay {
    class OverlayPolygons {
    public:
        static void render(const RenderContext& ctx,
                           const std::unordered_map<math::Vec2i64, std::unique_ptr<world::Chunk>, math::SpatialHash>& chunks,
                           const std::vector<entt::entity>& pendingList,
                           const std::vector<entt::entity>& taskList,
                           const ActiveDragData& activeDrag,
                           uint8_t layerMaxLevel,
                           entt::registry& registry,
                           sf::Font& gameFont);
    };
}