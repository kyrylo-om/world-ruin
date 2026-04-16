#pragma once

#include "Core/Types.hpp"
#include "../Generation/WorldGenerator.hpp"
#include <SFML/Graphics/Image.hpp>
#include <SFML/Graphics/Color.hpp>
#include <memory>
#include <vector>

namespace wr::world {

    class ChunkLODMesher {
    public:

        [[nodiscard]] static std::unique_ptr<sf::Image> generateLODImage(const std::vector<TileInfo>& chunkTiles);

    private:

        [[nodiscard]] static sf::Color getBiomeColor(BiomeType biome, uint8_t level, bool isRiver);
    };

}