#include "../../../include/World/Generation/WorldGenerator.hpp"

namespace wr::world {

    float WorldGenerator::getRawElevationNoise(core::GlobalCoord x, core::GlobalCoord y) const noexcept {
        float raw = (m_elevationNoise.GetNoise(static_cast<float>(x), static_cast<float>(y)) + 1.0f) * 0.5f;

        if (raw < 0.35f) {
            return raw;
        }

        if (m_riverGen->isRiver(x, y)) {
            return -1.0f;
        }

        return raw;
    }

    uint8_t WorldGenerator::getTopologicalLevel(core::GlobalCoord x, core::GlobalCoord y) const noexcept {
        const float tier1Threshold = 0.60f;
        const float tier2Threshold = 0.75f;
        const float tier3Threshold = 0.85f;

        float raw = getRawElevationNoise(x, y);

        if (raw < tier1Threshold) return 1;

        if (raw >= tier3Threshold) {
            bool tooClose = false;
            for (int dy = -2; dy <= 2; ++dy) {
                for (int dx = -2; dx <= 2; ++dx) {
                    if (getRawElevationNoise(x + dx, y + dy) < tier2Threshold) { tooClose = true; break; }
                }
                if (tooClose) break;
            }
            if (!tooClose) return 7;
        }

        if (raw >= tier2Threshold) {
            bool tooClose = false;
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    if (getRawElevationNoise(x + dx, y + dy) < tier1Threshold) { tooClose = true; break; }
                }
                if (tooClose) break;
            }
            if (!tooClose) return 5;
        }

        return 3;
    }

}