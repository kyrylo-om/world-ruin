#pragma once

#include "Core/Math.hpp"
#include "Rendering/TileHandler.hpp"
#include "World/ChunkManager.hpp"
#include <entt/entt.hpp>

namespace wr::world {

    class ResourceSystem {
    public:
        static void onResourceHit(entt::registry& registry, entt::entity resource, const rendering::TileHandler& tiles);
        static void onResourceDestroyed(entt::registry& registry, entt::entity resource, const rendering::TileHandler& tiles, ChunkManager* chunkManager, entt::entity taskEnt = entt::null);

        static void spawnLogs(entt::registry& registry, const rendering::TileHandler& tiles, const math::Vec2f& pos, float zPos, int count, ChunkManager* chunkManager, entt::entity taskEnt = entt::null, bool exactPosition = false, uint8_t forcedType = 1);
        static void spawnSmallRocks(entt::registry& registry, const rendering::TileHandler& tiles, const math::Vec2f& pos, float zPos, int count, ChunkManager* chunkManager, entt::entity taskEnt = entt::null, bool exactPosition = false, uint8_t forcedType = 0);

    private:
        static void spawnTreeParticles(entt::registry& registry, const rendering::TileHandler& tiles, const math::Vec2f& pos, float zPos, int treeType, bool isDestroyed);
        static void spawnBushParticles(entt::registry& registry, const rendering::TileHandler& tiles, const math::Vec2f& pos, float zPos, bool isDestroyed);
        static void spawnRockParticles(entt::registry& registry, const rendering::TileHandler& tiles, const math::Vec2f& pos, float zPos, int rockType, bool isDestroyed);
    };

}