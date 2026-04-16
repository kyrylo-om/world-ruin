#include "Rendering/Renderers/FoamRenderer.hpp"
#include "ECS/Components.hpp"
#include "ECS/Tags.hpp"
#include <SFML/Graphics/Sprite.hpp>

namespace wr::rendering {

    void FoamRenderer::render(const RenderContext& ctx, entt::registry& registry, const std::vector<entt::entity>& entities) {
        sf::FloatRect cullBounds = ctx.viewBounds;
        cullBounds.left -= 256.0f; cullBounds.top -= 256.0f;
        cullBounds.width += 512.0f; cullBounds.height += 512.0f;

        for (entt::entity e : entities) {
            const auto& sp = registry.get<ecs::ScreenPos>(e);
            float zOffset = 0.0f;
            bool isOnWater = false;

            if (const auto* wp = registry.try_get<ecs::WorldPos>(e)) {
                zOffset = wp->wz;
                isOnWater = wp->isOnWater;
            }

            if (isOnWater && cullBounds.contains(sp.x, sp.y - zOffset)) {
                sf::Sprite foamSpr(ctx.tiles.getFoamTexture());
                int frame = static_cast<int>(ctx.timeSeconds * 10.0f) % TileHandler::FOAM_FRAME_COUNT;
                foamSpr.setTextureRect(sf::IntRect(frame * 192, 0, 192, 192));
                foamSpr.setOrigin(96.0f, 96.0f);
                float cutLineY = sp.y + 16.0f;

                foamSpr.setPosition(sp.x, cutLineY - 16.0f);
                foamSpr.setRotation(0.0f);
                foamSpr.setScale(1.3f, 0.45f);
                foamSpr.setColor(sf::Color(255, 255, 255, 230));

                ctx.window.draw(foamSpr);
            }
        }
    }

}