#pragma once

#include "Core/Types.hpp"
#include "World/Biomes.hpp"
#include "RiverGenerator.hpp"
#include <FastNoiseLite.h>
#include <memory>
#include <atomic>
#include <vector>

namespace wr::world {

    struct TileInfo {
        core::TerrainType type;
        BiomeType biome;
        float rawElevation;
        uint8_t elevationLevel;
        bool isRiver;
        bool isRamp;
    };

    struct EntitySpawnData {
        int64_t lx;
        int64_t ly;
        uint8_t assetType;
        uint8_t entityClass;
        float offsetX;
        float offsetY;
    };

    class WorldGenerator {
    public:
        explicit WorldGenerator(uint32_t seed);

        [[nodiscard]] TileInfo generateTile(core::GlobalCoord x, core::GlobalCoord y) const noexcept;
        [[nodiscard]] core::TerrainType generateBlock(core::GlobalCoord x, core::GlobalCoord y) const noexcept;

        void generateChunkData(std::vector<TileInfo>& outTiles, core::GlobalCoord startX, core::GlobalCoord startY) const;

        void generateEntities(std::vector<EntitySpawnData>& outEntities, core::GlobalCoord startX, core::GlobalCoord startY, const std::vector<TileInfo>& chunkTiles) const;

        void debug_logCounts() const;

    private:
        [[nodiscard]] float getRawElevationNoise(core::GlobalCoord x, core::GlobalCoord y) const noexcept;
        [[nodiscard]] uint8_t getTopologicalLevel(core::GlobalCoord x, core::GlobalCoord y) const noexcept;

        uint32_t m_seed;
        FastNoiseLite m_elevationNoise;
        FastNoiseLite m_moistureNoise;
        FastNoiseLite m_rampNoise;

        std::unique_ptr<RiverGenerator> m_riverGen;

        mutable std::atomic<uint64_t> m_debugTotalTiles{0};
        mutable std::atomic<uint64_t> m_debugLevel1Tiles{0};
        mutable std::atomic<uint64_t> m_debugNearCliffTiles{0};
        mutable std::atomic<uint64_t> m_rampCounter{0};

    };

}