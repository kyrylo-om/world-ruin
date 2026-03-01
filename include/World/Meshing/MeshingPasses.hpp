#pragma once

#include "World/Meshing/MeshingContext.hpp"
#include "ChunkMesher.hpp"

namespace wr::world::passes {

    void processFoundations(ChunkMeshData& data, const MeshingContext& ctx, int64_t lx, int64_t ly, const TileInfo& info);
    void processWaterFoam(ChunkMeshData& data, const MeshingContext& ctx, int64_t lx, int64_t ly, const TileInfo& info);
    void processCliffsAndShadows(ChunkMeshData& data, const MeshingContext& ctx, int64_t lx, int64_t ly, const TileInfo& info);
    void processRamps(ChunkMeshData& data, const MeshingContext& ctx, int64_t lx, int64_t ly, const TileInfo& info);
    void processPlateauTops(ChunkMeshData& data, const MeshingContext& ctx, int64_t lx, int64_t ly, const TileInfo& info);

}