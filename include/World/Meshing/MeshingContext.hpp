#pragma once

#include "Core/Types.hpp"
#include "../Generation/WorldGenerator.hpp"
#include "Rendering/TileHandler.hpp"
#include "World/Biomes.hpp"
#include <vector>
#include "Rendering/Perspective.hpp"
#include <SFML/Graphics/VertexArray.hpp>

namespace wr::world {

    struct MeshingContext {
        const std::vector<TileInfo>& chunkTiles;
        const WorldGenerator& generator;
        const rendering::TileHandler& tileHandler;
        core::GlobalCoord startX;
        core::GlobalCoord startY;
        rendering::ViewDirection currentGenDir;
        int dirIdx;

        int dxUp, dyUp, dxDown, dyDown, dxLeft, dyLeft, dxRight, dyRight;

        rendering::TileHandler::Platform grassOverWater;
        rendering::TileHandler::Platform grassOverStone;
        rendering::TileHandler::CliffSet waterCliffs;
        rendering::TileHandler::CliffSet stoneCliffs;
        rendering::TileHandler::RampSet ramps;

        [[nodiscard]] TileInfo getInfo(int64_t lx, int64_t ly) const {
            if (lx >= 0 && lx < core::CHUNK_SIZE && ly >= 0 && ly < core::CHUNK_SIZE) {
                return chunkTiles[ly * core::CHUNK_SIZE + lx];
            }
            return generator.generateTile(startX + lx, startY + ly);
        }
    };

    void addQuad(sf::VertexArray& va, float px, float py, sf::IntRect tex, const MeshingContext& ctx,
                 BiomeType biome, float size = 64.0f, bool rotate180 = false,
                 bool applyGroundRot = false, bool applyCliffRot = false);

    bool isShadowVisible(rendering::ViewDirection view, int dx, int dy);
    bool getCliffStyle(int64_t lx, int64_t ly, const MeshingContext& ctx);

}