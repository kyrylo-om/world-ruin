#include "../../../../include/Rendering/Renderers/Overlay/OverlayText_World.hpp"
#include "../../../../include/Rendering/Renderers/Overlay/OverlayUtils.hpp"
#include "Rendering/TileHandler.hpp"
#include "ECS/Components.hpp"
#include "ECS/Tags.hpp"
#include <SFML/Graphics/Text.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/RenderTarget.hpp>
#include <algorithm>
#include <cmath>

namespace wr::rendering::overlay {

void OverlayText_World::render(const RenderContext& ctx,
                               entt::registry& registry,
                               const std::unordered_map<math::Vec2i64, std::unique_ptr<world::Chunk>, math::SpatialHash>& chunks,
                               sf::Font& gameFont)
{
    auto taskView = registry.view<ecs::TaskArea>();
    auto pendingView = registry.view<ecs::PendingTaskArea>();
    auto buildingView = registry.view<ecs::BuildingTag, ecs::WorldPos, ecs::ConstructionData>(entt::exclude<ecs::CityStorageTag>);
    auto storageView = registry.view<ecs::CityStorageTag, ecs::WorldPos, ecs::ConstructionData>();

    auto drawAreaLabel = [&](math::Vec2f startW, math::Vec2f endW, const std::string& textStr, int fontSize = 8, float errorTimer = 0.0f) {
        float z = overlay::getElevationInfo(chunks, startW, endW).second;

        float minX = std::min(startW.x, endW.x);
        float maxX = std::max(startW.x, endW.x);
        float minY = std::min(startW.y, endW.y);
        float maxY = std::max(startW.y, endW.y);

        sf::Vector2f sp0 = overlay::toScreen(ctx, {minX, minY}, z);
        sf::Vector2f sp1 = overlay::toScreen(ctx, {maxX, minY}, z);
        sf::Vector2f sp2 = overlay::toScreen(ctx, {maxX, maxY}, z);
        sf::Vector2f sp3 = overlay::toScreen(ctx, {minX, maxY}, z);

        float screenMaxX = std::max({sp0.x, sp1.x, sp2.x, sp3.x});
        float screenMaxY = std::max({sp0.y, sp1.y, sp2.y, sp3.y});

        sf::Vector2f pos(screenMaxX, screenMaxY);

        if (errorTimer > 0.0f) {
            pos.x += std::sin(ctx.timeSeconds * 40.0f) * 8.0f;
        }

        sf::Text t(textStr, gameFont, fontSize);
        t.setFillColor(sf::Color::White);
        t.setOutlineColor(sf::Color::Black);
        t.setOutlineThickness(1.5f);
        sf::FloatRect b = t.getLocalBounds();

        t.setOrigin(b.left + b.width, b.top + b.height);
        t.setPosition(pos.x - 4.0f, pos.y - 4.0f);
        ctx.window.draw(t);
    };

    for (auto e : taskView) {
        auto& area = registry.get<ecs::TaskArea>(e);
        std::string taskName = "Task " + std::to_string(area.id);

        for (size_t i = 0; i < area.areas.size(); ++i) {
            drawAreaLabel(area.areas[i].startWorld, area.areas[i].endWorld, taskName + " Area " + std::to_string(i + 1), 8);
        }
        if (area.hasDropoff) {
            drawAreaLabel(area.dropoffStart, area.dropoffEnd, taskName + " Drop-off", 8);
        }
    }

    for (auto e : pendingView) {
        auto& area = registry.get<ecs::PendingTaskArea>(e);
        std::string taskName = "New Task";

        for (size_t i = 0; i < area.areas.size(); ++i) {
            drawAreaLabel(area.areas[i].startWorld, area.areas[i].endWorld, taskName + " Area " + std::to_string(i + 1), 8, area.errorTimer);
        }
        if (area.hasDropoff) {
            drawAreaLabel(area.dropoffStart, area.dropoffEnd, taskName + " Drop-off", 8, area.errorTimer);
        }
    }

    for (auto e : buildingView) {
        const auto& wp = buildingView.get<ecs::WorldPos>(e);
        const auto& constr = buildingView.get<ecs::ConstructionData>(e);
        if (constr.isBuilt) continue;

        float hw = constr.resourceZoneWidth / 2.0f;
        float hh = constr.resourceZoneHeight / 2.0f;
        math::Vec2f startW = {wp.wx - hw, wp.wy - hh};
        math::Vec2f endW = {wp.wx + hw, wp.wy + hh};

        drawAreaLabel(startW, endW, "Resource Area", 8);

        int woodCurrent = 0;
        int rockCurrent = 0;
        auto resView = registry.view<ecs::ResourceTag, ecs::WorldPos>();
        for (auto r : resView) {
            auto& rWp = resView.get<ecs::WorldPos>(r);
            if (rWp.wx >= startW.x && rWp.wx <= endW.x && rWp.wy >= startW.y && rWp.wy <= endW.y) {
                if (registry.all_of<ecs::LogTag>(r)) woodCurrent++;
                if (registry.all_of<ecs::SmallRockTag>(r)) rockCurrent++;
            }
        }

        float z = overlay::getElevationInfo(chunks, startW, endW).second;
        sf::Vector2f centerPos = overlay::toScreen(ctx, {wp.wx, wp.wy}, z);
        centerPos.y -= 70.0f;

        int timeLeft = static_cast<int>(std::ceil(constr.maxTime - constr.buildProgress));
        sf::Text tTime("Time Left: " + std::to_string(std::max(0, timeLeft)) + "s", gameFont, 14);
        tTime.setFillColor(sf::Color::White);
        tTime.setOutlineColor(sf::Color::Black);
        tTime.setOutlineThickness(2.0f);
        sf::FloatRect tb = tTime.getLocalBounds();
        tTime.setOrigin(tb.left + tb.width / 2.0f, tb.top + tb.height / 2.0f);
        tTime.setPosition(centerPos.x, centerPos.y);
        ctx.window.draw(tTime);

        float iconY = centerPos.y + 25.0f;
        float startX = centerPos.x - 30.0f;

        auto drawContextIcon = [&](const sf::Texture& tex, const sf::IntRect& texRect, int current, int required, float x) {
            sf::Sprite spr(tex, texRect);
            spr.setOrigin(texRect.width / 2.0f, texRect.height / 2.0f);
            float scale = 32.0f / std::max(texRect.width, texRect.height);
            spr.setScale(scale, scale);
            spr.setPosition(x, iconY);
            ctx.window.draw(spr);

            sf::Text tRatio(std::to_string(current) + "/" + std::to_string(required), gameFont, 10);
            tRatio.setFillColor(current >= required ? sf::Color(100, 255, 100) : sf::Color::White);
            tRatio.setOutlineColor(sf::Color::Black);
            tRatio.setOutlineThickness(1.5f);
            sf::FloatRect rb = tRatio.getLocalBounds();
            tRatio.setOrigin(rb.left + rb.width / 2.0f, rb.top + rb.height / 2.0f);
            tRatio.setPosition(x + 30.0f, iconY);
            ctx.window.draw(tRatio);
        };

        const sf::Texture& atlas = ctx.tiles.getResourcesAtlas();
        drawContextIcon(ctx.tiles.getWoodResourceTexture(), sf::IntRect(0, 0, 64, 64), woodCurrent, constr.woodRequired, startX);
        drawContextIcon(atlas, sf::IntRect(0, 1408, 64, 64), rockCurrent, constr.rockRequired, startX + 70.0f);
    }

    for (auto e : storageView) {
        const auto& wp = storageView.get<ecs::WorldPos>(e);
        const auto& constr = storageView.get<ecs::ConstructionData>(e);

        float hw = constr.resourceZoneWidth / 2.0f;
        float hh = constr.resourceZoneHeight / 2.0f;
        math::Vec2f startW = {wp.wx - hw, wp.wy - hh};
        math::Vec2f endW = {wp.wx + hw, wp.wy + hh};

        drawAreaLabel(startW, endW, "City Storage", 8);

        float z = overlay::getElevationInfo(chunks, startW, endW).second;
        sf::Vector2f centerPos = overlay::toScreen(ctx, {wp.wx, wp.wy}, z);

        sf::Text sText("S", gameFont, 24);
        sText.setFillColor(sf::Color(150, 200, 255, 180));
        sText.setOutlineColor(sf::Color(0, 0, 0, 200));
        sText.setOutlineThickness(2.0f);
        sf::FloatRect b = sText.getLocalBounds();
        sText.setOrigin(b.left + b.width / 2.0f, b.top + b.height / 2.0f);
        sText.setPosition(centerPos);
        ctx.window.draw(sText);
    }
}

}