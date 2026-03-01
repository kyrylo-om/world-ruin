#include "../../../include/Simulation/Environment/ResourceSystem.hpp"
#include "../../../include/Config/ResourceConfig.hpp"
#include "ECS/Components.hpp"
#include "ECS/Tags.hpp"
#include "Rendering/TileHandler.hpp"
#include <cstdlib>
#include <cmath>
#include <algorithm>

namespace wr::world {

static float randFloat(float min, float max) {
    return min + static_cast<float>(rand()) / (static_cast<float>(RAND_MAX / (max - min)));
}

void ResourceSystem::spawnLogs(entt::registry& registry, const rendering::TileHandler& tiles, const math::Vec2f& pos, float zPos, int count, ChunkManager* chunkManager, entt::entity taskEnt, bool exactPosition, uint8_t forcedType) {
    for (int i = 0; i < count; ++i) {
        auto log = registry.create();

        float offsetX = 0.0f;
        float offsetY = 0.0f;
        float vX = 0.0f;
        float vY = 0.0f;
        float startZJump = 0.0f;
        float startZVel = 0.0f;

        if (!exactPosition) {
            float angle = randFloat(0.0f, 3.14159f * 2.0f);
            float dist = randFloat(0.1f, 0.4f);
            offsetX = std::cos(angle) * dist;
            offsetY = std::sin(angle) * dist;

            float speed = randFloat(2.5f, 4.5f);
            vX = std::cos(angle) * speed;
            vY = std::sin(angle) * speed;

            startZJump = randFloat(100.0f, 150.0f);
            startZVel = randFloat(0.0f, 20.0f);
        } else {
            startZJump = 30.0f;
        }

        math::Vec2f logPos = {pos.x + offsetX, pos.y + offsetY};

        registry.emplace<ecs::LogicalPos>(log, static_cast<int64_t>(std::floor(logPos.x)), static_cast<int64_t>(std::floor(logPos.y)));
        registry.emplace<ecs::WorldPos>(log, logPos.x, logPos.y, zPos, zPos);

        auto& wp = registry.get<ecs::WorldPos>(log);
        wp.zJump = startZJump;
        wp.zJumpVel = startZVel;

        registry.emplace<ecs::Velocity>(log, vX, vY, 1.0f, 0.0f);
        registry.emplace<ecs::ScreenPos>(log, 0.0f, 0.0f, 255.0f, 255.0f);

        registry.emplace<ecs::WalkableTag>(log);
        registry.emplace<ecs::ResourceData>(log, forcedType, false, 0.0f);

        registry.emplace<ecs::LogTag>(log);
        registry.emplace<ecs::ResourceTag>(log);
        registry.emplace<ecs::Hitbox>(log, 19.2f);
        registry.emplace<ecs::ClickArea>(log, 32.0f);
        registry.emplace<ecs::Health>(log, 1, 1);

        if (taskEnt != entt::null && registry.valid(taskEnt)) {
            registry.emplace<ecs::MarkedForHarvestTag>(log, sf::Color::White, taskEnt);
        }

        auto& spr = registry.emplace<ecs::SpriteComponent>(log).sprite;
        spr.setTexture(tiles.getWoodResourceTexture());

        float texWidth = 64.0f;
        float texHeight = 64.0f;
        spr.setTextureRect(sf::IntRect(0, 0, static_cast<int>(texWidth), static_cast<int>(texHeight)));
        spr.setOrigin(texWidth / 2.0f, texHeight / 2.0f);

        ecs::CachedQuad quad;
        quad.texture = spr.getTexture();

        float scale = 1.08f;
        float scaledW = texWidth * scale;
        float scaledH = texHeight * scale;
        float ox = scaledW / 2.0f;
        float oy = scaledH / 2.0f;

        quad.localVertices[0] = sf::Vertex({-ox, -oy}, sf::Color::White, {0, 0});
        quad.localVertices[1] = sf::Vertex({scaledW - ox, -oy}, sf::Color::White, {texWidth, 0});
        quad.localVertices[2] = sf::Vertex({scaledW - ox, scaledH - oy}, sf::Color::White, {texWidth, texHeight});
        quad.localVertices[3] = sf::Vertex({-ox, scaledH - oy}, sf::Color::White, {0, texHeight});
        registry.emplace<ecs::CachedQuad>(log, quad);

        if (chunkManager) {
            chunkManager->registerEntity(log, logPos);
        }
    }
}

void ResourceSystem::spawnSmallRocks(entt::registry& registry, const rendering::TileHandler& tiles, const math::Vec2f& pos, float zPos, int count, ChunkManager* chunkManager, entt::entity taskEnt, bool exactPosition, uint8_t forcedType) {
    for (int i = 0; i < count; ++i) {
        auto rock = registry.create();

        float offsetX = 0.0f;
        float offsetY = 0.0f;
        float vX = 0.0f;
        float vY = 0.0f;
        float startZJump = 0.0f;
        float startZVel = 0.0f;

        if (!exactPosition) {
            float angle = randFloat(0.0f, 3.14159f * 2.0f);
            float dist = randFloat(0.05f, 0.15f);
            offsetX = std::cos(angle) * dist;
            offsetY = std::sin(angle) * dist;

            float speed = randFloat(1.0f, 2.0f);
            vX = std::cos(angle) * speed;
            vY = std::sin(angle) * speed;

            startZJump = randFloat(30.0f, 60.0f);
            startZVel = randFloat(10.0f, 20.0f);
        } else {
            startZJump = 20.0f;
        }

        math::Vec2f rockPos = {pos.x + offsetX, pos.y + offsetY};

        registry.emplace<ecs::LogicalPos>(rock, static_cast<int64_t>(std::floor(rockPos.x)), static_cast<int64_t>(std::floor(rockPos.y)));
        registry.emplace<ecs::WorldPos>(rock, rockPos.x, rockPos.y, zPos, zPos);

        auto& wp = registry.get<ecs::WorldPos>(rock);
        wp.zJump = startZJump;
        wp.zJumpVel = startZVel;

        registry.emplace<ecs::Velocity>(rock, vX, vY, 1.0f, 0.0f);
        registry.emplace<ecs::ScreenPos>(rock, 0.0f, 0.0f, 255.0f, 255.0f);

        registry.emplace<ecs::WalkableTag>(rock);

        uint8_t rType = forcedType;
        if (rType == 0) rType = (rand() % 2 == 0) ? 1 : 3;
        registry.emplace<ecs::ResourceData>(rock, rType, false, 0.0f);

        registry.emplace<ecs::SmallRockTag>(rock);
        registry.emplace<ecs::ResourceTag>(rock);
        registry.emplace<ecs::Hitbox>(rock, 19.2f);
        registry.emplace<ecs::ClickArea>(rock, 32.0f);
        registry.emplace<ecs::Health>(rock, 1, 1);

        if (taskEnt != entt::null && registry.valid(taskEnt)) {
            registry.emplace<ecs::MarkedForHarvestTag>(rock, sf::Color::White, taskEnt);
        }

        auto& spr = registry.emplace<ecs::SpriteComponent>(rock).sprite;
        spr.setTexture(tiles.getResourcesAtlas());

        float texWidth = 64.0f;
        float texHeight = 64.0f;
        float tx = 0.0f;
        float ty = 1408.0f + static_cast<float>((rType - 1) * 64);

        spr.setTextureRect(sf::IntRect(0, static_cast<int>(ty), static_cast<int>(texWidth), static_cast<int>(texHeight)));
        spr.setOrigin(texWidth / 2.0f, texHeight / 2.0f);

        ecs::CachedQuad quad;
        quad.texture = spr.getTexture();

        float scale = 1.0f;
        float scaledW = texWidth * scale;
        float scaledH = texHeight * scale;
        float ox = scaledW / 2.0f;
        float oy = scaledH / 2.0f;

        quad.localVertices[0] = sf::Vertex({-ox, -oy}, sf::Color::White, {tx, ty});
        quad.localVertices[1] = sf::Vertex({scaledW - ox, -oy}, sf::Color::White, {tx + texWidth, ty});
        quad.localVertices[2] = sf::Vertex({scaledW - ox, scaledH - oy}, sf::Color::White, {tx + texWidth, ty + texHeight});
        quad.localVertices[3] = sf::Vertex({-ox, scaledH - oy}, sf::Color::White, {tx, ty + texHeight});
        registry.emplace<ecs::CachedQuad>(rock, quad);

        if (chunkManager) {
            chunkManager->registerEntity(rock, rockPos);
        }
    }
}

void ResourceSystem::onResourceHit(entt::registry& registry, entt::entity resource, const rendering::TileHandler& tiles) {
    if (auto* resData = registry.try_get<ecs::ResourceData>(resource)) {
        resData->shakeTimer = 0.15f;
        const auto* wp = registry.try_get<ecs::WorldPos>(resource);

        if (wp) {
            if (registry.all_of<ecs::TreeTag>(resource)) {
                spawnTreeParticles(registry, tiles, {wp->wx, wp->wy}, wp->wz, resData->type, false);
            } else if (registry.all_of<ecs::BushTag>(resource)) {
                spawnBushParticles(registry, tiles, {wp->wx, wp->wy}, wp->wz, false);
            } else if (registry.all_of<ecs::RockTag>(resource) || registry.all_of<ecs::SmallRockTag>(resource)) {
                spawnRockParticles(registry, tiles, {wp->wx, wp->wy}, wp->wz, resData->type, false);
            }
        }
    }
}

void ResourceSystem::onResourceDestroyed(entt::registry& registry, entt::entity resource, const rendering::TileHandler& tiles, ChunkManager* chunkManager, entt::entity taskEnt) {
    const auto* wp = registry.try_get<ecs::WorldPos>(resource);
    if (!wp) return;

    entt::entity forwardTaskEnt = entt::null;
    if (taskEnt != entt::null && registry.valid(taskEnt)) {
        if (auto* taskArea = registry.try_get<ecs::TaskArea>(taskEnt)) {
            if (taskArea->collectFutureDrops) {
                forwardTaskEnt = taskEnt;
            }
        }
    }

    if (registry.all_of<ecs::RockTag>(resource)) {
        int rockType = 2;
        if (auto* resData = registry.try_get<ecs::ResourceData>(resource)) {
            rockType = resData->type;
        }
        spawnRockParticles(registry, tiles, {wp->wx, wp->wy}, wp->wz, rockType, true);

        if (rockType == 2) {
            spawnSmallRocks(registry, tiles, {wp->wx, wp->wy}, wp->wz, config::ROCK_DROP_TYPE_2, chunkManager, forwardTaskEnt, false);
        } else if (rockType == 4) {
            spawnSmallRocks(registry, tiles, {wp->wx, wp->wy}, wp->wz, config::ROCK_DROP_TYPE_4, chunkManager, forwardTaskEnt, false);
        }

    } else if (registry.all_of<ecs::TreeTag>(resource)) {
        if (auto* resData = registry.try_get<ecs::ResourceData>(resource)) {
            spawnTreeParticles(registry, tiles, {wp->wx, wp->wy}, wp->wz, resData->type, true);

            int logCount = 1;
            if (resData->type == 1) logCount = config::TREE_DROP_TYPE_1;
            else if (resData->type == 2) logCount = config::TREE_DROP_TYPE_2;
            else if (resData->type == 3) logCount = config::TREE_DROP_TYPE_3;
            else if (resData->type == 4) logCount = config::TREE_DROP_TYPE_4;

            spawnLogs(registry, tiles, {wp->wx, wp->wy}, wp->wz, logCount, chunkManager, forwardTaskEnt, false);
        }
    } else if (registry.all_of<ecs::BushTag>(resource)) {
        spawnBushParticles(registry, tiles, {wp->wx, wp->wy}, wp->wz, true);
    }
}

void ResourceSystem::spawnRockParticles(entt::registry& registry, const rendering::TileHandler& tiles, const math::Vec2f& pos, float zPos, int rockType, bool isDestroyed) {
    int pieceCount = isDestroyed ? (5 + rand() % 4) : (2 + rand() % 3);

    for (int i = 0; i < pieceCount; ++i) {
        auto debris = registry.create();

        float offsetX = randFloat(-0.1f, 0.1f);
        float offsetY = randFloat(-0.1f, 0.1f);

        registry.emplace<ecs::WorldPos>(debris, pos.x + offsetX, pos.y + offsetY, zPos, 0.0f);
        registry.emplace<ecs::ScreenPos>(debris, 0.0f, 0.0f, 255.0f, 255.0f);

        float angle = randFloat(0.0f, 3.14159f * 2.0f);
        float speed = isDestroyed ? randFloat(1.2f, 3.0f) : randFloat(0.4f, 1.0f);
        registry.emplace<ecs::Velocity>(debris, std::cos(angle) * speed, std::sin(angle) * speed, 1.0f, randFloat(0.0f, 360.0f));

        float lifetime = randFloat(3.0f, 5.0f);
        float scale = isDestroyed ? randFloat(0.2f, 0.45f) : randFloat(0.15f, 0.25f);

        registry.emplace<ecs::DebrisData>(debris, lifetime, lifetime, scale, 1800.0f, randFloat(0.f, 100.f), false);
        registry.emplace<ecs::DebrisTag>(debris);

        auto& dWp = registry.get<ecs::WorldPos>(debris);
        dWp.zJumpVel = isDestroyed ? randFloat(120.0f, 250.0f) : randFloat(60.0f, 120.0f);
        dWp.zJump = isDestroyed ? 10.0f : 5.0f;

        auto& spr = registry.emplace<ecs::SpriteComponent>(debris).sprite;
        spr.setTexture(tiles.getResourcesAtlas());
        spr.setTextureRect(sf::IntRect(0, 1408 + (rockType - 1) * 64, 64, 64));
    }
}

void ResourceSystem::spawnTreeParticles(entt::registry& registry, const rendering::TileHandler& tiles,
                                        const math::Vec2f& pos, float zPos, int treeType, bool isDestroyed) {
    int particleCount = isDestroyed ? (6 + rand() % 4) : (4 + rand() % 3);

    for (int i = 0; i < particleCount; ++i) {
        auto debris = registry.create();

        float offsetX = randFloat(-0.2f, 0.2f);
        float offsetY = randFloat(-0.2f, 0.2f);

        registry.emplace<ecs::WorldPos>(debris, pos.x + offsetX, pos.y + offsetY, zPos, 0.0f);
        registry.emplace<ecs::ScreenPos>(debris, 0.0f, 0.0f, 255.0f, 255.0f);

        bool isCone = (treeType == 1 || treeType == 2) && (randFloat(0.0f, 1.0f) < 0.15f);

        float angle = randFloat(0.0f, 3.14159f * 2.0f);
        float speed = isCone ? randFloat(0.1f, 0.4f) : (isDestroyed ? randFloat(0.2f, 0.8f) : randFloat(0.1f, 0.4f));
        registry.emplace<ecs::Velocity>(debris, std::cos(angle) * speed, std::sin(angle) * speed, 1.0f, randFloat(0.0f, 360.0f));

        float lifetime = isCone ? randFloat(3.0f, 5.0f) : randFloat(2.0f, 3.5f);
        float gravity = isCone ? 1500.0f : 150.0f;
        float scale = isCone ? randFloat(0.15f, 0.3f) : randFloat(0.25f, 0.45f);

        registry.emplace<ecs::DebrisData>(debris, lifetime, lifetime, scale, gravity, randFloat(0.f, 100.f), !isCone);
        registry.emplace<ecs::DebrisTag>(debris);

        auto& dWp = registry.get<ecs::WorldPos>(debris);
        dWp.zJump = randFloat(60.0f, 130.0f);
        dWp.zJumpVel = isCone ? randFloat(0.0f, 30.0f) : randFloat(-30.0f, 10.0f);

        auto& spr = registry.emplace<ecs::SpriteComponent>(debris).sprite;

        if (isCone) {
            spr.setTexture(tiles.getConeTexture());
        } else {
            uint8_t leafIdx = static_cast<uint8_t>(std::clamp(treeType - 1, 0, 3));
            spr.setTexture(tiles.getLeafTexture(leafIdx));
        }
    }
}

void ResourceSystem::spawnBushParticles(entt::registry& registry, const rendering::TileHandler& tiles, const math::Vec2f& pos, float zPos, bool isDestroyed) {
    int particleCount = isDestroyed ? (4 + rand() % 3) : (1 + rand() % 2);

    for (int i = 0; i < particleCount; ++i) {
        auto debris = registry.create();

        float offsetX = randFloat(-0.25f, 0.25f);
        float offsetY = randFloat(-0.25f, 0.25f);

        registry.emplace<ecs::WorldPos>(debris, pos.x + offsetX, pos.y + offsetY, zPos + 10.0f, 0.0f);
        registry.emplace<ecs::ScreenPos>(debris, 0.0f, 0.0f, 255.0f, 255.0f);

        float angle = randFloat(0.0f, 3.14159f * 2.0f);
        float speed = isDestroyed ? randFloat(0.4f, 1.2f) : randFloat(0.2f, 0.6f);
        registry.emplace<ecs::Velocity>(debris, std::cos(angle) * speed, std::sin(angle) * speed, 1.0f, randFloat(0.0f, 360.0f));

        float lifetime = randFloat(1.5f, 2.5f);
        float scale = randFloat(0.3f, 0.5f);

        registry.emplace<ecs::DebrisData>(debris, lifetime, lifetime, scale, 220.0f, randFloat(0.f, 100.f), true);
        registry.emplace<ecs::DebrisTag>(debris);

        auto& dWp = registry.get<ecs::WorldPos>(debris);
        dWp.zJump = randFloat(20.0f, 50.0f);
        dWp.zJumpVel = randFloat(0.0f, 30.0f);

        auto& spr = registry.emplace<ecs::SpriteComponent>(debris).sprite;
        spr.setTexture(tiles.getBushLeafTexture());
    }
}

}