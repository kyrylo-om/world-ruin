#pragma once

#include "Core/Types.hpp"
#include "Core/ThreadPool.hpp"
#include "World/Chunk.hpp"
#include "Generation/WorldGenerator.hpp"
#include "Rendering/TileHandler.hpp"
#include "Rendering/Perspective.hpp"
#include <entt/entt.hpp>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <vector>

namespace wr::world {

    class ChunkManager {
    public:
        ChunkManager(core::ThreadPool& threadPool,
                     entt::registry& registry,
                     const WorldGenerator& generator,
                     const rendering::TileHandler& tileHandler);

        void update(const math::Vec2f& cameraCenterWorldPx, rendering::ViewDirection viewDir);

        [[nodiscard]] const std::unordered_map<math::Vec2i64, std::unique_ptr<Chunk>, math::SpatialHash>& getChunks() const noexcept {
            return m_chunks;
        }

        [[nodiscard]] TileInfo getGlobalTileInfo(core::GlobalCoord x, core::GlobalCoord y);
        [[nodiscard]] core::TerrainType getGlobalBlockType(core::GlobalCoord x, core::GlobalCoord y);

        void registerEntity(entt::entity entity, const math::Vec2f& worldPos);

    private:
        void enqueueChunkGeneration(math::Vec2i64 chunkCoord, rendering::ViewDirection viewDir);
        void unloadChunk(math::Vec2i64 chunkCoord);
        void finalizeReadyChunk(const math::Vec2i64& coord, Chunk& chunk);

        core::ThreadPool& m_threadPool;
        entt::registry& m_registry;
        const WorldGenerator& m_generator;
        const rendering::TileHandler& m_tileHandler;

        std::unordered_map<math::Vec2i64, std::unique_ptr<Chunk>, math::SpatialHash> m_chunks;
        std::mutex m_chunkMutex;

        std::vector<std::unique_ptr<Chunk>> m_chunkPool;
        std::mutex m_poolMutex;

        int64_t m_renderDistanceChunks = 2;
        rendering::ViewDirection m_currentViewDir = rendering::ViewDirection::North;

        math::Vec2i64 m_lastCenterChunk{0x7FFFFFFFFFFFFFFF, 0x7FFFFFFFFFFFFFFF};
    };

}