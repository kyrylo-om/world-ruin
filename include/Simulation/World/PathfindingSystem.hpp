#pragma once

#include "Core/Math.hpp"
#include "World/ChunkManager.hpp"
#include <vector>
#include <unordered_set>

namespace wr::simulation {

    class PathfindingSystem {
    public:

        static std::vector<math::Vec2f> findPath(
            const math::Vec2f& startWorld,
            const math::Vec2f& targetWorld,
            world::ChunkManager& chunkManager,
            const std::unordered_set<math::Vec2i64, math::SpatialHash>& obstacles);
    };

}