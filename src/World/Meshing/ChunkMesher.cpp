#include "../../../include/World/Meshing/ChunkMesher.hpp"
#include "World/Meshing/ChunkLODMesher.hpp"
#include "Rendering/Perspective.hpp"
#include "World/Meshing/MeshingContext.hpp"
#include "World/Meshing/MeshingPasses.hpp"

namespace wr::world {

ChunkMeshData ChunkMesher::generateMeshes(const std::vector<TileInfo>& chunkTiles,
                                          const WorldGenerator& generator,
                                          const rendering::TileHandler& tileHandler,
                                          core::GlobalCoord startX,
                                          core::GlobalCoord startY)
{
    ChunkMeshData data;

    data.lodImage = ChunkLODMesher::generateLODImage(chunkTiles);

    for (int i = 0; i < 4; ++i) {
        data.oceanVertices[i].setPrimitiveType(sf::Quads);
        data.riverVertices[i].setPrimitiveType(sf::Quads);
        data.foamLayers[i].setPrimitiveType(sf::Quads);

        data.shadowVertices[i].setPrimitiveType(sf::Quads);
        data.shadow4Vertices[i].setPrimitiveType(sf::Quads);
        data.shadow6Vertices[i].setPrimitiveType(sf::Quads);

        data.groundLayers[i].setPrimitiveType(sf::Quads);
        data.rampLayers[i].setPrimitiveType(sf::Quads);
        data.cliffLayers[i].setPrimitiveType(sf::Quads);
        data.level3Layers[i].setPrimitiveType(sf::Quads);

        data.ramp4Layers[i].setPrimitiveType(sf::Quads);
        data.cliff4Layers[i].setPrimitiveType(sf::Quads);
        data.level5Layers[i].setPrimitiveType(sf::Quads);

        data.ramp6Layers[i].setPrimitiveType(sf::Quads);
        data.cliff6Layers[i].setPrimitiveType(sf::Quads);
        data.level7Layers[i].setPrimitiveType(sf::Quads);
    }

    for (int i = 0; i < 4; ++i) {
        MeshingContext ctx{chunkTiles, generator, tileHandler, startX, startY, static_cast<rendering::ViewDirection>(i), i};

        switch (ctx.currentGenDir) {
            case rendering::ViewDirection::North: ctx.dxUp=0; ctx.dyUp=-1; ctx.dxDown=0; ctx.dyDown=1; ctx.dxLeft=-1; ctx.dyLeft=0; ctx.dxRight=1; ctx.dyRight=0; break;
            case rendering::ViewDirection::East:  ctx.dxUp=-1; ctx.dyUp=0; ctx.dxDown=1; ctx.dyDown=0; ctx.dxLeft=0; ctx.dyLeft=-1; ctx.dxRight=0; ctx.dyRight=1; break;
            case rendering::ViewDirection::South: ctx.dxUp=0; ctx.dyUp=1; ctx.dxDown=0; ctx.dyDown=-1; ctx.dxLeft=1; ctx.dyLeft=0; ctx.dxRight=-1; ctx.dyRight=0; break;
            case rendering::ViewDirection::West:  ctx.dxUp=1; ctx.dyUp=0; ctx.dxDown=-1; ctx.dyDown=0; ctx.dxLeft=0; ctx.dyLeft=1; ctx.dxRight=0; ctx.dyRight=-1; break;
        }

        ctx.grassOverWater = tileHandler.getGrassOverWater();
        ctx.grassOverStone = tileHandler.getGrassOverStone();
        ctx.waterCliffs = tileHandler.getWaterCliffs();
        ctx.stoneCliffs = tileHandler.getStoneCliffs();
        ctx.ramps = tileHandler.getRamps();

        for (int64_t ly = 0; ly < core::CHUNK_SIZE; ++ly) {
            for (int64_t lx = 0; lx < core::CHUNK_SIZE; ++lx) {
                const auto& info = chunkTiles[ly * core::CHUNK_SIZE + lx];

                passes::processFoundations(data, ctx, lx, ly, info);
                passes::processWaterFoam(data, ctx, lx, ly, info);
                passes::processCliffsAndShadows(data, ctx, lx, ly, info);
                passes::processRamps(data, ctx, lx, ly, info);
                passes::processPlateauTops(data, ctx, lx, ly, info);
            }
        }
    }
    return data;
}

}