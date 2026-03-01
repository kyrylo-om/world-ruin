#pragma once

#include "../../Core/Types.hpp"
#include "../../Core/ThreadPool.hpp"
#include "../../World/ChunkManager.hpp"
#include "../../World/Chunk.hpp"
#include "../../Rendering/Perspective.hpp"

#include <entt/entt.hpp>
#include <vector>
#include <future>

namespace wr::simulation {

    enum class UpdatePhase : uint8_t {
        EvenX_EvenY = 0,
        OddX_OddY   = 1,
        EvenX_OddY  = 2,
        OddX_EvenY  = 3
    };

    class ChunkSimulation {
    public:
        ChunkSimulation(core::ThreadPool& threadPool, entt::registry& registry, world::ChunkManager& chunkManager);

        void update(float dt, rendering::ViewDirection viewDir);

    private:
        [[nodiscard]] UpdatePhase getPhase(const math::Vec2i64& coord) const noexcept;
        void executePhase(UpdatePhase phase, const std::vector<world::Chunk*>& activeChunks, float dt);
        void processChunk(world::Chunk& chunk, float dt) noexcept;

        core::ThreadPool& m_threadPool;
        entt::registry& m_registry;
        world::ChunkManager& m_chunkManager;
        std::vector<std::future<void>> m_phaseFutures;
    };

}