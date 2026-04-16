#include "World/ChunkManager.hpp"
#include "../../include/World/Meshing/ChunkMesher.hpp"
#include "Rendering/TileHandler.hpp"
#include "World/Biomes.hpp"
#include "ECS/Components.hpp"
#include "ECS/Tags.hpp"
#include <cmath>
#include <algorithm>

namespace wr::world {

ChunkManager::ChunkManager(core::ThreadPool& threadPool,
                           entt::registry& registry,
                           const WorldGenerator& generator,
                           const rendering::TileHandler& tileHandler)
    : m_threadPool(threadPool), m_registry(registry), m_generator(generator), m_tileHandler(tileHandler) {}

void ChunkManager::update(const math::Vec2f& cameraCenterWorldPx, rendering::ViewDirection viewDir) {
    m_currentViewDir = viewDir;

    core::GlobalCoord centerBlockX = static_cast<core::GlobalCoord>(std::floor(cameraCenterWorldPx.x / core::TILE_PIXEL_SIZE));
    core::GlobalCoord centerBlockY = static_cast<core::GlobalCoord>(std::floor(cameraCenterWorldPx.y / core::TILE_PIXEL_SIZE));
    math::Vec2i64 centerChunk = math::worldToChunk(centerBlockX, centerBlockY);

    if (centerChunk != m_lastCenterChunk) {
        {
            std::unique_lock<std::shared_mutex> lock(m_chunkMutex);
            for (int64_t y = -m_renderDistanceChunks; y <= m_renderDistanceChunks; ++y) {
                for (int64_t x = -m_renderDistanceChunks; x <= m_renderDistanceChunks; ++x) {
                    math::Vec2i64 targetCoord{centerChunk.x + x, centerChunk.y + y};

                    if (m_chunks.find(targetCoord) == m_chunks.end()) {
                        std::unique_ptr<Chunk> newChunk;
                        {
                            std::lock_guard<std::mutex> poolLock(m_poolMutex);
                            if (!m_chunkPool.empty()) {
                                newChunk = std::move(m_chunkPool.back());
                                m_chunkPool.pop_back();
                                newChunk->recycle(targetCoord);
                            }
                        }

                        if (!newChunk) {
                            newChunk = std::make_unique<Chunk>(targetCoord);
                        }

                        m_chunks[targetCoord] = std::move(newChunk);
                        enqueueChunkGeneration(targetCoord, m_currentViewDir);
                    }
                }
            }
        }

        std::vector<math::Vec2i64> toUnload;
        {
            std::shared_lock<std::shared_mutex> lock(m_chunkMutex);
            for (auto& [coord, chunk] : m_chunks) {
                if (std::abs(coord.x - centerChunk.x) > m_renderDistanceChunks ||
                    std::abs(coord.y - centerChunk.y) > m_renderDistanceChunks) {
                    toUnload.push_back(coord);
                }
            }
        }

        if (!toUnload.empty()) {
            for (const auto& coord : toUnload) {
                unloadChunk(coord);
            }
        }

        m_lastCenterChunk = centerChunk;
    }

    int meshesUploadedThisFrame = 0;
    bool entitySpawnedThisFrame = false;

    {
        std::unique_lock<std::shared_mutex> lock(m_chunkMutex);
        for (auto& [coord, chunk] : m_chunks) {
            if (chunk->state.load(std::memory_order_acquire) == ChunkState::DataReady) {

                if (chunk->pendingMeshData || chunk->pendingLodImage) {
                    if (meshesUploadedThisFrame < 2) {
                        finalizeReadyChunk(coord, *chunk);
                        meshesUploadedThisFrame++;
                    }
                }

                if (!entitySpawnedThisFrame && !chunk->pendingEntities.empty()) {
                    int spawnedThisFrame = 0;
                    const int MAX_SPAWNS = 300;

                    while (!chunk->pendingEntities.empty() && spawnedThisFrame < MAX_SPAWNS) {
                        auto data = chunk->pendingEntities.back();
                        chunk->pendingEntities.pop_back();
                        spawnEntityForChunk(coord, *chunk, data);
                        spawnedThisFrame++;
                    }
                    entitySpawnedThisFrame = true;
                }

                if (chunk->pendingEntities.empty() && !chunk->pendingMeshData && !chunk->pendingLodImage) {
                    chunk->state.store(ChunkState::Active, std::memory_order_release);
                }

                if (meshesUploadedThisFrame >= 2 && entitySpawnedThisFrame) {
                    break;
                }
            }
        }
    }
}

void ChunkManager::enqueueChunkGeneration(math::Vec2i64 chunkCoord, rendering::ViewDirection viewDir) {
    auto& chunk = m_chunks[chunkCoord];
    chunk->state.store(ChunkState::Generating, std::memory_order_release);

    m_threadPool.enqueue([this, chunkCoord]() {
        core::GlobalCoord startX = chunkCoord.x * core::CHUNK_SIZE;
        core::GlobalCoord startY = chunkCoord.y * core::CHUNK_SIZE;

        std::vector<TileInfo> chunkTiles;
        chunkTiles.reserve(core::CHUNK_SIZE * core::CHUNK_SIZE);

        m_generator.generateChunkData(chunkTiles, startX, startY);

        std::vector<EntitySpawnData> spawnData;
        m_generator.generateEntities(spawnData, startX, startY, chunkTiles);

        ChunkMeshData meshData = ChunkMesher::generateMeshes(chunkTiles, m_generator, m_tileHandler, startX, startY);

        std::unique_lock<std::shared_mutex> lock(m_chunkMutex);

        auto it = m_chunks.find(chunkCoord);
        if (it != m_chunks.end()) {
            Chunk* c = it->second.get();

            for (int64_t ly = 0; ly < core::CHUNK_SIZE; ++ly) {
                for (int64_t lx = 0; lx < core::CHUNK_SIZE; ++lx) {
                    const auto& info = chunkTiles[ly * core::CHUNK_SIZE + lx];
                    c->setTile(lx, ly, info.type, info.elevationLevel);
                }
            }

            c->pendingEntities = std::move(spawnData);
            c->pendingLodImage = std::move(meshData.lodImage);
            c->pendingMeshData = std::make_unique<ChunkMeshData>(std::move(meshData));

            c->state.store(ChunkState::DataReady, std::memory_order_release);
        }
    });
}

core::TerrainType ChunkManager::getGlobalBlockType(core::GlobalCoord x, core::GlobalCoord y) {
    return m_generator.generateBlock(x, y);
}

void ChunkManager::unloadChunk(math::Vec2i64 chunkCoord) {
    std::unique_lock<std::shared_mutex> lock(m_chunkMutex);
    auto it = m_chunks.find(chunkCoord);

    if (it != m_chunks.end()) {
        m_registry.destroy(it->second->entities.begin(), it->second->entities.end());
        it->second->entities.clear();

        for (int y = 0; y < 16; ++y) {
            for (int x = 0; x < 16; ++x) {
                it->second->spatialCells[x][y].clear();
            }
        }

        std::lock_guard<std::mutex> poolLock(m_poolMutex);
        m_chunkPool.push_back(std::move(it->second));
        m_chunks.erase(it);
    }
}

TileInfo ChunkManager::getGlobalTileInfo(core::GlobalCoord x, core::GlobalCoord y) {
    math::Vec2i64 chunkCoord = math::worldToChunk(x, y);
    std::shared_lock<std::shared_mutex> lock(m_chunkMutex);
    auto it = m_chunks.find(chunkCoord);

    if (it != m_chunks.end() && it->second->state.load(std::memory_order_acquire) == ChunkState::Active) {
        int64_t lx = x - chunkCoord.x * core::CHUNK_SIZE;
        int64_t ly = y - chunkCoord.y * core::CHUNK_SIZE;
        TileInfo info;
        info.type = it->second->getTile(lx, ly);
        info.elevationLevel = it->second->getLevel(lx, ly);
        return info;
    }
    return m_generator.generateTile(x, y);
}

const Chunk* ChunkManager::getChunkThreadSafe(math::Vec2i64 chunkCoord) const noexcept {
    std::shared_lock<std::shared_mutex> lock(m_chunkMutex);
    auto it = m_chunks.find(chunkCoord);
    if (it != m_chunks.end() && it->second->state.load(std::memory_order_acquire) == ChunkState::Active) {
        return it->second.get();
    }
    return nullptr;
}

void ChunkManager::registerEntity(entt::entity entity, const math::Vec2f& worldPos) {
    int64_t cx = static_cast<int64_t>(std::floor(worldPos.x / 64.0f));
    int64_t cy = static_cast<int64_t>(std::floor(worldPos.y / 64.0f));

    std::unique_lock<std::shared_mutex> lock(m_chunkMutex);
    auto it = m_chunks.find({cx, cy});
    if (it != m_chunks.end() && it->second->state.load(std::memory_order_acquire) == ChunkState::Active) {
        it->second->entities.push_back(entity);

        int lx = static_cast<int>(std::floor(worldPos.x)) - cx * 64;
        int ly = static_cast<int>(std::floor(worldPos.y)) - cy * 64;
        int cellX = std::clamp(lx / 4, 0, 15);
        int cellY = std::clamp(ly / 4, 0, 15);

        it->second->spatialCells[cellX][cellY].push_back(entity);
    }
}

}