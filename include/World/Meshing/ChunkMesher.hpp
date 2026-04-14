#pragma once

#include "Core/Types.hpp"
#include "../Generation/WorldGenerator.hpp"
#include "Rendering/TileHandler.hpp"
#include <vector>
#include <array>
#include <memory>
#include <SFML/Graphics/VertexArray.hpp>
#include <SFML/Graphics/Image.hpp>

namespace wr::world {

    struct MeshingContext;

    struct ChunkMeshData {
        std::array<sf::VertexArray, 4> oceanVertices;
        std::array<sf::VertexArray, 4> riverVertices;

        std::array<sf::VertexArray, 4> foamLayers;

        std::array<sf::VertexArray, 4> shadowVertices;
        std::array<sf::VertexArray, 4> shadow4Vertices;
        std::array<sf::VertexArray, 4> shadow6Vertices;

        std::array<sf::VertexArray, 4> groundLayers;
        std::array<sf::VertexArray, 4> rampLayers;
        std::array<sf::VertexArray, 4> cliffLayers;
        std::array<sf::VertexArray, 4> level3Layers;

        std::array<sf::VertexArray, 4> ramp4Layers;
        std::array<sf::VertexArray, 4> cliff4Layers;
        std::array<sf::VertexArray, 4> level5Layers;

        std::array<sf::VertexArray, 4> ramp6Layers;
        std::array<sf::VertexArray, 4> cliff6Layers;
        std::array<sf::VertexArray, 4> level7Layers;

        std::unique_ptr<sf::Image> lodImage;
    };

    class ChunkMesher {
    public:
        static ChunkMeshData generateMeshes(const std::vector<TileInfo>& chunkTiles,
                                            const WorldGenerator& generator,
                                            const rendering::TileHandler& tileHandler,
                                            core::GlobalCoord startX,
                                            core::GlobalCoord startY);
    };

}