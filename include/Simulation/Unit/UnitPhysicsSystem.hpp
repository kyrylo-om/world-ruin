#pragma once

#include "World/ChunkManager.hpp"
#include "Core/Math.hpp"
#include "ECS/Components.hpp"
#include <entt/entt.hpp>
#include <vector>
#include <algorithm>
#include <memory>
#include <cmath>

namespace wr::simulation {

    struct FastChunkCache {
        int64_t minX = 9999999, maxX = -9999999;
        int64_t minY = 9999999, maxY = -9999999;
        int64_t width = 0;
        int64_t height = 0;
        std::vector<const world::Chunk*> grid;

        void reset() {
            minX = 9999999;
            maxX = -9999999;
            minY = 9999999;
            maxY = -9999999;
            width = 0;
            height = 0;
            grid.clear();
        }

        void build(const std::unordered_map<math::Vec2i64, std::unique_ptr<world::Chunk>, math::SpatialHash>& chunks) {
            reset();
            if (chunks.empty()) return;
            for (const auto& [coord, chunk] : chunks) {
                if (chunk->state.load(std::memory_order_acquire) != world::ChunkState::Active) continue;
                minX = std::min(minX, coord.x);
                maxX = std::max(maxX, coord.x);
                minY = std::min(minY, coord.y);
                maxY = std::max(maxY, coord.y);
            }
            if (minX > maxX) return;
            width = maxX - minX + 1;
            height = maxY - minY + 1;
            grid.assign(width * height, nullptr);
            for (const auto& [coord, chunk] : chunks) {
                if (chunk->state.load(std::memory_order_acquire) != world::ChunkState::Active) continue;
                grid[(coord.y - minY) * width + (coord.x - minX)] = chunk.get();
            }
        }

        void build(const std::unordered_map<math::Vec2i64, std::unique_ptr<world::Chunk>, math::SpatialHash>& chunks,
                   const math::Vec2i64& centerChunk,
                   int64_t radiusChunks) {
            reset();
            if (chunks.empty() || radiusChunks < 0) return;

            for (const auto& [coord, chunk] : chunks) {
                if (chunk->state.load(std::memory_order_acquire) != world::ChunkState::Active) continue;
                if (std::abs(coord.x - centerChunk.x) > radiusChunks || std::abs(coord.y - centerChunk.y) > radiusChunks) continue;

                minX = std::min(minX, coord.x);
                maxX = std::max(maxX, coord.x);
                minY = std::min(minY, coord.y);
                maxY = std::max(maxY, coord.y);
            }

            if (minX > maxX) return;

            width = maxX - minX + 1;
            height = maxY - minY + 1;
            grid.assign(width * height, nullptr);

            for (const auto& [coord, chunk] : chunks) {
                if (chunk->state.load(std::memory_order_acquire) != world::ChunkState::Active) continue;
                if (std::abs(coord.x - centerChunk.x) > radiusChunks || std::abs(coord.y - centerChunk.y) > radiusChunks) continue;
                grid[(coord.y - minY) * width + (coord.x - minX)] = chunk.get();
            }
        }

        [[nodiscard]] bool containsChunk(int64_t cx, int64_t cy) const noexcept {
            return width > 0 && height > 0 && cx >= minX && cx <= maxX && cy >= minY && cy <= maxY;
        }

        [[nodiscard]] bool containsBlock(int64_t bx, int64_t by) const noexcept {
            math::Vec2i64 chunkCoord = math::worldToChunk(bx, by);
            return containsChunk(chunkCoord.x, chunkCoord.y);
        }

        [[nodiscard]] bool containsWorld(float wx, float wy) const noexcept {
            int64_t bx = static_cast<int64_t>(std::floor(wx));
            int64_t by = static_cast<int64_t>(std::floor(wy));
            return containsBlock(bx, by);
        }

        const world::Chunk* get(int64_t cx, int64_t cy) const {
            if (grid.empty() || cx < minX || cx > maxX || cy < minY || cy > maxY) return nullptr;
            return grid[(cy - minY) * width + (cx - minX)];
        }
    };

    struct SpatialGridData {
        entt::entity entity;
        float x;
        float y;
        float radius;
    };

    class EntitySpatialGrid {
    public:
        template<typename View>
        void rebuild(entt::registry& registry, const FastChunkCache& chunkCache, View view) {
            if (chunkCache.width == 0) { m_width = 0; m_height = 0; return; }

            m_minX = chunkCache.minX * 16;
            m_minY = chunkCache.minY * 16;
            int64_t maxCellX = (chunkCache.maxX + 1) * 16;
            int64_t maxCellY = (chunkCache.maxY + 1) * 16;

            m_width = maxCellX - m_minX;
            m_height = maxCellY - m_minY;

            size_t gridTotal = static_cast<size_t>(m_width * m_height);
            m_offsets.assign(gridTotal + 1, 0);

            for (auto e : view) {
                auto& wp = registry.get<ecs::WorldPos>(e);
                int64_t cx = static_cast<int64_t>(std::floor(wp.wx / m_cellSize)) - m_minX;
                int64_t cy = static_cast<int64_t>(std::floor(wp.wy / m_cellSize)) - m_minY;
                if (cx >= 0 && cx < m_width && cy >= 0 && cy < m_height) {
                    m_offsets[cy * m_width + cx]++;
                }
            }

            size_t total = 0;
            for (size_t i = 0; i < gridTotal; ++i) {
                size_t count = m_offsets[i];
                m_offsets[i] = total;
                total += count;
            }
            m_offsets[gridTotal] = total;

            m_data.resize(total);
            std::vector<size_t> currentOffset = m_offsets;

            for (auto e : view) {
                auto& wp = registry.get<ecs::WorldPos>(e);
                int64_t cx = static_cast<int64_t>(std::floor(wp.wx / m_cellSize)) - m_minX;
                int64_t cy = static_cast<int64_t>(std::floor(wp.wy / m_cellSize)) - m_minY;
                if (cx >= 0 && cx < m_width && cy >= 0 && cy < m_height) {
                    size_t idx = cy * m_width + cx;

                    float r = 0.3f;
                    if (auto* hb = registry.try_get<ecs::Hitbox>(e)) r = hb->radius / 64.0f;
                    else if (auto* ca = registry.try_get<ecs::ClickArea>(e)) r = ca->radius / 64.0f;

                    m_data[currentOffset[idx]++] = {e, wp.wx, wp.wy, r};
                }
            }
        }

        template<typename Func>
        void forEachNearby(const math::Vec2f& pos, float radius, Func&& callback) const {
            if (m_width <= 0 || m_height <= 0) return;

            int64_t minCx = std::max(int64_t{0}, static_cast<int64_t>(std::floor((pos.x - radius) / m_cellSize)) - m_minX);
            int64_t maxCx = std::min(m_width - 1, static_cast<int64_t>(std::floor((pos.x + radius) / m_cellSize)) - m_minX);
            int64_t minCy = std::max(int64_t{0}, static_cast<int64_t>(std::floor((pos.y - radius) / m_cellSize)) - m_minY);
            int64_t maxCy = std::min(m_height - 1, static_cast<int64_t>(std::floor((pos.y + radius) / m_cellSize)) - m_minY);

            for (int64_t y = minCy; y <= maxCy; ++y) {
                for (int64_t x = minCx; x <= maxCx; ++x) {
                    size_t idx = y * m_width + x;
                    size_t start = m_offsets[idx];
                    size_t end = m_offsets[idx + 1];
                    for (size_t i = start; i < end; ++i) {
                        callback(m_data[i]);
                    }
                }
            }
        }

    private:
        int64_t m_minX{0};
        int64_t m_minY{0};
        int64_t m_width{0};
        int64_t m_height{0};
        const float m_cellSize{4.0f};

        std::vector<size_t> m_offsets;
        std::vector<SpatialGridData> m_data;
    };

    struct PhysicsTimers {
        double gridQuery = 0.0;
        double collisionMath = 0.0;
        double stateUpdate = 0.0;
    };

    class UnitPhysicsSystem {
    public:
        static void applyPhysicsAndCollisions(entt::registry& registry, entt::entity entity, ecs::WorldPos& wp, ecs::Velocity& vel, ecs::LogicalPos& lp, ecs::AnimationState& anim, float dt, const FastChunkCache& chunkCache, const EntitySpatialGrid& solidGrid, const EntitySpatialGrid& unitGrid, PhysicsTimers& pt);
    };

}