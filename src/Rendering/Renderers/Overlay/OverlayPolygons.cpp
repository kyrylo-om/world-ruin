#include "../../../../include/Rendering/Renderers/Overlay/OverlayPolygons.hpp"
#include "../../../../include/Rendering/Renderers/Overlay/OverlayUtils.hpp"
#include "../../../../include/Config/BuildingsConfig.hpp"
#include "Rendering/TileHandler.hpp"
#include "ECS/Components.hpp"
#include "ECS/Tags.hpp"
#include <SFML/Graphics/VertexArray.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/Text.hpp>
#include <SFML/Graphics/RenderTarget.hpp>
#include <cmath>

namespace wr::rendering::overlay {

void OverlayPolygons::render(const RenderContext& ctx,
                             const std::unordered_map<math::Vec2i64, std::unique_ptr<world::Chunk>, math::SpatialHash>& chunks,
                             const std::vector<entt::entity>& pendingList,
                             const std::vector<entt::entity>& taskList,
                             const ActiveDragData& activeDrag,
                             uint8_t layerMaxLevel,
                             entt::registry& registry,
                             sf::Font& gameFont)
{
    sf::VertexArray fillBatch(sf::Quads);
    sf::VertexArray outlineBatch(sf::Quads);

    auto drawPoly = [&](math::Vec2f startWorld, math::Vec2f endWorld, sf::Color fillColor, sf::Color outlineColor, float z, float shakeX, float thickness = 2.0f) {
        sf::Vector2f p0 = overlay::toScreen(ctx, {startWorld.x, startWorld.y}, z); p0.x += shakeX;
        sf::Vector2f p1 = overlay::toScreen(ctx, {endWorld.x, startWorld.y}, z);   p1.x += shakeX;
        sf::Vector2f p2 = overlay::toScreen(ctx, {endWorld.x, endWorld.y}, z);     p2.x += shakeX;
        sf::Vector2f p3 = overlay::toScreen(ctx, {startWorld.x, endWorld.y}, z);   p3.x += shakeX;

        fillBatch.append(sf::Vertex(p0, fillColor));
        fillBatch.append(sf::Vertex(p1, fillColor));
        fillBatch.append(sf::Vertex(p2, fillColor));
        fillBatch.append(sf::Vertex(p3, fillColor));

        auto addThickLine = [&](sf::Vector2f a, sf::Vector2f b) {
            sf::Vector2f dir = b - a;
            float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
            if (len == 0.0f) return;
            sf::Vector2f normal(-dir.y / len, dir.x / len);
            sf::Vector2f offset = normal * (thickness * 0.5f);
            sf::Vector2f extDir = (dir / len) * (thickness * 0.5f);
            sf::Vector2f pA = a - extDir;
            sf::Vector2f pB = b + extDir;

            outlineBatch.append(sf::Vertex(pA - offset, outlineColor));
            outlineBatch.append(sf::Vertex(pA + offset, outlineColor));
            outlineBatch.append(sf::Vertex(pB + offset, outlineColor));
            outlineBatch.append(sf::Vertex(pB - offset, outlineColor));
        };

        addThickLine(p0, p1);
        addThickLine(p1, p2);
        addThickLine(p2, p3);
        addThickLine(p3, p0);
    };

    auto countResourcesInZone = [&](math::Vec2f zoneStart, math::Vec2f zoneEnd, int& logCount, int& rockCount) {
        float minX = std::min(zoneStart.x, zoneEnd.x);
        float maxX = std::max(zoneStart.x, zoneEnd.x);
        float minY = std::min(zoneStart.y, zoneEnd.y);
        float maxY = std::max(zoneStart.y, zoneEnd.y);
        auto resView = registry.view<ecs::WorldPos, ecs::ResourceTag>();
        for (auto r : resView) {
            auto& wp = resView.get<ecs::WorldPos>(r);
            if (wp.wx >= minX && wp.wx <= maxX && wp.wy >= minY && wp.wy <= maxY) {
                if (registry.all_of<ecs::LogTag>(r)) logCount++;
                if (registry.all_of<ecs::SmallRockTag>(r)) rockCount++;
            }
        }
    };

    struct ZoneCountEntry { math::Vec2f start, end; float z; };
    std::vector<ZoneCountEntry> zonesToCount;

    auto drawZoneResourceCount = [&](math::Vec2f zoneStart, math::Vec2f zoneEnd, float z) {
        int logCount = 0, rockCount = 0;
        countResourcesInZone(zoneStart, zoneEnd, logCount, rockCount);
        if (logCount == 0 && rockCount == 0) return;

        sf::Vector2f bottomLeft = overlay::toScreen(ctx, {std::min(zoneStart.x, zoneEnd.x), std::max(zoneStart.y, zoneEnd.y)}, z);
        float iconSize = 22.0f;
        float cx = bottomLeft.x + 4.0f;
        float cy = bottomLeft.y - 4.0f;

        if (logCount > 0) {
            sf::Text logText(std::to_string(logCount), gameFont, 9);
            logText.setFillColor(sf::Color::White);
            logText.setOutlineColor(sf::Color::Black);
            logText.setOutlineThickness(1.5f);
            logText.setPosition(cx, cy - iconSize + 2.0f);
            ctx.window.draw(logText);
            cx += logText.getLocalBounds().width + 4.0f;

            sf::Sprite logSpr(ctx.tiles.getWoodResourceTexture(), sf::IntRect(0, 0, 64, 64));
            float s = iconSize / 64.0f;
            logSpr.setScale(s, s);
            logSpr.setOrigin(0.0f, 64.0f);
            logSpr.setPosition(cx, cy);
            ctx.window.draw(logSpr);
            cx += iconSize + 8.0f;
        }

        if (rockCount > 0) {
            sf::Text rockText(std::to_string(rockCount), gameFont, 9);
            rockText.setFillColor(sf::Color::White);
            rockText.setOutlineColor(sf::Color::Black);
            rockText.setOutlineThickness(1.5f);
            rockText.setPosition(cx, cy - iconSize + 2.0f);
            ctx.window.draw(rockText);
            cx += rockText.getLocalBounds().width + 4.0f;

            sf::Sprite rockSpr(ctx.tiles.getResourcesAtlas(), sf::IntRect(0, 1408, 64, 64));
            float s = iconSize / 64.0f;
            rockSpr.setScale(s, s);
            rockSpr.setOrigin(0.0f, 64.0f);
            rockSpr.setPosition(cx, cy);
            ctx.window.draw(rockSpr);
        }
    };

    for (auto e : taskList) {
        auto& area = registry.get<ecs::TaskArea>(e);
        if (area.color.a > 0) {
            for (const auto& rect : area.areas) {
                float z = overlay::getElevationInfo(chunks, rect.startWorld, rect.endWorld).second;
                drawPoly(rect.startWorld, rect.endWorld, area.color, sf::Color(area.color.r, area.color.g, area.color.b, 255), z, 0.0f);
            }
        }
        if (area.hasDropoff) {
            float zDrop = overlay::getElevationInfo(chunks, area.dropoffStart, area.dropoffEnd).second;
            drawPoly(area.dropoffStart, area.dropoffEnd, area.color, sf::Color(area.color.r, area.color.g, area.color.b, 255), zDrop, 0.0f);
            zonesToCount.push_back({area.dropoffStart, area.dropoffEnd, zDrop});
        }

        for (auto bEnt : area.targetBuildings) {
            if (registry.valid(bEnt) && registry.all_of<ecs::WorldPos, ecs::ConstructionData>(bEnt)) {
                auto& cData = registry.get<ecs::ConstructionData>(bEnt);
                auto& wp = registry.get<ecs::WorldPos>(bEnt);
                math::Vec2f startW = {wp.wx - cData.resourceZoneWidth/2.f, wp.wy - cData.resourceZoneHeight/2.f};
                math::Vec2f endW = {wp.wx + cData.resourceZoneWidth/2.f, wp.wy + cData.resourceZoneHeight/2.f};

                sf::Color buildingCol = area.color;
                buildingCol.a = 100;
                drawPoly(startW, endW, buildingCol, sf::Color(buildingCol.r, buildingCol.g, buildingCol.b, 255), wp.wz, 0.0f);
            }
        }
    }

    for (auto e : pendingList) {
        auto& area = registry.get<ecs::PendingTaskArea>(e);
        float shakeOffsetX = 0.0f;
        sf::Color pendingColor = sf::Color(255, 255, 255, 80);
        sf::Color pendingOutline = sf::Color(255, 255, 255, 255);

        if (area.errorTimer > 0.0f) {
            shakeOffsetX = std::sin(ctx.timeSeconds * 40.0f) * 8.0f;
            pendingColor = sf::Color(255, 100, 100, 150);
            pendingOutline = sf::Color(255, 100, 100, 255);
        }

        for (const auto& rect : area.areas) {
            float z = overlay::getElevationInfo(chunks, rect.startWorld, rect.endWorld).second;
            drawPoly(rect.startWorld, rect.endWorld, pendingColor, pendingOutline, z, shakeOffsetX);
        }

        if (area.hasDropoff) {
            float zDrop = overlay::getElevationInfo(chunks, area.dropoffStart, area.dropoffEnd).second;
            drawPoly(area.dropoffStart, area.dropoffEnd, pendingColor, pendingOutline, zDrop, shakeOffsetX);
        }

        for (auto bEnt : area.targetBuildings) {
            if (registry.valid(bEnt) && registry.all_of<ecs::WorldPos, ecs::ConstructionData>(bEnt)) {
                auto& cData = registry.get<ecs::ConstructionData>(bEnt);
                auto& wp = registry.get<ecs::WorldPos>(bEnt);
                math::Vec2f startW = {wp.wx - cData.resourceZoneWidth/2.f, wp.wy - cData.resourceZoneHeight/2.f};
                math::Vec2f endW = {wp.wx + cData.resourceZoneWidth/2.f, wp.wy + cData.resourceZoneHeight/2.f};

                sf::Color pBuildColor = pendingColor;
                pBuildColor.a = 100;
                drawPoly(startW, endW, pBuildColor, pendingOutline, wp.wz, 0.0f);
            }
        }
    }

    if (activeDrag.isDragging) {
        if (layerMaxLevel == 8) {
            auto info = overlay::getElevationInfo(chunks, activeDrag.startWorld, activeDrag.endWorld);
            drawPoly(activeDrag.startWorld, activeDrag.endWorld, activeDrag.color, sf::Color(activeDrag.color.r, activeDrag.color.g, activeDrag.color.b, 255), info.second, 0.0f);
        }
    }

    if (registry.ctx().contains<ecs::BuilderModeState>()) {
        auto& bState = registry.ctx().get<ecs::BuilderModeState>();
        if (bState.active && (bState.selectedBuilding == 1 || bState.selectedBuilding == 2)) {
            const auto* bDef = (bState.selectedBuilding == 1) ? &simulation::config::HOUSE_1 : &simulation::config::CITY_STORAGE;
            float hw = bDef->width / 2.0f;
            float hh = bDef->height / 2.0f;
            math::Vec2f startW = {bState.placementPos.x - hw, bState.placementPos.y - hh};
            math::Vec2f endW = {bState.placementPos.x + hw, bState.placementPos.y + hh};

            auto info = overlay::getElevationInfo(chunks, startW, endW);
            bool belongsToThisLayer = (info.first <= layerMaxLevel) && (info.first > layerMaxLevel - 2);
            if (layerMaxLevel == 2 && info.first <= 2) belongsToThisLayer = true;

            if (belongsToThisLayer) {
                sf::Color col = bState.canPlace ? sf::Color(100, 255, 100, 100) : sf::Color(255, 100, 100, 100);
                sf::Color outCol = bState.canPlace ? sf::Color(100, 255, 100, 255) : sf::Color(255, 100, 100, 255);

                if (bState.selectedBuilding == 2) {
                    col = bState.canPlace ? sf::Color(50, 150, 255, 60) : sf::Color(255, 100, 100, 100);
                    outCol = bState.canPlace ? sf::Color(50, 150, 255, 180) : sf::Color(255, 100, 100, 255);
                }

                drawPoly(startW, endW, col, outCol, info.second, 0.0f);

                if (bState.selectedBuilding == 1) {
                    sf::Sprite ghostSpr(ctx.tiles.getHouse1Texture());
                    auto bounds = ghostSpr.getLocalBounds();
                    ghostSpr.setOrigin(bounds.width / 2.0f, bounds.height * 0.65f);
                    ghostSpr.setScale(1.8f, 1.8f);
                    ghostSpr.setColor(bState.canPlace ? sf::Color(100, 255, 100, 180) : sf::Color(255, 100, 100, 180));

                    sf::Vector2f sp = overlay::toScreen(ctx, bState.placementPos, info.second);
                    ghostSpr.setPosition(sp.x, sp.y);
                    ctx.window.draw(ghostSpr);
                }
            }
        }
    }

    auto buildingView = registry.view<ecs::BuildingTag, ecs::WorldPos, ecs::ConstructionData>(entt::exclude<ecs::CityStorageTag>);
    for (auto e : buildingView) {
        const auto& wp = buildingView.get<ecs::WorldPos>(e);
        const auto& constr = buildingView.get<ecs::ConstructionData>(e);
        if (constr.isBuilt) continue;

        float hw = constr.resourceZoneWidth / 2.0f;
        float hh = constr.resourceZoneHeight / 2.0f;
        math::Vec2f startW = {wp.wx - hw, wp.wy - hh};
        math::Vec2f endW = {wp.wx + hw, wp.wy + hh};

        auto info = overlay::getElevationInfo(chunks, startW, endW);
        bool belongsToThisLayer = (info.first <= layerMaxLevel) && (info.first > layerMaxLevel - 2);
        if (layerMaxLevel == 2 && info.first <= 2) belongsToThisLayer = true;

        if (belongsToThisLayer) {
            drawPoly(startW, endW, sf::Color(100, 255, 100, 80), sf::Color(100, 255, 100, 255), wp.wz, 0.0f);
        }
    }

    auto storageView = registry.view<ecs::CityStorageTag, ecs::WorldPos, ecs::ConstructionData>();
    for (auto e : storageView) {
        const auto& wp = storageView.get<ecs::WorldPos>(e);
        const auto& constr = storageView.get<ecs::ConstructionData>(e);

        float hw = constr.resourceZoneWidth / 2.0f;
        float hh = constr.resourceZoneHeight / 2.0f;
        math::Vec2f startW = {wp.wx - hw, wp.wy - hh};
        math::Vec2f endW = {wp.wx + hw, wp.wy + hh};

        auto info = overlay::getElevationInfo(chunks, startW, endW);
        bool belongsToThisLayer = (info.first <= layerMaxLevel) && (info.first > layerMaxLevel - 2);
        if (layerMaxLevel == 2 && info.first <= 2) belongsToThisLayer = true;

        if (belongsToThisLayer) {
            drawPoly(startW, endW, sf::Color(50, 150, 255, 40), sf::Color(50, 150, 255, 120), wp.wz, 0.0f);
            zonesToCount.push_back({startW, endW, wp.wz});
        }
    }

    auto selectedBuilders = registry.view<ecs::SelectedTag, ecs::UnitData, ecs::WorkerState>();
    for(auto sb : selectedBuilders) {
        auto& uData = selectedBuilders.get<ecs::UnitData>(sb);
        auto& wState = selectedBuilders.get<ecs::WorkerState>(sb);
        if(uData.type == ecs::UnitType::Builder) {
            for (size_t i = 0; i < wState.taskQueue.size(); ++i) {
                auto qEnt = wState.taskQueue[i];
                if (registry.valid(qEnt) && registry.all_of<ecs::BuildingTag, ecs::WorldPos, ecs::ConstructionData>(qEnt)) {
                    auto& wp = registry.get<ecs::WorldPos>(qEnt);
                    auto& cData = registry.get<ecs::ConstructionData>(qEnt);

                    if (!cData.isBuilt) {
                        math::Vec2f startW = {wp.wx - cData.resourceZoneWidth/2.f, wp.wy - cData.resourceZoneHeight/2.f};
                        math::Vec2f endW = {wp.wx + cData.resourceZoneWidth/2.f, wp.wy + cData.resourceZoneHeight/2.f};

                        float pulse = (std::sin(ctx.timeSeconds * 6.0f) + 1.0f) * 0.5f;

                        if (qEnt == wState.currentTask) {
                            sf::Color activeFill(50 + pulse*50, 150 + pulse*105, 255, 120 + pulse*40);
                            sf::Color activeOut(100, 200, 255, 255);
                            drawPoly(startW, endW, activeFill, activeOut, wp.wz, 0.0f, 4.0f);
                        } else {
                            sf::Color queueFill(100, 150, 200, 80);
                            sf::Color queueOut(100, 150, 200, 200);
                            drawPoly(startW, endW, queueFill, queueOut, wp.wz, 0.0f, 2.0f);
                        }
                    }
                }
            }
        }
    }

    if (fillBatch.getVertexCount() > 0) ctx.window.draw(fillBatch);
    if (outlineBatch.getVertexCount() > 0) ctx.window.draw(outlineBatch);

    for (auto& zone : zonesToCount) {
        drawZoneResourceCount(zone.start, zone.end, zone.z);
    }
}

}