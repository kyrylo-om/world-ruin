#pragma once
#include "Core/Math.hpp"
#include "World/ChunkManager.hpp"
#include <vector>
#include <memory>

namespace wr::simulation {

    struct GlobalObstacleMap {
        int64_t minX{0}, minY{0}, width{0}, height{0};
        std::vector<uint8_t> grid;

        void init(int64_t mx, int64_t my, int64_t w, int64_t h) {
            minX = mx; minY = my; width = w; height = h;
            grid.assign(w * h, 0);
        }

        void set(int64_t x, int64_t y, uint8_t val = 1) {
            int64_t lx = x - minX;
            int64_t ly = y - minY;
            if (lx >= 0 && lx < width && ly >= 0 && ly < height) {
                grid[ly * width + lx] = val;
            }
        }

        bool isBlocked(int64_t x, int64_t y) const {
            int64_t lx = x - minX;
            int64_t ly = y - minY;
            if (lx >= 0 && lx < width && ly >= 0 && ly < height) {
                return grid[ly * width + lx] != 0;
            }
            return false;
        }
    };

    class PathfindingSystem {
    public:
        static std::vector<math::Vec2f> findPath(
            const math::Vec2f& startWorld,
            const math::Vec2f& targetWorld,
            world::ChunkManager& chunkManager,
            const GlobalObstacleMap& obstacles);
    };

}