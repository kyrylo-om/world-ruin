#pragma once

#include "Core/Types.hpp"
#include "Core/Math.hpp"
#include "World/Biomes.hpp"
#include "Meshing/ChunkMesher.hpp"
#include "Generation/WorldGenerator.hpp"
#include <SFML/Graphics/VertexBuffer.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/Graphics/Image.hpp>
#include <entt/entt.hpp>
#include <vector>
#include <atomic>
#include <array>
#include <memory>

namespace wr::world {

    enum class ChunkState : uint8_t {
        Empty,
        Generating,
        DataReady,
        Active
    };

    struct Chunk {
        math::Vec2i64 coordinate;
        std::atomic<ChunkState> state{ChunkState::Empty};

        std::vector<core::TerrainType> tiles;
        std::vector<uint8_t> levels;

        std::vector<EntitySpawnData> pendingEntities;

        std::vector<entt::entity> entities;

        static constexpr int NUM_CELLS = 16;
        std::vector<entt::entity> spatialCells[NUM_CELLS][NUM_CELLS];

        std::unique_ptr<ChunkMeshData> pendingMeshData;

        std::unique_ptr<sf::Image> pendingLodImage;
        sf::Texture lodTexture;
        sf::VertexBuffer lodBuffer;

        std::array<sf::VertexBuffer, 4> oceanVertices;
        std::array<sf::VertexBuffer, 4> riverVertices;
        std::array<sf::VertexBuffer, 4> foamLayers;

        std::array<sf::VertexBuffer, 4> shadowVertices;
        std::array<sf::VertexBuffer, 4> shadow4Vertices;
        std::array<sf::VertexBuffer, 4> shadow6Vertices;

        std::array<sf::VertexBuffer, 4> groundLayers;
        std::array<sf::VertexBuffer, 4> rampLayers;
        std::array<sf::VertexBuffer, 4> cliffLayers;
        std::array<sf::VertexBuffer, 4> level3Layers;

        std::array<sf::VertexBuffer, 4> ramp4Layers;
        std::array<sf::VertexBuffer, 4> cliff4Layers;
        std::array<sf::VertexBuffer, 4> level5Layers;

        std::array<sf::VertexBuffer, 4> ramp6Layers;
        std::array<sf::VertexBuffer, 4> cliff6Layers;
        std::array<sf::VertexBuffer, 4> level7Layers;

        explicit Chunk(math::Vec2i64 coord) : coordinate(coord) {
            tiles.resize(core::CHUNK_SIZE * core::CHUNK_SIZE, core::TerrainType::Water);
            levels.resize(core::CHUNK_SIZE * core::CHUNK_SIZE, 0);
        }

        ~Chunk() = default;

        void recycle(math::Vec2i64 newCoord) {
            coordinate = newCoord;
            state.store(ChunkState::Empty, std::memory_order_release);
            pendingEntities.clear();
            entities.clear();
            pendingMeshData.reset();
            pendingLodImage.reset();
        }

        void setTile(int64_t lx, int64_t ly, core::TerrainType type, uint8_t level) {
            size_t idx = ly * core::CHUNK_SIZE + lx;
            tiles[idx] = type;
            levels[idx] = level;
        }

        [[nodiscard]] core::TerrainType getTile(int64_t lx, int64_t ly) const {
            return tiles[ly * core::CHUNK_SIZE + lx];
        }

        [[nodiscard]] uint8_t getLevel(int64_t lx, int64_t ly) const {
            return levels[ly * core::CHUNK_SIZE + lx];
        }
    };

}