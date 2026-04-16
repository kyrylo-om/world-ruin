#include "../../../include/World/Generation/WorldGenerator.hpp"
#include "World/Biomes.hpp"
#include <iostream>

namespace wr::world {

    WorldGenerator::WorldGenerator(uint32_t seed) : m_seed(seed) {
        m_elevationNoise.SetSeed(seed);
        m_elevationNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
        m_elevationNoise.SetFrequency(0.003f);
        m_elevationNoise.SetFractalType(FastNoiseLite::FractalType_FBm);
        m_elevationNoise.SetFractalOctaves(5);

        m_moistureNoise.SetSeed(seed + 100);
        m_moistureNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
        m_moistureNoise.SetFrequency(0.002f);

        m_rampNoise.SetSeed(seed + 300);
        m_rampNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
        m_rampNoise.SetFrequency(0.5f);

        m_riverGen = std::make_unique<RiverGenerator>(seed + 200);
    }

    void WorldGenerator::debug_logCounts() const {
        uint64_t total = m_debugTotalTiles.exchange(0);
        uint64_t lvl1 = m_debugLevel1Tiles.exchange(0);
        uint64_t nearCliff = m_debugNearCliffTiles.exchange(0);
        uint64_t ramps = m_rampCounter.exchange(0);

        if (total > 0) {
            std::cout << "\n[WorldGen Pipeline]"
                      << " Tiles Evaluated: " << total
                      << " | Valid Lvl1: " << lvl1
                      << " | Touching Cliff: " << nearCliff
                      << " | Ramps Placed: " << ramps << "\n";
        } else {
            std::cout << "[WorldGen Debug] Idle (0 new chunks generated this second. Move the camera!)" << std::endl;
        }
    }

    core::TerrainType WorldGenerator::generateBlock(core::GlobalCoord x, core::GlobalCoord y) const noexcept {
        return generateTile(x, y).type;
    }

    TileInfo WorldGenerator::generateTile(core::GlobalCoord x, core::GlobalCoord y) const noexcept {
        m_debugTotalTiles.fetch_add(1, std::memory_order_relaxed);

        TileInfo info;
        info.rawElevation = getRawElevationNoise(x, y);
        info.isRiver = m_riverGen->isRiver(x, y);
        info.isRamp = false;

        info.elevationLevel = getTopologicalLevel(x, y);

        if (info.rawElevation < 0.35f) {
            info.type = core::TerrainType::Water;
            info.biome = (info.rawElevation < 0.0f) ? BiomeType::River : BiomeType::Ocean;
            info.elevationLevel = 1;
        } else {
            info.type = core::TerrainType::Ground;
            float moisture = (m_moistureNoise.GetNoise(static_cast<float>(x), static_cast<float>(y)) + 1.0f) * 0.5f;
            if (moisture < 0.30f)      info.biome = BiomeType::Plains;
            else if (moisture < 0.50f) info.biome = BiomeType::Plains2;
            else if (moisture < 0.70f) info.biome = BiomeType::Bush;
            else                       info.biome = BiomeType::SpineForest;

            if (info.elevationLevel == 1 || info.elevationLevel == 3 || info.elevationLevel == 5) {
                if (info.elevationLevel == 1) m_debugLevel1Tiles.fetch_add(1, std::memory_order_relaxed);

                uint8_t targetL = info.elevationLevel + 2;

                bool cN = getTopologicalLevel(x, y - 1) >= targetL;
                bool cS = getTopologicalLevel(x, y + 1) >= targetL;
                bool cW = getTopologicalLevel(x - 1, y) >= targetL;
                bool cE = getTopologicalLevel(x + 1, y) >= targetL;

                bool isCornerCliff = (cN && cE && !cS && !cW) ||
                                     (cN && cW && !cS && !cE) ||
                                     (cS && cE && !cN && !cW) ||
                                     (cS && cW && !cN && !cE);

                if (isCornerCliff) {
                    if (info.elevationLevel == 1) m_debugNearCliffTiles.fetch_add(1, std::memory_order_relaxed);
                    float rNoise = m_rampNoise.GetNoise(static_cast<float>(x), static_cast<float>(y));
                    if (rNoise > 0.0001f) {
                        info.elevationLevel += 1;
                        info.isRamp = true;
                        m_rampCounter.fetch_add(1, std::memory_order_relaxed);
                    }
                }
            }
        }
        return info;
    }

}