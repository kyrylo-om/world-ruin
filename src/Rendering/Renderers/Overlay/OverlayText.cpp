#include "../../../../include/Rendering/Renderers/Overlay/OverlayText.hpp"
#include "../../../../include/Rendering/Renderers/Overlay/OverlayText_World.hpp"
#include "../../../../include/Rendering/Renderers/Overlay/OverlayText_TaskMode.hpp"
#include "../../../../include/Rendering/Renderers/Overlay/OverlayText_NormalMode.hpp"
#include "../../../../include/Rendering/Renderers/Overlay/OverlayText_BuilderMode.hpp"
#include "ECS/Components.hpp"
#include "ECS/Tags.hpp"
#include <SFML/Graphics/RenderTarget.hpp>

namespace wr::rendering::overlay {

    void OverlayText::render(const RenderContext& ctx,
                             entt::registry& registry,
                             const std::unordered_map<math::Vec2i64, std::unique_ptr<world::Chunk>, math::SpatialHash>& chunks,
                             sf::Font& gameFont)
    {

        OverlayText_World::render(ctx, registry, chunks, gameFont);

        sf::View oldView = ctx.window.getView();
        sf::Vector2f windowSize(static_cast<float>(ctx.window.getSize().x), static_cast<float>(ctx.window.getSize().y));
        sf::View hudView(sf::FloatRect(0.0f, 0.0f, windowSize.x, windowSize.y));
        ctx.window.setView(hudView);

        float rightEdge = windowSize.x - 20.0f;

        bool isTaskMode = registry.ctx().contains<ecs::TaskModeState>() && registry.ctx().get<ecs::TaskModeState>().active;
        bool isBuilderMode = registry.ctx().contains<ecs::BuilderModeState>() && registry.ctx().get<ecs::BuilderModeState>().active;

        if (isBuilderMode) {
            OverlayText_BuilderMode::render(ctx, registry, gameFont, rightEdge, windowSize);
        } else if (isTaskMode) {
            OverlayText_TaskMode::render(ctx, registry, chunks, gameFont, rightEdge, windowSize);
        } else {
            OverlayText_NormalMode::render(ctx, registry, chunks, gameFont, rightEdge, windowSize);
        }

        ctx.window.setView(oldView);
    }

}