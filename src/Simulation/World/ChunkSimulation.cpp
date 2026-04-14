#include "../../../include/Simulation/World/ChunkSimulation.hpp"
#include "../../../include/Simulation/Unit/UnitAnimationSystem.hpp"
#include "../../../include/Simulation/Environment/ResourceUpdateSystem.hpp"
#include "../../../include/Simulation/Environment/DebrisSystem.hpp"
#include "../../../include/Simulation/World/TaskSystem.hpp"
#include "ECS/Components.hpp"
#include "Core/Profiler.hpp"

namespace wr::simulation {

    ChunkSimulation::ChunkSimulation(core::ThreadPool& threadPool, entt::registry& registry, world::ChunkManager& chunkManager)
        : m_threadPool(threadPool), m_registry(registry), m_chunkManager(chunkManager) {}

    void ChunkSimulation::update(float dt, rendering::ViewDirection viewDir, const rendering::Camera& camera) {
        const auto& chunks = m_chunkManager.getChunks();
        std::vector<world::Chunk*> activeChunks;
        activeChunks.reserve(chunks.size());

        for (auto& [coord, chunk] : chunks) {
            if (chunk->state.load(std::memory_order_acquire) == world::ChunkState::Active) {
                activeChunks.push_back(chunk.get());
            }
        }

        {
            core::ScopedTimer animTimer("2.1_AnimSystem");
            UnitAnimationSystem::update(m_registry, dt, viewDir, camera);
        }
        {
            core::ScopedTimer resTimer("2.2_ResourceSystem");
            ResourceUpdateSystem::update(m_registry, dt, activeChunks, m_chunkManager);
        }
        {
            core::ScopedTimer debrisTimer("2.3_DebrisSystem");
            DebrisSystem::update(m_registry, dt, camera);
        }
        {
            core::ScopedTimer taskTimer("2.4_TaskSystem");
            TaskSystem::update(m_registry);
        }
    }

}