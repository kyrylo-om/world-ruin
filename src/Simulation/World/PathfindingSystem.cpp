#include "../../../include/Simulation/World/PathfindingSystem.hpp"
#include <queue>
#include <unordered_map>
#include <cmath>
#include <algorithm>

namespace wr::simulation {

struct AStarNode {
    math::Vec2i64 pos;
    float gCost;
    float fCost;

    bool operator>(const AStarNode& other) const {
        return fCost > other.fCost;
    }
};

std::vector<math::Vec2f> PathfindingSystem::findPath(
    const math::Vec2f& start, const math::Vec2f& target,
    world::ChunkManager& chunkManager,
    const std::unordered_set<math::Vec2i64, math::SpatialHash>& obstacles) {

    math::Vec2i64 startGrid{static_cast<int64_t>(std::floor(start.x)), static_cast<int64_t>(std::floor(start.y))};
    math::Vec2i64 targetGrid{static_cast<int64_t>(std::floor(target.x)), static_cast<int64_t>(std::floor(target.y))};

    std::vector<math::Vec2f> path;

    if (startGrid == targetGrid) {
        path.push_back(target);
        return path;
    }

    std::priority_queue<AStarNode, std::vector<AStarNode>, std::greater<AStarNode>> openSet;
    std::unordered_map<math::Vec2i64, math::Vec2i64, math::SpatialHash> cameFrom;
    std::unordered_map<math::Vec2i64, float, math::SpatialHash> gScore;

    openSet.push({startGrid, 0.0f, 0.0f});
    gScore[startGrid] = 0.0f;

    int iterations = 0;
    const int MAX_ITERATIONS = 4000;

    math::Vec2i64 bestNode = startGrid;
    float bestDist = std::abs(startGrid.x - targetGrid.x) + std::abs(startGrid.y - targetGrid.y);

    while (!openSet.empty() && iterations < MAX_ITERATIONS) {
        AStarNode current = openSet.top();
        openSet.pop();

        if (current.pos == targetGrid) {
            bestNode = current.pos;
            break;
        }

        iterations++;

        math::Vec2i64 neighbors[4] = {
            {current.pos.x + 1, current.pos.y},
            {current.pos.x - 1, current.pos.y},
            {current.pos.x, current.pos.y + 1},
            {current.pos.x, current.pos.y - 1}
        };

        auto currentInfo = chunkManager.getGlobalTileInfo(current.pos.x, current.pos.y);

        for (const auto& next : neighbors) {
            auto nextInfo = chunkManager.getGlobalTileInfo(next.x, next.y);

            if (std::abs(nextInfo.elevationLevel - currentInfo.elevationLevel) > 1) continue;

            if (next != targetGrid && obstacles.count(next)) continue;

            float tentative_g = gScore[current.pos] + 1.0f;

            if (gScore.find(next) == gScore.end() || tentative_g < gScore[next]) {
                cameFrom[next] = current.pos;
                gScore[next] = tentative_g;

                float h = (std::abs(next.x - targetGrid.x) + std::abs(next.y - targetGrid.y)) * 1.05f;
                openSet.push({next, tentative_g, tentative_g + h});

                if (h < bestDist) {
                    bestDist = h;
                    bestNode = next;
                }
            }
        }
    }

    math::Vec2i64 curr = bestNode;
    while (curr != startGrid) {
        path.push_back({static_cast<float>(curr.x) + 0.5f, static_cast<float>(curr.y) + 0.5f});
        if (cameFrom.find(curr) == cameFrom.end()) break;
        curr = cameFrom[curr];
    }

    std::reverse(path.begin(), path.end());

    if (bestNode == targetGrid) {
        path.push_back(target);
    }

    return path;
}

}