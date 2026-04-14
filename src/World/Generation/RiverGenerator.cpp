#include "../../../include/World/Generation/RiverGenerator.hpp"
#include <cmath>

namespace wr::world {

    RiverGenerator::RiverGenerator(uint32_t seed) : m_seed(seed) {
        m_warpNoiseX.SetSeed(seed + 50);
        m_warpNoiseX.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
        m_warpNoiseX.SetFrequency(0.002f);

        m_warpNoiseY.SetSeed(seed + 51);
        m_warpNoiseY.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
        m_warpNoiseY.SetFrequency(0.002f);

        m_riverNoise.SetSeed(seed + 52);
        m_riverNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
        m_riverNoise.SetFrequency(0.005f);
    }

    bool RiverGenerator::isRiver(int64_t x, int64_t y) const noexcept {
        float fx = static_cast<float>(x);
        float fy = static_cast<float>(y);

        float offsetX = m_warpNoiseX.GetNoise(fx, fy) * 30.0f;
        float offsetY = m_warpNoiseY.GetNoise(fx, fy) * 30.0f;
        float riverVal = std::abs(m_riverNoise.GetNoise(fx + offsetX, fy + offsetY));

        return riverVal < 0.035f;
    }

    void RiverGenerator::generateRiverGrid(bool* outGrid, int64_t startX, int64_t startY, int width, int height) const noexcept {

        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                float fx = static_cast<float>(startX + x);
                float fy = static_cast<float>(startY + y);

                float offsetX = m_warpNoiseX.GetNoise(fx, fy) * 30.0f;
                float offsetY = m_warpNoiseY.GetNoise(fx, fy) * 30.0f;
                float riverVal = std::abs(m_riverNoise.GetNoise(fx + offsetX, fy + offsetY));

                outGrid[y * width + x] = (riverVal < 0.035f);
            }
        }
    }

}