#pragma once

#include "../RenderContext.hpp"
#include "ECS/Components.hpp"
#include <entt/entt.hpp>
#include <vector>
#include <SFML/Graphics/Font.hpp>

namespace wr::rendering {

    struct ActiveDragData {
        bool isDragging{false};
        math::Vec2f startWorld{0.0f, 0.0f};
        math::Vec2f endWorld{0.0f, 0.0f};
        sf::Color color{255, 255, 255, 255};
    };

    class OverlayRenderer {
    public:

        static void renderLayer(const RenderContext& ctx,
                                entt::registry& registry,
                                const std::unordered_map<math::Vec2i64, std::unique_ptr<world::Chunk>, math::SpatialHash>& chunks,
                                const std::vector<entt::entity>& pendingList,
                                const std::vector<entt::entity>& taskList,
                                const ActiveDragData& activeDrag,
                                uint8_t layerMaxLevel,
                                sf::Font& gameFont);

        static void renderText(const RenderContext& ctx,
                               entt::registry& registry,
                               const std::unordered_map<math::Vec2i64, std::unique_ptr<world::Chunk>, math::SpatialHash>& chunks,
                               sf::Font& gameFont);

        static std::pair<uint8_t, float> getElevationInfo(const std::unordered_map<math::Vec2i64, std::unique_ptr<world::Chunk>, math::SpatialHash>& chunks, math::Vec2f start, math::Vec2f end);
        static sf::Vector2f toScreen(const RenderContext& ctx, math::Vec2f w, float zHeight);
    };

}