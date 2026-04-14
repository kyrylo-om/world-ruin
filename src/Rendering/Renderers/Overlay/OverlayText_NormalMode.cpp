#include "../../../../include/Rendering/Renderers/Overlay/OverlayText_NormalMode.hpp"
#include "Rendering/TileHandler.hpp"
#include "ECS/Components.hpp"
#include "ECS/Tags.hpp"
#include <SFML/Graphics/Text.hpp>
#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/RenderTarget.hpp>
#include <algorithm>

namespace wr::rendering::overlay {

void OverlayText_NormalMode::render(const RenderContext& ctx,
                                    entt::registry& registry,
                                    const std::unordered_map<math::Vec2i64, std::unique_ptr<world::Chunk>, math::SpatialHash>& chunks,
                                    sf::Font& gameFont,
                                    float rightEdge,
                                    sf::Vector2f windowSize)
{
    auto taskView = registry.view<ecs::TaskArea>();
    auto workerView = registry.view<ecs::WorkerTag, ecs::UnitData, ecs::WorkerState>();
    auto markedView = registry.view<ecs::MarkedForHarvestTag>();

    auto focusedTaskView = registry.view<ecs::FocusedTaskTag, ecs::TaskArea>();
    auto focusedAreaView = registry.view<ecs::FocusedAreaTag, ecs::TaskArea>();

    if (focusedAreaView.begin() != focusedAreaView.end()) {
        auto e = *focusedAreaView.begin();
        auto& fAreaTag = registry.get<ecs::FocusedAreaTag>(e);
        auto& tArea = registry.get<ecs::TaskArea>(e);
        int idx = fAreaTag.areaIndex;

        if (idx >= 0 && idx < static_cast<int>(tArea.areas.size())) {
            const auto& rect = tArea.areas[idx];

            float uiY = 20.0f;
            sf::Text title("TASK " + std::to_string(tArea.id) + " AREA " + std::to_string(idx + 1), gameFont, 18);
            title.setFillColor(sf::Color(255, 215, 0));
            title.setOutlineColor(sf::Color::Black);
            title.setOutlineThickness(2.0f);
            sf::FloatRect tb = title.getLocalBounds();
            title.setPosition(rightEdge - tb.width, uiY);
            ctx.window.draw(title);

            uiY += 85.0f;

            int treeCount = 0, rockCount = 0, smallRockCount = 0, bushCount = 0, logCount = 0;
            float minX = std::min(rect.startWorld.x, rect.endWorld.x);
            float maxX = std::max(rect.startWorld.x, rect.endWorld.x);
            float minY = std::min(rect.startWorld.y, rect.endWorld.y);
            float maxY = std::max(rect.startWorld.y, rect.endWorld.y);

            for (auto res : markedView) {
                if (markedView.get<ecs::MarkedForHarvestTag>(res).taskEntity == e) {
                    auto* wp = registry.try_get<ecs::WorldPos>(res);
                    if (wp && wp->wx >= minX && wp->wx <= maxX && wp->wy >= minY && wp->wy <= maxY) {
                        if (registry.all_of<ecs::TreeTag>(res)) treeCount++;
                        if (registry.all_of<ecs::RockTag>(res)) rockCount++;
                        if (registry.all_of<ecs::SmallRockTag>(res)) smallRockCount++;
                        if (registry.all_of<ecs::BushTag>(res)) bushCount++;
                        if (registry.all_of<ecs::LogTag>(res)) logCount++;
                    }
                }
            }

            int activeResIcons = (treeCount > 0 ? 1 : 0) + (rockCount > 0 ? 1 : 0) + (smallRockCount > 0 ? 1 : 0) + (bushCount > 0 ? 1 : 0) + (logCount > 0 ? 1 : 0);
            if (activeResIcons > 0) {
                float resIconSpacing = 80.0f;
                float totalW = activeResIcons * resIconSpacing;
                float resX = rightEdge - totalW + 30.0f;
                float resY = uiY;

                auto drawResInfoHUD = [&](const sf::Texture& tex, const sf::IntRect& texRect, float targetSize, int count) {
                    sf::Sprite slot(ctx.tiles.getBannerSlotsTexture(), sf::IntRect(0, 0, 64, 64));
                    slot.setOrigin(32.f, 32.f);
                    slot.setPosition(resX, resY);
                    ctx.window.draw(slot);

                    sf::Sprite resSpr(tex, texRect);
                    resSpr.setOrigin(texRect.width / 2.0f, texRect.height / 2.0f);
                    float s = targetSize / std::max((float)texRect.width, (float)texRect.height);
                    resSpr.setScale(s, s);
                    resSpr.setPosition(resX, resY);
                    ctx.window.draw(resSpr);

                    sf::Sprite cBadge(ctx.tiles.getBadgeRedTexture());
                    cBadge.setOrigin(32.f, 32.f);
                    cBadge.setScale(0.45f, 0.45f);
                    cBadge.setPosition(resX + 24.f, resY + 24.f);
                    ctx.window.draw(cBadge);

                    sf::Text cText(std::to_string(count), gameFont, 10);
                    cText.setFillColor(sf::Color::White);
                    cText.setOutlineColor(sf::Color::Black);
                    cText.setOutlineThickness(1.5f);
                    sf::FloatRect cb = cText.getLocalBounds();
                    cText.setOrigin(cb.left + cb.width / 2.0f, cb.top + cb.height / 2.0f);
                    cText.setPosition(resX + 24.f, resY + 24.f);
                    ctx.window.draw(cText);

                    resX += resIconSpacing;
                };

                const sf::Texture& atlas = ctx.tiles.getResourcesAtlas();
                if (treeCount > 0) drawResInfoHUD(atlas, sf::IntRect(0, 0, 192, 192), 40.0f, treeCount);
                if (rockCount > 0) drawResInfoHUD(atlas, sf::IntRect(0, 1408 + 3 * 64, 64, 64), 40.0f, rockCount);
                if (bushCount > 0) drawResInfoHUD(atlas, sf::IntRect(0, 896, 128, 128), 40.0f, bushCount);
                if (logCount > 0) drawResInfoHUD(ctx.tiles.getWoodResourceTexture(), sf::IntRect(0, 0, 64, 64), 40.0f, logCount);
                if (smallRockCount > 0) drawResInfoHUD(atlas, sf::IntRect(0, 1408, 64, 64), 40.0f, smallRockCount);
            }
        }
    }
    else if (focusedTaskView.begin() != focusedTaskView.end()) {
        auto focusedEnt = *focusedTaskView.begin();
        auto& fArea = registry.get<ecs::TaskArea>(focusedEnt);

        float uiY = 20.0f;
        sf::Text title("TASK " + std::to_string(fArea.id) + " DETAILS", gameFont, 18);
        title.setFillColor(sf::Color(255, 215, 0));
        title.setOutlineColor(sf::Color::Black);
        title.setOutlineThickness(2.0f);
        sf::FloatRect tb = title.getLocalBounds();
        title.setPosition(rightEdge - tb.width, uiY);
        ctx.window.draw(title);
        uiY += 45.0f;

        int warriorCount = 0, workerCount = 0, courierCount = 0;
        for (auto w : workerView) {
            auto& uData = workerView.get<ecs::UnitData>(w);
            auto& wState = workerView.get<ecs::WorkerState>(w);
            if (wState.currentTask == focusedEnt) {
                if (uData.type == ecs::UnitType::Warrior) warriorCount++;
                else if (uData.type == ecs::UnitType::Courier) courierCount++;
                else workerCount++;
            }
        }

        int treeCount = 0, rockCount = 0, smallRockCount = 0, bushCount = 0, logCount = 0;
        for (auto res : markedView) {
            if (markedView.get<ecs::MarkedForHarvestTag>(res).taskEntity == focusedEnt) {
                if (registry.all_of<ecs::TreeTag>(res)) treeCount++;
                if (registry.all_of<ecs::RockTag>(res)) rockCount++;
                if (registry.all_of<ecs::SmallRockTag>(res)) smallRockCount++;
                if (registry.all_of<ecs::BushTag>(res)) bushCount++;
                if (registry.all_of<ecs::LogTag>(res)) logCount++;
            }
        }

        float iconSpacing = 110.5f;
        int numIcons = (warriorCount > 0 ? 1 : 0) + (workerCount > 0 ? 1 : 0) + (courierCount > 0 ? 1 : 0);

        if (numIcons > 0) {
            float totalWidth = ((numIcons - 1) * iconSpacing);
            float startX = rightEdge - totalWidth - 15.0f;
            float currentX = startX;

            auto drawIcon = [&](const sf::Texture& tex, int count, float x, float y) {
                sf::Sprite spr(tex);
                sf::FloatRect b = spr.getLocalBounds();
                spr.setOrigin(b.width / 2.0f, b.height / 2.0f);
                spr.setScale(0.35f, 0.35f);
                spr.setPosition(x, y);
                ctx.window.draw(spr);

                sf::Text countText("x" + std::to_string(count), gameFont, 14);
                countText.setFillColor(sf::Color::White);
                countText.setOutlineColor(sf::Color::Black);
                countText.setOutlineThickness(2.0f);
                sf::FloatRect textB = countText.getLocalBounds();
                countText.setOrigin(0.0f, textB.top + textB.height / 2.0f);
                countText.setPosition(x + (b.width * 0.35f) / 2.0f + 4.0f, y);
                ctx.window.draw(countText);
            };

            if (warriorCount > 0) { drawIcon(ctx.tiles.getAvatarWarrior(), warriorCount, currentX, uiY); currentX += iconSpacing; }
            if (workerCount > 0)  { drawIcon(ctx.tiles.getAvatarWorker(), workerCount, currentX, uiY); currentX += iconSpacing; }
            if (courierCount > 0) { drawIcon(ctx.tiles.getAvatarCourier(), courierCount, currentX, uiY); }
        }

        int activeResIcons = (treeCount > 0 ? 1 : 0) + (rockCount > 0 ? 1 : 0) + (smallRockCount > 0 ? 1 : 0) + (bushCount > 0 ? 1 : 0) + (logCount > 0 ? 1 : 0);
        if (activeResIcons > 0) {
            float resIconSpacing = 75.0f;
            float resStartX = rightEdge - ((activeResIcons - 1) * resIconSpacing) - 15.0f;
            float resX = resStartX;
            float resY = uiY + (numIcons > 0 ? 55.0f : 0.0f);

            auto drawResInfoHUD = [&](const sf::Texture& tex, const sf::IntRect& texRect, float targetSize, int count) {
                sf::Sprite slot(ctx.tiles.getBannerSlotsTexture(), sf::IntRect(0, 0, 64, 64));
                slot.setOrigin(32.f, 32.f);
                slot.setPosition(resX, resY);
                ctx.window.draw(slot);

                sf::Sprite resSpr(tex, texRect);
                resSpr.setOrigin(texRect.width / 2.0f, texRect.height / 2.0f);
                float s = targetSize / std::max((float)texRect.width, (float)texRect.height);
                resSpr.setScale(s, s);
                resSpr.setPosition(resX, resY);
                ctx.window.draw(resSpr);

                sf::Sprite cBadge(ctx.tiles.getBadgeRedTexture());
                cBadge.setOrigin(32.f, 32.f);
                cBadge.setScale(0.45f, 0.45f);
                cBadge.setPosition(resX + 24.f, resY + 24.f);
                ctx.window.draw(cBadge);

                sf::Text cText(std::to_string(count), gameFont, 10);
                cText.setFillColor(sf::Color::White);
                cText.setOutlineColor(sf::Color::Black);
                cText.setOutlineThickness(1.5f);
                sf::FloatRect cb = cText.getLocalBounds();
                cText.setOrigin(cb.left + cb.width / 2.0f, cb.top + cb.height / 2.0f);
                cText.setPosition(resX + 24.f, resY + 24.f);
                ctx.window.draw(cText);

                resX += resIconSpacing;
            };

            const sf::Texture& atlas = ctx.tiles.getResourcesAtlas();
            if (treeCount > 0) drawResInfoHUD(atlas, sf::IntRect(0, 0, 192, 192), 40.0f, treeCount);
            if (rockCount > 0) drawResInfoHUD(atlas, sf::IntRect(0, 1408 + 3 * 64, 64, 64), 40.0f, rockCount);
            if (bushCount > 0) drawResInfoHUD(atlas, sf::IntRect(0, 896, 128, 128), 40.0f, bushCount);
            if (logCount > 0) drawResInfoHUD(ctx.tiles.getWoodResourceTexture(), sf::IntRect(0, 0, 64, 64), 40.0f, logCount);
            if (smallRockCount > 0) drawResInfoHUD(atlas, sf::IntRect(0, 1408, 64, 64), 40.0f, smallRockCount);
        }
    }
    else if (taskView.begin() != taskView.end()) {
        float uiY = 20.0f;
        sf::Text title("ACTIVE TASKS", gameFont, 16);
        title.setFillColor(sf::Color::White);
        title.setOutlineColor(sf::Color::Black);
        title.setOutlineThickness(2.0f);
        sf::FloatRect tb = title.getLocalBounds();
        title.setPosition(rightEdge - tb.width, uiY);
        ctx.window.draw(title);
        uiY += 30.0f;

        for (auto e : taskView) {
            auto& tArea = registry.get<ecs::TaskArea>(e);
            sf::Color solidCol(tArea.color.r, tArea.color.g, tArea.color.b, 255);

            sf::RectangleShape colorBox(sf::Vector2f(14.0f, 14.0f));
            colorBox.setFillColor(solidCol);
            colorBox.setOutlineColor(sf::Color::Black);
            colorBox.setOutlineThickness(1.5f);
            colorBox.setPosition(rightEdge - 14.0f, uiY);
            ctx.window.draw(colorBox);

            sf::Text taskText("Task " + std::to_string(tArea.id), gameFont, 14);
            taskText.setFillColor(sf::Color(200, 200, 200));
            taskText.setOutlineColor(sf::Color::Black);
            taskText.setOutlineThickness(1.5f);
            sf::FloatRect ab = taskText.getLocalBounds();
            taskText.setPosition(rightEdge - 25.0f - ab.width, uiY - 1.0f);
            ctx.window.draw(taskText);

            uiY += 25.0f;
        }
    }

    auto selectedView = registry.view<ecs::SelectedTag, ecs::UnitData, ecs::WorkerState>();
    int selCount = std::distance(selectedView.begin(), selectedView.end());

    if (selCount > 0) {
        float bottomY = windowSize.y - 20.0f;
        auto entity = *selectedView.begin();
        auto& uData = selectedView.get<ecs::UnitData>(entity);
        auto& wState = selectedView.get<ecs::WorkerState>(entity);

        auto drawContextHint = [&](const std::string& str, sf::Color color) {
            sf::Text hText(str, gameFont, 14);
            hText.setFillColor(color);
            hText.setOutlineColor(sf::Color::Black);
            hText.setOutlineThickness(2.0f);
            sf::FloatRect hb = hText.getLocalBounds();
            hText.setPosition(rightEdge - hb.width, bottomY - hb.height);
            ctx.window.draw(hText);
            bottomY -= (hb.height + 15.0f);
        };

        bool hasAnyTask = (wState.currentTask != entt::null && registry.valid(wState.currentTask)) || !wState.taskQueue.empty();

        if (hasAnyTask) {
            bool isPaused = registry.all_of<ecs::PausedTag>(entity);
            drawContextHint(isPaused ? "P: Resume Task" : "P: Pause Unit (Regain control)", sf::Color::White);
            drawContextHint("Ctrl + Q: Cancel Unit Task", sf::Color::White);

            if (wState.currentTask != entt::null && registry.valid(wState.currentTask)) {
                if (registry.all_of<ecs::TaskArea>(wState.currentTask)) {
                    auto& tArea = registry.get<ecs::TaskArea>(wState.currentTask);
                    sf::Color solidCol(tArea.color.r, tArea.color.g, tArea.color.b, 255);
                    drawContextHint("Assigned to Task " + std::to_string(tArea.id), solidCol);
                } else if (registry.all_of<ecs::BuildingTag>(wState.currentTask)) {
                    drawContextHint("Building House", sf::Color(100, 200, 255));
                }
            } else if (!wState.taskQueue.empty()) {
                drawContextHint("Queued to Build", sf::Color(100, 200, 255));
            }
        }
    }
}

}