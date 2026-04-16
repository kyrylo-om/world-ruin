#include "../../../../include/Rendering/Renderers/Overlay/OverlayText_TaskMode.hpp"
#include "Rendering/TileHandler.hpp"
#include "ECS/Components.hpp"
#include "ECS/Tags.hpp"
#include <SFML/Graphics/Text.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/RenderTarget.hpp>
#include <algorithm>

namespace wr::rendering::overlay {

void OverlayText_TaskMode::render(const RenderContext& ctx,
                                  entt::registry& registry,
                                  const std::unordered_map<math::Vec2i64, std::unique_ptr<world::Chunk>, math::SpatialHash>& chunks,
                                  sf::Font& gameFont,
                                  float rightEdge,
                                  sf::Vector2f windowSize)
{
    float uiY = 20.0f;

    sf::Text tMode("TASK MODE ACTIVE", gameFont, 18);
    tMode.setFillColor(sf::Color(255, 215, 0));
    tMode.setOutlineColor(sf::Color::Black);
    tMode.setOutlineThickness(2.0f);
    sf::FloatRect b = tMode.getLocalBounds();
    tMode.setPosition(rightEdge - b.width, uiY);
    ctx.window.draw(tMode);
    uiY += 35.0f;

    auto pendingView = registry.view<ecs::PendingTaskArea>();
    if (pendingView.begin() != pendingView.end()) {
        auto pEnt = *pendingView.begin();
        auto& pArea = pendingView.get<ecs::PendingTaskArea>(pEnt);

        int globalLogCount = 0;
        int globalSmallRockCount = 0;
        auto resViewAll = registry.view<ecs::ResourceTag, ecs::WorldPos>();
        for (auto r : resViewAll) {
            auto& wp = resViewAll.get<ecs::WorldPos>(r);
            bool insideAnyArea = false;
            for (const auto& rect : pArea.areas) {
                float minX = std::min(rect.startWorld.x, rect.endWorld.x);
                float maxX = std::max(rect.startWorld.x, rect.endWorld.x);
                float minY = std::min(rect.startWorld.y, rect.endWorld.y);
                float maxY = std::max(rect.startWorld.y, rect.endWorld.y);
                if (wp.wx >= minX && wp.wx <= maxX && wp.wy >= minY && wp.wy <= maxY) {
                    insideAnyArea = true; break;
                }
            }
            if (insideAnyArea) {
                if (registry.all_of<ecs::LogTag>(r)) globalLogCount++;
                if (registry.all_of<ecs::SmallRockTag>(r)) globalSmallRockCount++;
            }
        }

        int treeCount = 0, rockCount = 0, smallRockCount = 0, bushCount = 0, logCount = 0;
        bool canTrees = false, canRocks = false, canSmallRocks = false, canBushes = false, canLogs = false;
        bool incTrees = false, incRocks = false, incSmallRocks = false, incBushes = false, incLogs = false;

        if (pArea.selectedAreaIndex >= 0 && pArea.selectedAreaIndex < static_cast<int>(pArea.areas.size())) {
            const auto& rect = pArea.areas[pArea.selectedAreaIndex];
            canTrees = rect.canHarvestTrees; incTrees = rect.includeTrees;
            canRocks = rect.canHarvestRocks; incRocks = rect.includeRocks;
            canSmallRocks = rect.canHarvestSmallRocks; incSmallRocks = rect.includeSmallRocks;
            canBushes = rect.canHarvestBushes; incBushes = rect.includeBushes;
            canLogs = rect.canHarvestLogs; incLogs = rect.includeLogs;

            float minX = std::min(rect.startWorld.x, rect.endWorld.x);
            float maxX = std::max(rect.startWorld.x, rect.endWorld.x);
            float minY = std::min(rect.startWorld.y, rect.endWorld.y);
            float maxY = std::max(rect.startWorld.y, rect.endWorld.y);

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
        } else {
            canTrees = pArea.canHarvestTrees; incTrees = pArea.includeTrees;
            canRocks = pArea.canHarvestRocks; incRocks = pArea.includeRocks;
            canSmallRocks = pArea.canHarvestSmallRocks; incSmallRocks = pArea.includeSmallRocks;
            canBushes = pArea.canHarvestBushes; incBushes = pArea.includeBushes;
            canLogs = pArea.canHarvestLogs; incLogs = pArea.includeLogs;

            auto resView = registry.view<ecs::ResourceTag, ecs::WorldPos>();
            for (auto r : resView) {
                auto& wp = resView.get<ecs::WorldPos>(r);
                bool insideAnyArea = false;
                for (const auto& rect : pArea.areas) {
                    float minX = std::min(rect.startWorld.x, rect.endWorld.x);
                    float maxX = std::max(rect.startWorld.x, rect.endWorld.x);
                    float minY = std::min(rect.startWorld.y, rect.endWorld.y);
                    float maxY = std::max(rect.startWorld.y, rect.endWorld.y);
                    if (wp.wx >= minX && wp.wx <= maxX && wp.wy >= minY && wp.wy <= maxY) {
                        insideAnyArea = true; break;
                    }
                }
                if (insideAnyArea) {
                    if (registry.all_of<ecs::TreeTag>(r)) treeCount++;
                    if (registry.all_of<ecs::RockTag>(r)) rockCount++;
                    if (registry.all_of<ecs::SmallRockTag>(r)) smallRockCount++;
                    if (registry.all_of<ecs::BushTag>(r)) bushCount++;
                    if (registry.all_of<ecs::LogTag>(r)) logCount++;
                }
            }
        }

        int activeIcons = (canTrees && treeCount > 0 ? 1 : 0) +
                          (canRocks && rockCount > 0 ? 1 : 0) +
                          (canSmallRocks && smallRockCount > 0 ? 1 : 0) +
                          (canBushes && bushCount > 0 ? 1 : 0) +
                          (canLogs && logCount > 0 ? 1 : 0);

        if (activeIcons > 0) {
            float iconSpacing = 80.0f;
            float totalW = activeIcons * iconSpacing;
            float currentX = rightEdge - totalW + 30.0f;
            float slotY = uiY + 25.0f;

            auto drawResInfoHUD = [&](int keyNum, const sf::Texture& tex, const sf::IntRect& texRect, float targetSize, int count, bool included) {
                sf::Sprite slot(ctx.tiles.getBannerSlotsTexture(), sf::IntRect(0, 0, 64, 64));
                slot.setOrigin(32.f, 32.f);
                slot.setPosition(currentX, slotY);
                if (!included) slot.setColor(sf::Color(150, 150, 150, 200));
                ctx.window.draw(slot);

                sf::Sprite resSpr(tex, texRect);
                resSpr.setOrigin(texRect.width / 2.0f, texRect.height / 2.0f);
                float s = targetSize / std::max((float)texRect.width, (float)texRect.height);
                resSpr.setScale(s, s);
                resSpr.setPosition(currentX, slotY);
                if (!included) resSpr.setColor(sf::Color(100, 100, 100, 150));
                ctx.window.draw(resSpr);

                sf::Sprite kBadge(ctx.tiles.getBadgeBlueTexture());
                kBadge.setOrigin(32.f, 32.f);
                kBadge.setScale(0.40f, 0.40f);
                kBadge.setPosition(currentX - 24.f, slotY - 24.f);
                if (!included) kBadge.setColor(sf::Color(150, 150, 150, 200));
                ctx.window.draw(kBadge);

                sf::Text kText(std::to_string(keyNum), gameFont, 10);
                kText.setFillColor(included ? sf::Color::Yellow : sf::Color(200, 200, 200));
                kText.setOutlineColor(sf::Color::Black);
                kText.setOutlineThickness(1.5f);
                sf::FloatRect kb = kText.getLocalBounds();
                kText.setOrigin(kb.left + kb.width / 2.0f, kb.top + kb.height / 2.0f);
                kText.setPosition(currentX - 24.f, slotY - 24.f);
                ctx.window.draw(kText);

                sf::Sprite cBadge(ctx.tiles.getBadgeRedTexture());
                cBadge.setOrigin(32.f, 32.f);
                cBadge.setScale(0.45f, 0.45f);
                cBadge.setPosition(currentX + 24.f, slotY + 24.f);
                if (!included) cBadge.setColor(sf::Color(150, 150, 150, 200));
                ctx.window.draw(cBadge);

                sf::Text cText(std::to_string(count), gameFont, 10);
                cText.setFillColor(included ? sf::Color::White : sf::Color(150, 150, 150));
                cText.setOutlineColor(sf::Color::Black);
                cText.setOutlineThickness(1.5f);
                sf::FloatRect cb = cText.getLocalBounds();
                cText.setOrigin(cb.left + cb.width / 2.0f, cb.top + cb.height / 2.0f);
                cText.setPosition(currentX + 24.f, slotY + 24.f);
                ctx.window.draw(cText);

                currentX += iconSpacing;
            };

            const sf::Texture& atlas = ctx.tiles.getResourcesAtlas();
            if (canTrees && treeCount > 0) drawResInfoHUD(1, atlas, sf::IntRect(0, 0, 192, 192), 40.0f, treeCount, incTrees);
            if (canRocks && rockCount > 0) drawResInfoHUD(2, atlas, sf::IntRect(0, 1408 + 3 * 64, 64, 64), 40.0f, rockCount, incRocks);
            if (canBushes && bushCount > 0) drawResInfoHUD(3, atlas, sf::IntRect(0, 896, 128, 128), 40.0f, bushCount, incBushes);
            if (canLogs && logCount > 0) drawResInfoHUD(4, ctx.tiles.getWoodResourceTexture(), sf::IntRect(0, 0, 64, 64), 40.0f, logCount, incLogs);
            if (canSmallRocks && smallRockCount > 0) drawResInfoHUD(5, atlas, sf::IntRect(0, 1408, 64, 64), 40.0f, smallRockCount, incSmallRocks);
        }

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

        drawHint("Double Click Area: Remove", sf::Color(200, 200, 200));
        drawHint("ENTER: Confirm Task", sf::Color::White);

        if (pArea.collectFutureDrops || globalLogCount > 0 || globalSmallRockCount > 0) {
            sf::Color eCol = (pArea.errorTimer > 0.0f && !pArea.hasDropoff) ? sf::Color(255, 100, 100) : (pArea.hasDropoff ? sf::Color(100, 255, 100) : sf::Color(255, 215, 0));
            drawHint(pArea.hasDropoff ? "E + Drag: Move Drop-off" : "E + Drag: Draw Drop-off", eCol);
        }

        bool hasCityStorage = !registry.view<ecs::CityStorageTag>().empty();
        bool cityStorageInArea = false;
        if (hasCityStorage) {
            auto csView = registry.view<ecs::CityStorageTag, ecs::WorldPos, ecs::ConstructionData>();
            for (auto csEnt : csView) {
                auto& csWp = csView.get<ecs::WorldPos>(csEnt);
                for (const auto& rect : pArea.areas) {
                    float minX = std::min(rect.startWorld.x, rect.endWorld.x);
                    float maxX = std::max(rect.startWorld.x, rect.endWorld.x);
                    float minY = std::min(rect.startWorld.y, rect.endWorld.y);
                    float maxY = std::max(rect.startWorld.y, rect.endWorld.y);
                    if (csWp.wx >= minX && csWp.wx <= maxX && csWp.wy >= minY && csWp.wy <= maxY) {
                        cityStorageInArea = true; break;
                    }
                }
                if (cityStorageInArea) break;
            }

            sf::Color gCol = pArea.hasDropoff ? sf::Color(200, 200, 200) : sf::Color(100, 255, 100);
            drawHint(pArea.hasDropoff ? "City Storage [Disabled]" : "City Storage [Active]", gCol);

            if (cityStorageInArea) {
                drawHint("City Storage in area [Protected]", sf::Color(255, 215, 0));
            }
        }

        sf::Color fCol = pArea.collectFutureDrops ? sf::Color(100, 255, 100) : sf::Color(200, 200, 200);
        drawHint(pArea.collectFutureDrops ? "F: Auto-collect [ON]" : "F: Auto-collect [OFF]", fCol);
    }
}

}