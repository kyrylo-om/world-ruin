#include "Rendering/Renderers/HUDRenderer.hpp"
#include "ECS/Components.hpp"
#include "ECS/Tags.hpp"
#include "Rendering/TileHandler.hpp"
#include <SFML/Graphics/Sprite.hpp>
#include <algorithm>

namespace wr::rendering {

void HUDRenderer::render(const RenderContext& ctx, entt::registry& registry) {
    auto selectedView = registry.view<ecs::SelectedTag, ecs::UnitData, ecs::Health>();
    int count = std::distance(selectedView.begin(), selectedView.end());

    if (count == 0) return;

    auto drawBar = [&](const sf::Texture& baseTex, const sf::Texture& fillTex, float x, float y, int midSegments, float hpPercent, float fillYOffset, float fillHeight, float leftWoodThick, float rightWoodThick, float xScale) {
        float curX = x;

        sf::Sprite left(baseTex, sf::IntRect(0, 0, 64, 64));
        left.setScale(xScale, 1.0f);
        left.setPosition(curX, y);
        ctx.window.draw(left);
        curX += 64.0f * xScale;

        for (int i = 0; i < midSegments; ++i) {
            sf::Sprite mid(baseTex, sf::IntRect(128, 0, 64, 64));
            mid.setScale(xScale, 1.0f);
            mid.setPosition(curX, y);
            ctx.window.draw(mid);
            curX += 64.0f * xScale;
        }

        sf::Sprite right(baseTex, sf::IntRect(256, 0, 64, 64));
        right.setScale(xScale, 1.0f);
        right.setPosition(curX, y);
        ctx.window.draw(right);

        float scaledLeftWoodThick = leftWoodThick * xScale;
        float scaledRightWoodThick = rightWoodThick * xScale;

        float totalInnerWidth = (64.0f * xScale) - scaledLeftWoodThick + (midSegments * 64.0f * xScale) + (64.0f * xScale) - scaledRightWoodThick;

        float fillW = totalInnerWidth * hpPercent;

        if (fillW > 0.0f) {
            sf::Sprite fill(fillTex);
            fill.setTextureRect(sf::IntRect(0, 0, static_cast<int>(fillW), 64));
            fill.setScale(1.0f, fillHeight / 64.0f);
            fill.setPosition(x + scaledLeftWoodThick, y + fillYOffset);
            ctx.window.draw(fill);
        }
    };

    sf::View oldView = ctx.window.getView();
    ctx.window.setView(ctx.window.getDefaultView());

    if (count == 1) {
        entt::entity e = *selectedView.begin();
        const auto& uData = selectedView.get<ecs::UnitData>(e);
        const auto& health = selectedView.get<ecs::Health>(e);

        float startX = 20.0f;
        float startY = 20.0f;

        float iconScale = 0.75f;

        sf::Texture* avatarTex = nullptr;
        if (uData.type == ecs::UnitType::Warrior) avatarTex = const_cast<sf::Texture*>(&ctx.tiles.getAvatarWarrior());
        else if (uData.type == ecs::UnitType::Courier) avatarTex = const_cast<sf::Texture*>(&ctx.tiles.getAvatarCourier());
        else avatarTex = const_cast<sf::Texture*>(&ctx.tiles.getAvatarWorker());

        sf::Sprite avatar(*avatarTex);
        avatar.setPosition(startX, startY);
        avatar.setScale(iconScale, iconScale);
        ctx.window.draw(avatar);

        float hpPercent = std::clamp(static_cast<float>(health.current) / static_cast<float>(health.max), 0.0f, 1.0f);
        int segments = std::max(1, health.max / 25) - 1;

        float barX = startX + (192.0f * iconScale) + 10.0f;
        float barY = startY + (192.0f * iconScale) * 0.5f - 32.0f;

        float fillYOffset = -5.0f;
        float fillHeight = 73.5f;
        float leftWoodThick = 48.5f;
        float rightWoodThick = 48.0f;

        float xScale = 1.5f;

        drawBar(ctx.tiles.getBigBarBaseTexture(), ctx.tiles.getBigBarFillTexture(),
                barX, barY, segments, hpPercent, fillYOffset, fillHeight, leftWoodThick, rightWoodThick, xScale);
    } else {
        float startX = 20.0f;
        float startY = 20.0f;
        float yOffset = 75.0f;

        int drawnCount = 0;
        for (auto e : selectedView) {
            if (drawnCount >= 10) break;

            const auto& uData = selectedView.get<ecs::UnitData>(e);
            const auto& health = selectedView.get<ecs::Health>(e);

            float iconScale = 0.35f;

            sf::Texture* avatarTex = nullptr;
            if (uData.type == ecs::UnitType::Warrior) avatarTex = const_cast<sf::Texture*>(&ctx.tiles.getAvatarWarrior());
            else if (uData.type == ecs::UnitType::Courier) avatarTex = const_cast<sf::Texture*>(&ctx.tiles.getAvatarCourier());
            else avatarTex = const_cast<sf::Texture*>(&ctx.tiles.getAvatarWorker());

            sf::Sprite avatar(*avatarTex);
            avatar.setPosition(startX, startY + drawnCount * yOffset);
            avatar.setScale(iconScale, iconScale);
            ctx.window.draw(avatar);

            float hpPercent = std::clamp(static_cast<float>(health.current) / static_cast<float>(health.max), 0.0f, 1.0f);
            int segments = std::max(1, health.max / 25) - 1;

            float barX = startX + (192.0f * iconScale) - 15.0f;
            float barY = startY + drawnCount * yOffset + (192.0f * iconScale) * 0.5f - 32.0f;

            float fillYOffset = -53.0f;
            float fillHeight = 172.5f;
            float leftWoodThick = 55.0f;
            float rightWoodThick = 55.0f;

            float xScale = 1.0f;

            drawBar(ctx.tiles.getSmallBarBaseTexture(), ctx.tiles.getSmallBarFillTexture(),
                    barX, barY, segments, hpPercent, fillYOffset, fillHeight, leftWoodThick, rightWoodThick, xScale);

            drawnCount++;
        }
    }

    ctx.window.setView(oldView);
}

}