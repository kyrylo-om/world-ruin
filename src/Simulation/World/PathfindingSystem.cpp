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
    const GlobalObstacleMap& obstacles) {

    math::Vec2i64 startGrid{static_cast<int64_t>(std::floor(start.x)), static_cast<int64_t>(std::floor(start.y))};
    math::Vec2i64 targetGrid{static_cast<int64_t>(std::floor(target.x)), static_cast<int64_t>(std::floor(target.y))};

    std::vector<math::Vec2f> path;

    if (startGrid == targetGrid) {
        path.push_back(target);
        return path;
    }

    thread_local std::priority_queue<AStarNode, std::vector<AStarNode>, std::greater<AStarNode>> openSet;
    thread_local std::unordered_map<math::Vec2i64, math::Vec2i64, math::SpatialHash> cameFrom;
    thread_local std::unordered_map<math::Vec2i64, float, math::SpatialHash> gScore;
    thread_local std::unordered_map<math::Vec2i64, const world::Chunk*, math::SpatialHash> chunkCache;

    openSet = std::priority_queue<AStarNode, std::vector<AStarNode>, std::greater<AStarNode>>();
    cameFrom.clear();
    gScore.clear();
    chunkCache.clear();

    auto getElevationFast = [&](math::Vec2i64 pos) -> uint8_t {
        math::Vec2i64 cPos = math::worldToChunk(pos.x, pos.y);
        const world::Chunk* c = nullptr;

        auto it = chunkCache.find(cPos);
        if (it != chunkCache.end()) {
            c = it->second;
        } else {
            c = chunkManager.getChunkThreadSafe(cPos);
            chunkCache[cPos] = c;
        }

        if (c) {
            int64_t lx = pos.x - cPos.x * core::CHUNK_SIZE;
            int64_t ly = pos.y - cPos.y * core::CHUNK_SIZE;
            if (lx >= 0 && lx < core::CHUNK_SIZE && ly >= 0 && ly < core::CHUNK_SIZE) {
                return c->getLevel(lx, ly);
            }
        }
        return 3;
    };

    openSet.push({startGrid, 0.0f, 0.0f});
    gScore[startGrid] = 0.0f;

    int iterations = 0;
    const int MAX_ITERATIONS = 15000;

    math::Vec2i64 bestNode = startGrid;
    float bestDist = 999999.0f;

    uint8_t targetElevation = getElevationFast(targetGrid);

    math::Vec2i64 dirs[8] = {
        {1, 0}, {-1, 0}, {0, 1}, {0, -1},
        {1, 1}, {-1, -1}, {1, -1}, {-1, 1}
    };

    while (!openSet.empty() && iterations < MAX_ITERATIONS) {
        AStarNode current = openSet.top();
        openSet.pop();

        if (current.pos == targetGrid) {
            bestNode = current.pos;
            break;
        }

        iterations++;
        uint8_t currentElevation = getElevationFast(current.pos);

        for (int i = 0; i < 8; ++i) {
            math::Vec2i64 next = {current.pos.x + dirs[i].x, current.pos.y + dirs[i].y};

            if (next != targetGrid && obstacles.isBlocked(next.x, next.y)) continue;

            uint8_t nextElevation = getElevationFast(next);

            if (std::abs(nextElevation - currentElevation) > 1) continue;

            if (i >= 4) {
                math::Vec2i64 adj1 = {current.pos.x + dirs[i].x, current.pos.y};
                math::Vec2i64 adj2 = {current.pos.x, current.pos.y + dirs[i].y};

                if (obstacles.isBlocked(adj1.x, adj1.y) || obstacles.isBlocked(adj2.x, adj2.y)) continue;

                uint8_t e1 = getElevationFast(adj1);
                uint8_t e2 = getElevationFast(adj2);

                if (std::abs(e1 - currentElevation) > 1 || std::abs(e2 - currentElevation) > 1) continue;
                if (std::abs(nextElevation - e1) > 1 || std::abs(nextElevation - e2) > 1) continue;
            }

            float moveCost = (i < 4) ? 1.0f : 1.414f;
            float tentative_g = gScore[current.pos] + moveCost;

            if (gScore.find(next) == gScore.end() || tentative_g < gScore[next]) {
                cameFrom[next] = current.pos;
                gScore[next] = tentative_g;

                float dx = std::abs(static_cast<float>(next.x - targetGrid.x));
                float dy = std::abs(static_cast<float>(next.y - targetGrid.y));

                float h = (dx + dy) + (1.414f - 2.0f) * std::min(dx, dy);

                float elevDiff = std::abs(static_cast<float>(nextElevation) - static_cast<float>(targetElevation));
                h += (elevDiff * 25.0f);

                if (h < bestDist) {
                    bestDist = h;
                    bestNode = next;
                }

                openSet.push({next, tentative_g, tentative_g + (h * 1.5f)});
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