#include "../../../../include/Rendering/Renderers/Overlay/OverlayText_BuilderMode.hpp"
#include "../../../../include/Config/BuildingsConfig.hpp"
#include "Rendering/TileHandler.hpp"
#include "ECS/Components.hpp"
#include "ECS/Tags.hpp"
#include <SFML/Graphics/Text.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/RenderTarget.hpp>

namespace wr::rendering::overlay {

void OverlayText_BuilderMode::render(const RenderContext& ctx,
                                     entt::registry& registry,
                                     sf::Font& gameFont,
                                     float rightEdge,
                                     sf::Vector2f windowSize)
{
    float uiY = 20.0f;

    sf::Text tMode("BUILDER MODE", gameFont, 18);
    tMode.setFillColor(sf::Color(100, 200, 255));
    tMode.setOutlineColor(sf::Color::Black);
    tMode.setOutlineThickness(2.0f);
    sf::FloatRect b = tMode.getLocalBounds();
    tMode.setPosition(rightEdge - b.width, uiY);
    ctx.window.draw(tMode);
    uiY += 45.0f;

    auto& bState = registry.ctx().get<ecs::BuilderModeState>();

    auto drawBuildingSlot = [&](int keyNum, const simulation::config::BuildingDef& bDef, bool isSelected, float x, float y, sf::Color tintColor, float customScale) {
        sf::Sprite slot(ctx.tiles.getBannerSlotsTexture(), sf::IntRect(0, 0, 64, 64));
        slot.setOrigin(32.f, 32.f);
        slot.setPosition(x, y);
        if (!isSelected) slot.setColor(sf::Color(150, 150, 150, 200));
        ctx.window.draw(slot);

        if (keyNum == 1) {
            sf::Sprite buildingSpr(ctx.tiles.getHouse1Texture());
            sf::FloatRect hb = buildingSpr.getLocalBounds();
            buildingSpr.setOrigin(hb.width / 2.0f, hb.height / 2.0f);
            float scale = customScale / std::max(hb.width, hb.height);
            buildingSpr.setScale(scale, scale);
            buildingSpr.setPosition(x, y);

            sf::Color finalTint = tintColor;
            if (!isSelected) { finalTint.r /= 1.5f; finalTint.g /= 1.5f; finalTint.b /= 1.5f; finalTint.a = 150; }
            buildingSpr.setColor(finalTint);
            ctx.window.draw(buildingSpr);
        } else if (keyNum == 2) {
            sf::Text sText("S", gameFont, 24);
            sText.setFillColor(isSelected ? tintColor : sf::Color(tintColor.r, tintColor.g, tintColor.b, 150));
            sText.setOutlineColor(sf::Color::Black);
            sText.setOutlineThickness(2.0f);
            sf::FloatRect sb = sText.getLocalBounds();
            sText.setOrigin(sb.left + sb.width / 2.0f, sb.top + sb.height / 2.0f);
            sText.setPosition(x, y);
            ctx.window.draw(sText);
        }

        sf::Sprite kBadge(ctx.tiles.getBadgeBlueTexture());
        kBadge.setOrigin(32.f, 32.f);
        kBadge.setScale(0.40f, 0.40f);
        kBadge.setPosition(x - 24.f, y - 24.f);
        if (!isSelected) kBadge.setColor(sf::Color(150, 150, 150, 200));
        ctx.window.draw(kBadge);

        sf::Text kText(std::to_string(keyNum), gameFont, 10);
        kText.setFillColor(isSelected ? sf::Color::Yellow : sf::Color(200, 200, 200));
        kText.setOutlineColor(sf::Color::Black);
        kText.setOutlineThickness(1.5f);
        sf::FloatRect kb = kText.getLocalBounds();
        kText.setOrigin(kb.left + kb.width / 2.0f, kb.top + kb.height / 2.0f);
        kText.setPosition(x - 24.f, y - 24.f);
        ctx.window.draw(kText);

        sf::Sprite wBadge(ctx.tiles.getBadgeRedTexture());
        wBadge.setOrigin(32.f, 32.f);
        wBadge.setScale(0.45f, 0.45f);
        wBadge.setPosition(x - 24.f, y + 24.f);
        ctx.window.draw(wBadge);

        sf::Text wText(std::to_string(bDef.cost.wood), gameFont, 10);
        wText.setFillColor(sf::Color(200, 150, 50));
        wText.setOutlineColor(sf::Color::Black);
        wText.setOutlineThickness(1.5f);
        sf::FloatRect wb = wText.getLocalBounds();
        wText.setOrigin(wb.left + wb.width / 2.0f, wb.top + wb.height / 2.0f);
        wText.setPosition(x - 24.f, y + 24.f);
        ctx.window.draw(wText);

        sf::Sprite rBadge(ctx.tiles.getBadgeRedTexture());
        rBadge.setOrigin(32.f, 32.f);
        rBadge.setScale(0.45f, 0.45f);
        rBadge.setPosition(x + 24.f, y + 24.f);
        ctx.window.draw(rBadge);

        sf::Text rText(std::to_string(bDef.cost.rock), gameFont, 10);
        rText.setFillColor(sf::Color(150, 150, 150));
        rText.setOutlineColor(sf::Color::Black);
        rText.setOutlineThickness(1.5f);
        sf::FloatRect rb = rText.getLocalBounds();
        rText.setOrigin(rb.left + rb.width / 2.0f, rb.top + rb.height / 2.0f);
        rText.setPosition(x + 24.f, y + 24.f);
        ctx.window.draw(rText);
    };

    float slotY = uiY + 15.0f;
    float houseX = rightEdge - 60.0f;
    float storageX = rightEdge - 150.0f;

    drawBuildingSlot(1, simulation::config::HOUSE_1, (bState.selectedBuilding == 1), houseX, slotY, sf::Color::White, 40.0f);
    drawBuildingSlot(2, simulation::config::CITY_STORAGE, (bState.selectedBuilding == 2), storageX, slotY, sf::Color(150, 200, 255), 50.0f);

    float bottomY = windowSize.y - 20.0f;
    auto drawHint = [&](const std::string& str, sf::Color color) {
        sf::Text hText(str, gameFont, 14);
        hText.setFillColor(color);
        hText.setOutlineColor(sf::Color::Black);
        hText.setOutlineThickness(2.0f);
        sf::FloatRect ht = hText.getLocalBounds();
        hText.setPosition(rightEdge - ht.width, bottomY - ht.height);
        ctx.window.draw(hText);
        bottomY -= (ht.height + 15.0f);
    };

    bool hasBuilderSelected = false;
    auto selView = registry.view<ecs::SelectedTag, ecs::UnitData>();
    for(auto e : selView) {
        if(selView.get<ecs::UnitData>(e).type == ecs::UnitType::Builder) hasBuilderSelected = true;
    }

    if (hasBuilderSelected) {
        drawHint("Double Click House: Remove/Cancel", sf::Color(255, 100, 100));
        drawHint("Click Unbuilt House: Add to Builder Queue", sf::Color(100, 200, 255));
    } else {
        drawHint("Double Click Unstarted House OR Empty Storage: Delete", sf::Color(255, 100, 100));
    }

    if (bState.selectedBuilding == 1 || bState.selectedBuilding == 2) {
        drawHint("ENTER: Place Blueprint", bState.canPlace ? sf::Color::White : sf::Color(255, 100, 100));
    }
}

}