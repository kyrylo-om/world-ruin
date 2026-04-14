#pragma once

#include "../../Core/Types.hpp"
#include "../../Core/ThreadPool.hpp"
#include "../../World/ChunkManager.hpp"
#include "../../World/Chunk.hpp"
#include "../../Rendering/Perspective.hpp"
#include "../../Rendering/Camera.hpp"

#include <entt/entt.hpp>
#include <vector>

namespace wr::simulation {

    class ChunkSimulation {
    public:
        ChunkSimulation(core::ThreadPool& threadPool, entt::registry& registry, world::ChunkManager& chunkManager);

        void update(float dt, rendering::ViewDirection viewDir, const rendering::Camera& camera);

    private:
        core::ThreadPool& m_threadPool;
        entt::registry& m_registry;
        world::ChunkManager& m_chunkManager;
    };

}