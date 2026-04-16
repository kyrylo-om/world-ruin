#pragma once
#include "World/ChunkManager.hpp"
#include "World/Chunk.hpp"
#include <entt/entt.hpp>
#include <vector>

namespace wr::simulation {
    class ResourceUpdateSystem {
    public:
        static void update(entt::registry& registry, float dt, const std::vector<world::Chunk*>& activeChunks, world::ChunkManager& chunkManager);
    };
}