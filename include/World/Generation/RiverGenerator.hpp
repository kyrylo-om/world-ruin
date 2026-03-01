#pragma once

#include <FastNoiseLite.h>
#include <cstdint>

namespace wr::world {

    class RiverGenerator {
    public:
        explicit RiverGenerator(uint32_t seed);

        [[nodiscard]] bool isRiver(int64_t x, int64_t y) const noexcept;

        void generateRiverGrid(bool* outGrid, int64_t startX, int64_t startY, int width, int height) const noexcept;

    private:
        uint32_t m_seed;
        mutable FastNoiseLite m_warpNoiseX;
        mutable FastNoiseLite m_warpNoiseY;
        mutable FastNoiseLite m_riverNoise;
    };

}