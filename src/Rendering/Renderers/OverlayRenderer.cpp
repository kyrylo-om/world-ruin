#include "Rendering/Renderers/OverlayRenderer.hpp"
#include "ECS/Tags.hpp"
#include <SFML/Graphics/ConvexShape.hpp>
#include <SFML/Graphics/Text.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <cmath>
#include <string>
#include <algorithm>

namespace wr::rendering {

std::pair<uint8_t, float> OverlayRenderer::getElevationInfo(const std::unordered_map<math::Vec2i64, std::unique_ptr<world::Chunk>, math::SpatialHash>& chunks, math::Vec2f start, math::Vec2f end) {
    float mx = (start.x + end.x) / 2.0f;
    float my = (start.y + end.y) / 2.0f;

    int64_t cx = static_cast<int64_t>(std::floor(mx / 64.0f));
    int64_t cy = static_cast<int64_t>(std::floor(my / 64.0f));

    uint8_t lvl = 1;
    auto it = chunks.find({cx, cy});
    if (it != chunks.end() && it->second->state.load(std::memory_order_acquire) == world::ChunkState::Active) {
        int64_t lx = static_cast<int64_t>(std::floor(mx)) - cx * 64;
        int64_t ly = static_cast<int64_t>(std::floor(my)) - cy * 64;
        if (lx >= 0 && lx < 64 && ly >= 0 && ly < 64) {
            lvl = it->second->getLevel(lx, ly);
        }
    }

    float z = 0.0f;
    if (lvl == 2) z = 32.0f;
    else if (lvl == 3) z = 64.0f;
    else if (lvl == 4) z = 96.0f;
    else if (lvl == 5) z = 128.0f;
    else if (lvl == 6) z = 160.0f;
    else if (lvl == 7) z = 192.0f;

    return {lvl, z};
}

sf::Vector2f OverlayRenderer::toScreen(const RenderContext& ctx, math::Vec2f w, float zHeight) {
    float dx = (w.x - static_cast<float>(ctx.camera.getAbsolutePosition().x)) * 64.0f;
    float dy = (w.y - static_cast<float>(ctx.camera.getAbsolutePosition().y)) * 64.0f;
    math::Vec2f rel = { dx - ctx.camera.getSubPixelX(), dy - ctx.camera.getSubPixelY() };

    float rad = ctx.currentAngle * 3.14159265f / 180.0f;
    float rotX = rel.x * std::cos(rad) - rel.y * std::sin(rad);
    float rotY = rel.x * std::sin(rad) + rel.y * std::cos(rad);

    return sf::Vector2f(rotX, rotY - zHeight);
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
    auto drawPoly = [&](math::Vec2f startWorld, math::Vec2f endWorld, sf::Color color, float z) {
        sf::ConvexShape poly(4);
        poly.setPoint(0, toScreen(ctx, {startWorld.x, startWorld.y}, z));
        poly.setPoint(1, toScreen(ctx, {endWorld.x, startWorld.y}, z));
        poly.setPoint(2, toScreen(ctx, {endWorld.x, endWorld.y}, z));
        poly.setPoint(3, toScreen(ctx, {startWorld.x, endWorld.y}, z));

        poly.setFillColor(color);
        poly.setOutlineColor(sf::Color(color.r, color.g, color.b, 255));
        poly.setOutlineThickness(2.0f);
        ctx.window.draw(poly);
    };

    for (auto e : taskList) {
        auto& area = registry.get<ecs::TaskArea>(e);
        if (area.color.a > 0) {
            float z = getElevationInfo(chunks, area.startWorld, area.endWorld).second;
            drawPoly(area.startWorld, area.endWorld, area.color, z);
        }
        if (area.hasDropoff) {
            float zDrop = getElevationInfo(chunks, area.dropoffStart, area.dropoffEnd).second;
            drawPoly(area.dropoffStart, area.dropoffEnd, sf::Color(255, 255, 255, 60), zDrop);
        }
    }

    for (auto e : pendingList) {
        auto& area = registry.get<ecs::PendingTaskArea>(e);
        float z = getElevationInfo(chunks, area.startWorld, area.endWorld).second;
        drawPoly(area.startWorld, area.endWorld, sf::Color(255, 255, 255, 80), z);

        if (area.hasDropoff) {
            float zDrop = getElevationInfo(chunks, area.dropoffStart, area.dropoffEnd).second;
            drawPoly(area.dropoffStart, area.dropoffEnd, sf::Color(255, 255, 255, 60), zDrop);
        }
    }

    if (activeDrag.isDragging) {
        auto info = getElevationInfo(chunks, activeDrag.startWorld, activeDrag.endWorld);
        bool belongsToThisLayer = (info.first <= layerMaxLevel) && (info.first > layerMaxLevel - 2);

        if (layerMaxLevel == 2 && info.first <= 2) belongsToThisLayer = true;

        if (belongsToThisLayer) {
            drawPoly(activeDrag.startWorld, activeDrag.endWorld, activeDrag.color, info.second);
        }
    }
}

void OverlayRenderer::renderText(const RenderContext& ctx,
                                 entt::registry& registry,
                                 const std::unordered_map<math::Vec2i64, std::unique_ptr<world::Chunk>, math::SpatialHash>& chunks,
                                 sf::Font& gameFont)
{
    auto taskView = registry.view<ecs::TaskArea>();
    auto workerView = registry.view<ecs::WorkerTag, ecs::UnitData>();

    for (auto e : taskView) {
        auto& area = registry.get<ecs::TaskArea>(e);

        if (area.hasDropoff) {
            float zDrop = getElevationInfo(chunks, area.dropoffStart, area.dropoffEnd).second;
            sf::Vector2f center = toScreen(ctx, {(area.dropoffStart.x + area.dropoffEnd.x)/2.f, (area.dropoffStart.y + area.dropoffEnd.y)/2.f}, zDrop);
            sf::Text t;
            t.setFont(gameFont);
            t.setString("Drop-off");
            t.setCharacterSize(14);
            t.setFillColor(sf::Color::White);
            t.setOutlineColor(sf::Color::Black);
            t.setOutlineThickness(1.5f);
            sf::FloatRect b = t.getLocalBounds();
            t.setOrigin(b.left + b.width/2.f, b.top + b.height/2.f);
            t.setPosition(center);
            ctx.window.draw(t);
        }

        if (area.color.a > 0) {
            float minX = std::min(area.startWorld.x, area.endWorld.x);
            float maxX = std::max(area.startWorld.x, area.endWorld.x);
            float minY = std::min(area.startWorld.y, area.endWorld.y);
            float maxY = std::max(area.startWorld.y, area.endWorld.y);

            float midX = (minX + maxX) / 2.0f;
            float midY = (minY + maxY) / 2.0f;
            float z = getElevationInfo(chunks, area.startWorld, area.endWorld).second;

            int warriorCount = 0;
            int workerCount = 0;
            int courierCount = 0;

            for (auto w : workerView) {
                auto& uData = workerView.get<ecs::UnitData>(w);
                if (uData.currentTask == e) {
                    if (uData.type == ecs::UnitType::Warrior) warriorCount++;
                    else if (uData.type == ecs::UnitType::Courier) courierCount++;
                    else workerCount++;
                }
            }

            sf::Vector2f centerPos = toScreen(ctx, {midX, midY}, z);
            centerPos.y -= 12.0f;

            sf::Text idText;
            idText.setFont(gameFont);
            idText.setString("Task " + std::to_string(area.id));
            idText.setCharacterSize(28);
            idText.setFillColor(sf::Color::White);
            idText.setOutlineColor(sf::Color::Black);
            idText.setOutlineThickness(2.5f);

            sf::FloatRect textBounds = idText.getLocalBounds();
            idText.setOrigin(textBounds.left + textBounds.width / 2.0f, textBounds.top + textBounds.height / 2.0f);
            idText.setPosition(centerPos);
            ctx.window.draw(idText);

            float iconSpacing = 110.5f;
            int numIcons = (warriorCount > 0 ? 1 : 0) + (workerCount > 0 ? 1 : 0) + (courierCount > 0 ? 1 : 0);

            if (numIcons > 0) {
                float totalWidth = ((numIcons - 1) * iconSpacing);
                float startX = centerPos.x - (totalWidth / 2.0f);
                float currentX = startX;
                float iconY = centerPos.y + 60.0f;

                auto drawIcon = [&](const sf::Texture& tex, int count, float x, float y) {
                    sf::Sprite spr(tex);
                    sf::FloatRect b = spr.getLocalBounds();
                    spr.setOrigin(b.width / 2.0f, b.height / 2.0f);
                    spr.setScale(0.35f, 0.35f);
                    spr.setPosition(x, y);
                    ctx.window.draw(spr);

                    sf::Text countText;
                    countText.setFont(gameFont);
                    countText.setString("x" + std::to_string(count));
                    countText.setCharacterSize(14);
                    countText.setFillColor(sf::Color::White);
                    countText.setOutlineColor(sf::Color::Black);
                    countText.setOutlineThickness(2.0f);

                    sf::FloatRect tb = countText.getLocalBounds();
                    countText.setOrigin(0.0f, tb.top + tb.height / 2.0f);
                    countText.setPosition(x + (b.width * 0.35f) / 2.0f + 4.0f, y);
                    ctx.window.draw(countText);
                };

                if (warriorCount > 0) { drawIcon(ctx.tiles.getAvatarWarrior(), warriorCount, currentX, iconY); currentX += iconSpacing; }
                if (workerCount > 0)  { drawIcon(ctx.tiles.getAvatarWorker(), workerCount, currentX, iconY); currentX += iconSpacing; }
                if (courierCount > 0) { drawIcon(ctx.tiles.getAvatarCourier(), courierCount, currentX, iconY); }
            }
        }
    }

    auto pendingView = registry.view<ecs::PendingTaskArea>();
    for (auto e : pendingView) {
        auto& area = registry.get<ecs::PendingTaskArea>(e);

        if (area.hasDropoff) {
            float zDrop = getElevationInfo(chunks, area.dropoffStart, area.dropoffEnd).second;
            sf::Vector2f center = toScreen(ctx, {(area.dropoffStart.x + area.dropoffEnd.x)/2.f, (area.dropoffStart.y + area.dropoffEnd.y)/2.f}, zDrop);
            sf::Text t;
            t.setFont(gameFont);
            t.setString("Drop-off");
            t.setCharacterSize(14);
            t.setFillColor(sf::Color::White);
            t.setOutlineColor(sf::Color::Black);
            t.setOutlineThickness(1.5f);
            sf::FloatRect b = t.getLocalBounds();
            t.setOrigin(b.left + b.width/2.f, b.top + b.height/2.f);
            t.setPosition(center);
            ctx.window.draw(t);
        }

        float minX = std::min(area.startWorld.x, area.endWorld.x);
        float maxX = std::max(area.startWorld.x, area.endWorld.x);
        float minY = std::min(area.startWorld.y, area.endWorld.y);
        float maxY = std::max(area.startWorld.y, area.endWorld.y);

        float midX = (minX + maxX) / 2.0f;
        float midY = (minY + maxY) / 2.0f;
        float z = getElevationInfo(chunks, area.startWorld, area.endWorld).second;

        int treeCount = 0, rockCount = 0, smallRockCount = 0, bushCount = 0, logCount = 0;
        auto resView = registry.view<ecs::ResourceTag, ecs::WorldPos>();
        for (auto r : resView) {
            auto& wp = resView.get<ecs::WorldPos>(r);
            if (wp.wx >= minX && wp.wx <= maxX && wp.wy >= minY && wp.wy <= maxY) {
                if (registry.all_of<ecs::TreeTag>(r)) treeCount++;
                if (registry.all_of<ecs::RockTag>(r)) rockCount++;
                if (registry.all_of<ecs::SmallRockTag>(r)) smallRockCount++;
                if (registry.all_of<ecs::BushTag>(r)) bushCount++;
                if (registry.all_of<ecs::LogTag>(r)) logCount++;
            }
        }

        sf::Vector2f centerPos = toScreen(ctx, {midX, midY}, z);

        int activeIcons = (area.canHarvestTrees && treeCount > 0 ? 1 : 0) +
                          (area.canHarvestRocks && rockCount > 0 ? 1 : 0) +
                          (area.canHarvestSmallRocks && smallRockCount > 0 ? 1 : 0) +
                          (area.canHarvestBushes && bushCount > 0 ? 1 : 0) +
                          (area.canHarvestLogs && logCount > 0 ? 1 : 0);

        if (activeIcons > 0) {
            float iconSpacing = 160.0f;
            float startX = centerPos.x - ((activeIcons - 1) * iconSpacing) / 2.0f;
            float currentX = startX;
            float iconY = centerPos.y;

            auto drawResInfo = [&](int keyNum, const sf::Texture& tex, const sf::IntRect& texRect, float scale, int count, bool included) {
                sf::Text kText;
                kText.setFont(gameFont);
                kText.setString(std::to_string(keyNum) + ": ");
                kText.setCharacterSize(16);
                kText.setFillColor(included ? sf::Color::Yellow : sf::Color(150, 150, 150));
                kText.setOutlineColor(sf::Color::Black);
                kText.setOutlineThickness(2.0f);
                sf::FloatRect kb = kText.getLocalBounds();
                kText.setOrigin(0.0f, kb.top + kb.height / 2.0f);
                kText.setPosition(currentX - 55.0f, iconY);
                ctx.window.draw(kText);

                sf::Sprite spr(tex, texRect);
                spr.setOrigin(texRect.width / 2.0f, texRect.height / 2.0f);
                spr.setScale(scale, scale);
                spr.setPosition(currentX - 10.0f, iconY);
                if (!included) spr.setColor(sf::Color(100, 100, 100, 150));
                ctx.window.draw(spr);

                sf::Text cText;
                cText.setFont(gameFont);
                cText.setString("x" + std::to_string(count));
                cText.setCharacterSize(16);
                cText.setFillColor(included ? sf::Color::White : sf::Color(150, 150, 150));
                cText.setOutlineColor(sf::Color::Black);
                cText.setOutlineThickness(2.0f);
                sf::FloatRect cb = cText.getLocalBounds();
                cText.setOrigin(0.0f, cb.top + cb.height / 2.0f);
                cText.setPosition(currentX + 30.0f, iconY);
                ctx.window.draw(cText);

                currentX += iconSpacing;
            };

            const sf::Texture& atlas = ctx.tiles.getResourcesAtlas();

            if (area.canHarvestTrees && treeCount > 0) drawResInfo(1, atlas, sf::IntRect(0, 0, 192, 192), 0.35f, treeCount, area.includeTrees);
            if (area.canHarvestRocks && rockCount > 0) drawResInfo(2, atlas, sf::IntRect(0, 1408 + 3 * 64, 64, 64), 1.2f, rockCount, area.includeRocks);
            if (area.canHarvestBushes && bushCount > 0) drawResInfo(3, atlas, sf::IntRect(0, 896, 128, 128), 0.70f, bushCount, area.includeBushes);
            if (area.canHarvestLogs && logCount > 0) drawResInfo(4, ctx.tiles.getWoodResourceTexture(), sf::IntRect(0, 0, 64, 64), 1.0f, logCount, area.includeLogs);
            if (area.canHarvestSmallRocks && smallRockCount > 0) drawResInfo(5, atlas, sf::IntRect(0, 1408, 64, 64), 1.0f, smallRockCount, area.includeSmallRocks);
        }

        bool hasCourier = false;
        for (auto se : registry.view<ecs::SelectedTag, ecs::UnitData>()) {
            if (registry.get<ecs::UnitData>(se).type == ecs::UnitType::Courier) {
                hasCourier = true; break;
            }
        }

        if (area.includeLogs || area.includeSmallRocks || hasCourier) {
            sf::Vector2f p0 = toScreen(ctx, {minX, minY}, z);
            sf::Vector2f p1 = toScreen(ctx, {maxX, minY}, z);
            sf::Vector2f p2 = toScreen(ctx, {maxX, maxY}, z);
            sf::Vector2f p3 = toScreen(ctx, {minX, maxY}, z);

            float bottomY = std::max({p0.y, p1.y, p2.y, p3.y});

            sf::Text hintText;
            hintText.setFont(gameFont);
            hintText.setString(area.isSelectingDropoff ? "Q: Specify Drop-off" : "Q: Specify Drop-off");
            hintText.setCharacterSize(10);

            if (area.isSelectingDropoff) hintText.setFillColor(sf::Color(100, 255, 100));
            else hintText.setFillColor(sf::Color(255, 215, 0));

            hintText.setOutlineColor(sf::Color::Black);
            hintText.setOutlineThickness(1.5f);

            sf::FloatRect hb = hintText.getLocalBounds();
            hintText.setOrigin(hb.left + hb.width / 2.0f, hb.top + hb.height / 2.0f);

            hintText.setPosition(centerPos.x, bottomY + hb.height + 15.0f);
            ctx.window.draw(hintText);

            if (hasCourier) {
                sf::Text fHintText;
                fHintText.setFont(gameFont);
                fHintText.setString(area.collectFutureDrops ? "F: Auto-collect [ON]" : "F: Auto-collect [OFF]");
                fHintText.setCharacterSize(10);
                fHintText.setFillColor(area.collectFutureDrops ? sf::Color(100, 255, 100) : sf::Color(150, 150, 150));
                fHintText.setOutlineColor(sf::Color::Black);
                fHintText.setOutlineThickness(1.5f);
                sf::FloatRect fhb = fHintText.getLocalBounds();
                fHintText.setOrigin(fhb.left + fhb.width / 2.0f, fhb.top + fhb.height / 2.0f);
                fHintText.setPosition(centerPos.x, bottomY + hb.height + 15.0f + fhb.height + 12.0f);
                ctx.window.draw(fHintText);
            }
        }
    }
}

}