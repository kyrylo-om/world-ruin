#include "../../../include/World/Generation/WorldGenerator.hpp"
#include "World/Biomes.hpp"
#include "../../../include/Config/WorldConfig.hpp"
#include <array>

namespace wr::world {

    void WorldGenerator::generateChunkData(std::vector<TileInfo>& outTiles, core::GlobalCoord startX, core::GlobalCoord startY) const {
        constexpr int P_NOISE = 3;
        constexpr int SZ_NOISE = core::CHUNK_SIZE + P_NOISE * 2;

        constexpr int P_LEVEL = 1;
        constexpr int SZ_LEVEL = core::CHUNK_SIZE + P_LEVEL * 2;

        std::array<bool, SZ_NOISE * SZ_NOISE> riverGrid;
        std::array<float, SZ_NOISE * SZ_NOISE> noiseGrid;
        std::array<uint8_t, SZ_LEVEL * SZ_LEVEL> levelGrid;

        std::array<float, core::CHUNK_SIZE * core::CHUNK_SIZE> moistGrid;
        std::array<float, core::CHUNK_SIZE * core::CHUNK_SIZE> rampRngGrid;

        core::GlobalCoord nStartX = startX - P_NOISE;
        core::GlobalCoord nStartY = startY - P_NOISE;

        m_riverGen->generateRiverGrid(riverGrid.data(), nStartX, nStartY, SZ_NOISE, SZ_NOISE);

        for (int y = 0; y < SZ_NOISE; ++y) {
            for (int x = 0; x < SZ_NOISE; ++x) {
                int idx = y * SZ_NOISE + x;
                float raw = (m_elevationNoise.GetNoise(static_cast<float>(nStartX + x), static_cast<float>(nStartY + y)) + 1.0f) * 0.5f;

                if (raw < config::WATER_THRESHOLD) {
                    noiseGrid[idx] = raw;
                } else if (riverGrid[idx]) {
                    noiseGrid[idx] = config::RIVER_OVERRIDE;
                } else {
                    noiseGrid[idx] = raw;
                }
            }
        }

        auto getNoise = [&](int nx, int ny) -> float { return noiseGrid[ny * SZ_NOISE + nx]; };

        for (int y = 0; y < SZ_LEVEL; ++y) {
            for (int x = 0; x < SZ_LEVEL; ++x) {
                int nx = x + (P_NOISE - P_LEVEL);
                int ny = y + (P_NOISE - P_LEVEL);

                float raw = getNoise(nx, ny);
                uint8_t lvl = 3;

                if (raw < config::ELEVATION_T1) {
                    lvl = 1;
                } else if (raw >= config::ELEVATION_T3) {
                    bool tooClose = false;
                    for (int dy = -2; dy <= 2; ++dy) {
                        for (int dx = -2; dx <= 2; ++dx) {
                            if (getNoise(nx + dx, ny + dy) < config::ELEVATION_T2) { tooClose = true; break; }
                        }
                        if (tooClose) break;
                    }
                    if (!tooClose) lvl = 7;
                }
                if (lvl == 3 && raw >= config::ELEVATION_T2) {
                    bool tooClose = false;
                    for (int dy = -1; dy <= 1; ++dy) {
                        for (int dx = -1; dx <= 1; ++dx) {
                            if (getNoise(nx + dx, ny + dy) < config::ELEVATION_T1) { tooClose = true; break; }
                        }
                        if (tooClose) break;
                    }
                    if (!tooClose) lvl = 5;
                }
                levelGrid[y * SZ_LEVEL + x] = lvl;
            }
        }

        auto getLvl = [&](int lx, int ly) -> uint8_t { return levelGrid[ly * SZ_LEVEL + lx]; };

        for (int y = 0; y < core::CHUNK_SIZE; ++y) {
            for (int x = 0; x < core::CHUNK_SIZE; ++x) {
                float fx = static_cast<float>(startX + x);
                float fy = static_cast<float>(startY + y);
                int idx = y * core::CHUNK_SIZE + x;
                moistGrid[idx] = (m_moistureNoise.GetNoise(fx, fy) + 1.0f) * 0.5f;
                rampRngGrid[idx] = m_rampNoise.GetNoise(fx, fy);
            }
        }

        uint64_t localLvl1 = 0, localNearCliff = 0, localRamps = 0;

        for (int y = 0; y < core::CHUNK_SIZE; ++y) {
            for (int x = 0; x < core::CHUNK_SIZE; ++x) {
                int nx = x + P_NOISE;
                int ny = y + P_NOISE;
                int lx = x + P_LEVEL;
                int ly = y + P_LEVEL;
                int idx = y * core::CHUNK_SIZE + x;

                TileInfo info;
                info.rawElevation = getNoise(nx, ny);
                info.isRiver = riverGrid[ny * SZ_NOISE + nx];
                info.isRamp = false;
                info.elevationLevel = getLvl(lx, ly);

                if (info.rawElevation < config::WATER_THRESHOLD) {
                    info.type = core::TerrainType::Water;
                    info.biome = (info.rawElevation < 0.0f) ? BiomeType::River : BiomeType::Ocean;
                    info.elevationLevel = 1;
                } else {
                    info.type = core::TerrainType::Ground;
                    float moisture = moistGrid[idx];

                    if (moisture < config::MOISTURE_PLAINS)       info.biome = BiomeType::Plains;
                    else if (moisture < config::MOISTURE_PLAINS2) info.biome = BiomeType::Plains2;
                    else if (moisture < config::MOISTURE_BUSH)    info.biome = BiomeType::Bush;
                    else                                          info.biome = BiomeType::SpineForest;

                    if (info.elevationLevel == 1 || info.elevationLevel == 3 || info.elevationLevel == 5) {
                        if (info.elevationLevel == 1) localLvl1++;
                        uint8_t targetL = info.elevationLevel + 2;

                        bool cN = getLvl(lx, ly - 1) >= targetL;
                        bool cS = getLvl(lx, ly + 1) >= targetL;
                        bool cW = getLvl(lx - 1, ly) >= targetL;
                        bool cE = getLvl(lx + 1, ly) >= targetL;

                        bool isCornerCliff = (cN && cE && !cS && !cW) ||
                                             (cN && cW && !cS && !cE) ||
                                             (cS && cE && !cN && !cW) ||
                                             (cS && cW && !cN && !cE);

                        if (isCornerCliff) {
                            if (info.elevationLevel == 1) localNearCliff++;
                            if (rampRngGrid[idx] > config::RAMP_THRESHOLD) {
                                info.elevationLevel += 1;
                                info.isRamp = true;
                                localRamps++;
                            }
                        }
                    }
                }
                outTiles.push_back(info);
            }
        }

        m_debugTotalTiles.fetch_add(core::CHUNK_SIZE * core::CHUNK_SIZE, std::memory_order_relaxed);
        m_debugLevel1Tiles.fetch_add(localLvl1, std::memory_order_relaxed);
        m_debugNearCliffTiles.fetch_add(localNearCliff, std::memory_order_relaxed);
        m_rampCounter.fetch_add(localRamps, std::memory_order_relaxed);
    }

    void WorldGenerator::generateEntities(std::vector<EntitySpawnData>& outEntities, core::GlobalCoord startX, core::GlobalCoord startY, const std::vector<TileInfo>& chunkTiles) const {
        auto hash = [](int64_t x, int64_t y, uint32_t seed) -> float {
            uint64_t h = (static_cast<uint64_t>(x) * 73856093) ^ (static_cast<uint64_t>(y) * 19349663) ^ seed;
            h = (h ^ (h >> 33)) * 0xff51afd7ed558ccd;
            h = (h ^ (h >> 33)) * 0xc4ceb9fe1a85ec53;
            h = h ^ (h >> 33);
            return static_cast<float>(h) / static_cast<float>(0xFFFFFFFFFFFFFFFFull);
        };

        for (int64_t ly = 0; ly < core::CHUNK_SIZE; ++ly) {
            for (int64_t lx = 0; lx < core::CHUNK_SIZE; ++lx) {
                const auto& info = chunkTiles[ly * core::CHUNK_SIZE + lx];

                if (info.type != core::TerrainType::Ground || info.isRamp) continue;

                core::GlobalCoord gx = startX + lx;
                core::GlobalCoord gy = startY + ly;

                float offsetX = config::ENTITY_OFFSET_BASE + hash(gx, gy, m_seed + config::SEED_OFFSET_X) * config::ENTITY_OFFSET_VAR;
                float offsetY = config::ENTITY_OFFSET_BASE + hash(gx, gy, m_seed + config::SEED_OFFSET_Y) * config::ENTITY_OFFSET_VAR;

                float rockRoll = hash(gx, gy, m_seed + config::SEED_ROCK_ROLL);
                bool nearCliff = false;
                for (int dy = -3; dy <= 3 && !nearCliff; ++dy) {
                    for (int dx = -3; dx <= 3 && !nearCliff; ++dx) {
                        if (dx * dx + dy * dy <= config::ROCK_CLIFF_CHECK_RADIUS_SQ) {
                            int64_t nx = lx + dx; int64_t ny = ly + dy;
                            uint8_t nLvl;
                            if (nx >= 0 && nx < core::CHUNK_SIZE && ny >= 0 && ny < core::CHUNK_SIZE) {
                                nLvl = chunkTiles[ny * core::CHUNK_SIZE + nx].elevationLevel;
                            } else {
                                nLvl = getTopologicalLevel(gx + dx, gy + dy);
                            }
                            if (nLvl >= info.elevationLevel + 2) nearCliff = true;
                        }
                    }
                }

                float rockChance = nearCliff ? config::ROCK_CHANCE_NEAR_CLIFF : config::ROCK_CHANCE_BASE;
                if (rockRoll < rockChance) {
                    uint8_t rockType = 1 + static_cast<uint8_t>(hash(gx, gy, m_seed + config::SEED_ROCK_TYPE) * config::MAX_ROCK_TYPES);
                    if (rockType > 4) rockType = 4;
                    outEntities.push_back({lx, ly, rockType, 0, offsetX, offsetY});
                    continue;
                }

                float treeRoll = hash(gx, gy, m_seed + config::SEED_TREE_ROLL);
                float treeChance = 0.0f;

                if (info.biome == BiomeType::SpineForest)      treeChance = config::TREE_CHANCE_SPINE;
                else if (info.biome == BiomeType::Bush)        treeChance = config::TREE_CHANCE_BUSH;
                else if (info.biome == BiomeType::Plains2)     treeChance = config::TREE_CHANCE_PLAINS2;
                else if (info.biome == BiomeType::Plains)      treeChance = config::TREE_CHANCE_PLAINS;

                if (treeRoll < treeChance) {
                    float typeRoll = hash(gx, gy, m_seed + config::SEED_TREE_TYPE);
                    uint8_t treeType = 1;

                    if (info.biome == BiomeType::SpineForest) {
                        if (typeRoll < config::TREE_TYPE_SPINE_PINE_RATIO)
                            treeType = (hash(gx, gy, m_seed + config::SEED_TREE_SUBTYPE) > 0.5f) ? 1 : 2;
                        else
                            treeType = (hash(gx, gy, m_seed + config::SEED_TREE_SUBTYPE) > 0.5f) ? 3 : 4;
                    } else if (info.biome == BiomeType::Plains || info.biome == BiomeType::Plains2) {
                        if (typeRoll < config::TREE_TYPE_PLAINS_BIRCH_RATIO)
                            treeType = (hash(gx, gy, m_seed + config::SEED_TREE_SUBTYPE) > 0.5f) ? 3 : 4;
                        else
                            treeType = (hash(gx, gy, m_seed + config::SEED_TREE_SUBTYPE) > 0.5f) ? 1 : 2;
                    } else if (info.biome == BiomeType::Bush) {
                        treeType = 1 + static_cast<uint8_t>(hash(gx, gy, m_seed + config::SEED_TREE_SUBTYPE) * config::MAX_TREE_TYPES);
                        if (treeType > 4) treeType = 4;
                    }

                    outEntities.push_back({lx, ly, treeType, 1, offsetX, offsetY});
                    continue;
                }

                float bushRoll = hash(gx, gy, m_seed + config::SEED_BUSH_ROLL);
                float bushChance = 0.0f;

                if (info.biome == BiomeType::Bush)             bushChance = config::BUSH_CHANCE_BUSH;
                else if (info.biome == BiomeType::Plains2)     bushChance = config::BUSH_CHANCE_PLAINS2;
                else if (info.biome == BiomeType::SpineForest) bushChance = config::BUSH_CHANCE_SPINE;
                else if (info.biome == BiomeType::Plains)      bushChance = config::BUSH_CHANCE_PLAINS;

                if (bushRoll < bushChance) {
                    uint8_t bushType = static_cast<uint8_t>(hash(gx, gy, m_seed + config::SEED_BUSH_TYPE) * config::MAX_BUSH_TYPES);
                    if (bushType > 3) bushType = 3;

                    outEntities.push_back({lx, ly, bushType, 2, offsetX, offsetY});
                }
            }
        }
    }

}