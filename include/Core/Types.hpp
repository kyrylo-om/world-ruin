#pragma once

#include <cstdint>
#include <cstddef>

namespace wr::core {

    using GlobalCoord = int64_t;

    using ChunkCoord = int64_t;

    constexpr GlobalCoord CHUNK_SIZE = 64;
    constexpr float TILE_PIXEL_SIZE = 64.0f;
    constexpr float CHUNK_PIXEL_SIZE = CHUNK_SIZE * TILE_PIXEL_SIZE;

    enum class TerrainType : uint8_t {
        Water = 0,
        Ground = 1
    };

    enum class Direction : uint8_t {
        North = 0,
        East  = 1,
        South = 2,
        West  = 3
    };

}