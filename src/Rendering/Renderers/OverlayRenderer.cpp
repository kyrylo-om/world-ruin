#include "Rendering/Renderers/OverlayRenderer.hpp"
#include "Rendering/Renderers/Overlay/OverlayUtils.hpp"
#include "Rendering/Renderers/Overlay/OverlayPolygons.hpp"
#include "Rendering/Renderers/Overlay/OverlayText.hpp"

namespace wr::rendering {

    std::pair<uint8_t, float> OverlayRenderer::getElevationInfo(const std::unordered_map<math::Vec2i64, std::unique_ptr<world::Chunk>, math::SpatialHash>& chunks, math::Vec2f start, math::Vec2f end) {
        return overlay::getElevationInfo(chunks, start, end);
    }

    sf::Vector2f OverlayRenderer::toScreen(const RenderContext& ctx, math::Vec2f w, float zHeight) {
        return overlay::toScreen(ctx, w, zHeight);
    }

    void OverlayRenderer::renderLayer(const RenderContext& ctx,
                                      entt::registry& registry,
                                      const std::unordered_map<math::Vec2i64, std::unique_ptr<world::Chunk>, math::SpatialHash>& chunks,
                                      const std::vector<entt::entity>& pendingList,
                                      const std::vector<entt::entity>& taskList,
                                      const ActiveDragData& activeDrag,
                                      uint8_t layerMaxLevel,
                                      sf::Font& gameFont)
    {
        overlay::OverlayPolygons::render(ctx, chunks, pendingList, taskList, activeDrag, layerMaxLevel, registry, gameFont);
    }

    void OverlayRenderer::renderText(const RenderContext& ctx,
                                     entt::registry& registry,
                                     const std::unordered_map<math::Vec2i64, std::unique_ptr<world::Chunk>, math::SpatialHash>& chunks,
                                     sf::Font& gameFont)
    {
        overlay::OverlayText::render(ctx, registry, chunks, gameFont);
    }

}