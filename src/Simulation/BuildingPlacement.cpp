#include "Simulation/BuildingPlacement.hpp"
#include "ECS/Tags.hpp"
#include "Rendering/TileHandler.hpp"
#include <cmath>

namespace wr::simulation {

bool isTerrainValid(world::ChunkManager& chunkManager, math::Vec2f pos, const config::BuildingDef& bDef) {
    float hw = bDef.width / 2.0f;
    float hh = bDef.height / 2.0f;

    int minBx = static_cast<int>(std::floor(pos.x - hw + 0.01f));
    int maxBx = static_cast<int>(std::floor(pos.x + hw - 0.01f));
    int minBy = static_cast<int>(std::floor(pos.y - hh + 0.01f));
    int maxBy = static_cast<int>(std::floor(pos.y + hh - 0.01f));

    int baseLevel = -1;
    for (int by = minBy; by <= maxBy; ++by) {
        for (int bx = minBx; bx <= maxBx; ++bx) {
            auto info = chunkManager.getGlobalTileInfo(bx, by);
            if (info.type == core::TerrainType::Water || info.isRamp) return false;
            if (baseLevel == -1) baseLevel = info.elevationLevel;
            else if (baseLevel != info.elevationLevel) return false;
        }
    }
    return true;
}

bool hasObstacleOverlap(entt::registry& registry, math::Vec2f pos, const config::BuildingDef& bDef) {
    float hw = bDef.hitboxWidth / 2.0f;
    float hh = bDef.hitboxHeight / 2.0f;
    float minX = pos.x - hw, maxX = pos.x + hw;
    float minY = pos.y - hh, maxY = pos.y + hh;

    auto resView = registry.view<ecs::WorldPos>();
    for (auto e : resView) {
        if (!registry.any_of<ecs::TreeTag, ecs::RockTag, ecs::SmallRockTag, ecs::BushTag, ecs::BuildingTag>(e)) continue;

        auto& wp = resView.get<ecs::WorldPos>(e);
        float r = 0.4f;
        if (auto* hb = registry.try_get<ecs::Hitbox>(e)) r = hb->radius / 64.0f;
        else if (auto* ca = registry.try_get<ecs::ClickArea>(e)) r = ca->radius / 64.0f;

        if (wp.wx + r > minX && wp.wx - r < maxX && wp.wy + r > minY && wp.wy - r < maxY) {
            return true;
        }
    }
    return false;
}

bool hasResourceZoneOverlap(entt::registry& registry, math::Vec2f pos, const config::BuildingDef& bDef, bool isStorage) {
    float rw = bDef.resourceZoneWidth / 2.0f;
    float rh = bDef.resourceZoneHeight / 2.0f;
    float rMinX = pos.x - rw, rMaxX = pos.x + rw;
    float rMinY = pos.y - rh, rMaxY = pos.y + rh;

    if (isStorage) {
        auto houseView = registry.view<ecs::BuildingTag, ecs::WorldPos>(entt::exclude<ecs::CityStorageTag>);
        for (auto e : houseView) {
            auto& hWp = houseView.get<ecs::WorldPos>(e);
            float hRw = config::HOUSE_1.resourceZoneWidth / 2.0f;
            float hRh = config::HOUSE_1.resourceZoneHeight / 2.0f;
            if (!(rMaxX < hWp.wx - hRw || rMinX > hWp.wx + hRw ||
                  rMaxY < hWp.wy - hRh || rMinY > hWp.wy + hRh)) {
                return true;
            }
        }
    } else {
        auto storageView = registry.view<ecs::CityStorageTag, ecs::WorldPos>();
        for (auto s : storageView) {
            auto& sWp = storageView.get<ecs::WorldPos>(s);
            float sRw = config::CITY_STORAGE.resourceZoneWidth / 2.0f;
            float sRh = config::CITY_STORAGE.resourceZoneHeight / 2.0f;
            if (!(rMaxX < sWp.wx - sRw || rMinX > sWp.wx + sRw ||
                  rMaxY < sWp.wy - sRh || rMinY > sWp.wy + sRh)) {
                return true;
            }
        }
    }
    return false;
}

bool canPlaceBuilding(entt::registry& registry, world::ChunkManager& chunkManager, math::Vec2f pos, const config::BuildingDef& bDef, bool isStorage) {
    if (!isTerrainValid(chunkManager, pos, bDef)) return false;
    if (hasObstacleOverlap(registry, pos, bDef)) return false;
    if (hasResourceZoneOverlap(registry, pos, bDef, isStorage)) return false;
    return true;
}

entt::entity spawnBuilding(entt::registry& registry, world::ChunkManager& chunkManager,
                           const config::BuildingDef& bDef, math::Vec2f pos, bool isStorage) {
    int bX = static_cast<int>(std::floor(pos.x));
    int bY = static_cast<int>(std::floor(pos.y));
    uint8_t level = chunkManager.getGlobalTileInfo(bX, bY).elevationLevel;
    float zHeight = (level - 1) * 32.0f;

    auto bEnt = registry.create();
    registry.emplace<ecs::BuildingTag>(bEnt);

    registry.emplace<ecs::LogicalPos>(bEnt, static_cast<int64_t>(bX), static_cast<int64_t>(bY));
    registry.emplace<ecs::WorldPos>(bEnt, pos.x, pos.y, zHeight, zHeight);
    registry.emplace<ecs::ScreenPos>(bEnt, 0.f, 0.f, 255.0f, 255.0f, static_cast<uint8_t>(0));
    registry.emplace<ecs::Hitbox>(bEnt, (bDef.hitboxWidth * 64.0f) / 2.0f);
    registry.emplace<ecs::ClickArea>(bEnt, (bDef.hitboxWidth * 64.0f) / 2.0f);

    bool instant = (bDef.buildTime <= 0.0f);
    registry.emplace<ecs::ConstructionData>(bEnt,
        bDef.cost.wood, bDef.cost.rock,
        bDef.cost.wood, bDef.cost.rock,
        bDef.resourceZoneWidth, bDef.resourceZoneHeight,
        instant, instant ? bDef.buildTime : 0.0f, bDef.buildTime
    );

    if (isStorage) {
        registry.emplace<ecs::CityStorageTag>(bEnt);
    } else {
        auto& spr = registry.emplace<ecs::SpriteComponent>(bEnt).sprite;
        auto* tilesPtr = registry.ctx().find<const rendering::TileHandler*>();
        if (tilesPtr) {
            spr.setTexture((*tilesPtr)->getHouse1Texture());
            auto bounds = spr.getLocalBounds();
            spr.setOrigin(bounds.width / 2.0f, bounds.height * 0.65f);
            spr.setScale(1.8f, 1.8f);
        }
        registry.emplace<ecs::SolidTag>(bEnt);
    }

    return bEnt;
}

}
