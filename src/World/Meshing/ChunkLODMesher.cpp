#include "../../../include/World/Meshing/ChunkLODMesher.hpp"
#include <algorithm>

namespace wr::world {

    sf::Color ChunkLODMesher::getBiomeColor(BiomeType biome, uint8_t level, bool isRiver) {
        sf::Color pixelColor;

        if (isRiver) {
            return sf::Color(107, 185, 240);
        }

        switch (biome) {
            case BiomeType::Plains:      pixelColor = sf::Color(172, 199, 84); break;
            case BiomeType::Plains2:     pixelColor = sf::Color(98, 167, 60);  break;
            case BiomeType::Bush:        pixelColor = sf::Color(66, 132, 49);  break;
            case BiomeType::SpineForest: pixelColor = sf::Color(41, 96, 45);   break;
            case BiomeType::Ocean:       return sf::Color(45, 120, 215);
            default:                     return sf::Color(100, 100, 100);
        }

        if (level == 5) {
            pixelColor.r = std::min(255, pixelColor.r + 15);
            pixelColor.g = std::min(255, pixelColor.g + 15);
            pixelColor.b = std::min(255, pixelColor.b + 15);
        } else if (level == 7) {
            pixelColor.r = std::min(255, pixelColor.r + 30);
            pixelColor.g = std::min(255, pixelColor.g + 30);
            pixelColor.b = std::min(255, pixelColor.b + 30);
        }

        return pixelColor;
    }

    std::unique_ptr<sf::Image> ChunkLODMesher::generateLODImage(const std::vector<TileInfo>& chunkTiles) {
        auto image = std::make_unique<sf::Image>();
        image->create(core::CHUNK_SIZE, core::CHUNK_SIZE);

        for (int64_t ly = 0; ly < core::CHUNK_SIZE; ++ly) {
            for (int64_t lx = 0; lx < core::CHUNK_SIZE; ++lx) {
                const auto& info = chunkTiles[ly * core::CHUNK_SIZE + lx];
                image->setPixel(lx, ly, getBiomeColor(info.biome, info.elevationLevel, info.isRiver));
            }
        }
        return image;
    }

}