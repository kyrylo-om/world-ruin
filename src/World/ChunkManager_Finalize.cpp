#include "World/ChunkManager.hpp"
#include "../../include/World/Meshing/ChunkMesher.hpp"
#include "Rendering/TileHandler.hpp"
#include "ECS/Components.hpp"
#include "ECS/Tags.hpp"
#include "../../include/Config/ResourceConfig.hpp"
#include <algorithm>
#include <cmath>

namespace wr::world {

void ChunkManager::finalizeReadyChunk(const math::Vec2i64& coord, Chunk& chunk) {
    if (chunk.pendingLodImage) {
        chunk.lodTexture.loadFromImage(*chunk.pendingLodImage);
        chunk.lodTexture.setSmooth(false);

        sf::VertexArray lodQuad(sf::Quads, 4);
        float s = static_cast<float>(core::CHUNK_PIXEL_SIZE);
        float texS = static_cast<float>(core::CHUNK_SIZE);

        lodQuad[0].position = {0.0f, 0.0f}; lodQuad[0].texCoords = {0.0f, 0.0f};
        lodQuad[1].position = {s, 0.0f};    lodQuad[1].texCoords = {texS, 0.0f};
        lodQuad[2].position = {s, s};       lodQuad[2].texCoords = {texS, texS};
        lodQuad[3].position = {0.0f, s};    lodQuad[3].texCoords = {0.0f, texS};

        chunk.lodBuffer.create(4);
        chunk.lodBuffer.setPrimitiveType(sf::Quads);
        chunk.lodBuffer.setUsage(sf::VertexBuffer::Static);
        chunk.lodBuffer.update(&lodQuad[0]);

        chunk.pendingLodImage.reset();
    }

    if (chunk.pendingMeshData) {
        auto& pending = *chunk.pendingMeshData;

        auto uploadBuffer = [](const sf::VertexArray& sourceArray, sf::VertexBuffer& destBuffer) {
            if (sourceArray.getVertexCount() > 0) {
                destBuffer.create(sourceArray.getVertexCount());
                destBuffer.setPrimitiveType(sf::Quads);
                destBuffer.setUsage(sf::VertexBuffer::Static);
                destBuffer.update(&sourceArray[0]);
            } else {
                destBuffer = sf::VertexBuffer();
            }
        };

        for (int i = 0; i < 4; ++i) {
            uploadBuffer(pending.oceanVertices[i], chunk.oceanVertices[i]);
            uploadBuffer(pending.riverVertices[i], chunk.riverVertices[i]);
            uploadBuffer(pending.foamLayers[i], chunk.foamLayers[i]);

            uploadBuffer(pending.shadowVertices[i], chunk.shadowVertices[i]);
            uploadBuffer(pending.shadow4Vertices[i], chunk.shadow4Vertices[i]);
            uploadBuffer(pending.shadow6Vertices[i], chunk.shadow6Vertices[i]);

            uploadBuffer(pending.groundLayers[i], chunk.groundLayers[i]);
            uploadBuffer(pending.rampLayers[i], chunk.rampLayers[i]);
            uploadBuffer(pending.cliffLayers[i], chunk.cliffLayers[i]);
            uploadBuffer(pending.level3Layers[i], chunk.level3Layers[i]);

            uploadBuffer(pending.ramp4Layers[i], chunk.ramp4Layers[i]);
            uploadBuffer(pending.cliff4Layers[i], chunk.cliff4Layers[i]);
            uploadBuffer(pending.level5Layers[i], chunk.level5Layers[i]);

            uploadBuffer(pending.ramp6Layers[i], chunk.ramp6Layers[i]);
            uploadBuffer(pending.cliff6Layers[i], chunk.cliff6Layers[i]);
            uploadBuffer(pending.level7Layers[i], chunk.level7Layers[i]);
        }
        chunk.pendingMeshData.reset();
    }
}

void ChunkManager::spawnEntityForChunk(const math::Vec2i64& coord, Chunk& chunk, const EntitySpawnData& data) {
    float baseHitboxRadius = 19.2f;

    entt::entity entity = m_registry.create();
    chunk.entities.push_back(entity);

    int cellTiles = core::CHUNK_SIZE / 16;
    int cellX = std::clamp(static_cast<int>(data.lx / cellTiles), 0, 15);
    int cellY = std::clamp(static_cast<int>(data.ly / cellTiles), 0, 15);
    chunk.spatialCells[cellX][cellY].push_back(entity);

    float worldX = static_cast<float>(coord.x * core::CHUNK_SIZE + data.lx) + data.offsetX;
    float worldY = static_cast<float>(coord.y * core::CHUNK_SIZE + data.ly) + data.offsetY;

    uint8_t level = chunk.getLevel(data.lx, data.ly);
    float zHeight = 0.0f;
    if (level == 2) zHeight = 32.0f;
    else if (level == 3) zHeight = 64.0f;
    else if (level == 4) zHeight = 96.0f;
    else if (level == 5) zHeight = 128.0f;
    else if (level == 6) zHeight = 160.0f;
    else if (level == 7) zHeight = 192.0f;

    m_registry.emplace<ecs::LogicalPos>(entity, coord.x * core::CHUNK_SIZE + data.lx, coord.y * core::CHUNK_SIZE + data.ly);
    m_registry.emplace<ecs::WorldPos>(entity, worldX, worldY, zHeight, zHeight);
    m_registry.emplace<ecs::ScreenPos>(entity, worldX, worldY, 255.0f, 255.0f, static_cast<uint8_t>(0));

    auto& spr = m_registry.emplace<ecs::SpriteComponent>(entity).sprite;
    m_registry.emplace<ecs::ResourceTag>(entity);

    if (data.entityClass == 0) {

        spr.setTexture(m_tileHandler.getResourcesAtlas());

        float texWidth = 64.0f;
        float texHeight = 64.0f;
        float tx = 0.0f;
        float ty = 1408.0f + static_cast<float>((data.assetType - 1) * 64);

        spr.setTextureRect(sf::IntRect(0, static_cast<int>(ty), static_cast<int>(texWidth), static_cast<int>(texHeight)));
        spr.setOrigin(texWidth / 2.0f, texHeight / 2.0f);
        spr.setRotation(0.0f);

        float ox = texWidth / 2.0f;
        float oy = texHeight / 2.0f;

        ecs::CachedQuad quad;
        quad.texture = spr.getTexture();
        quad.localVertices[0] = sf::Vertex({-ox, -oy}, sf::Color::White, {tx, ty});
        quad.localVertices[1] = sf::Vertex({texWidth - ox, -oy}, sf::Color::White, {tx + texWidth, ty});
        quad.localVertices[2] = sf::Vertex({texWidth - ox, texHeight - oy}, sf::Color::White, {tx + texWidth, ty + texHeight});
        quad.localVertices[3] = sf::Vertex({-ox, texHeight - oy}, sf::Color::White, {tx, ty + texHeight});
        m_registry.emplace<ecs::CachedQuad>(entity, quad);

        if (data.assetType == 1 || data.assetType == 3) {
            m_registry.emplace<ecs::SmallRockTag>(entity);
            m_registry.emplace<ecs::WalkableTag>(entity);
            m_registry.emplace<ecs::ResourceData>(entity, data.assetType, false, 0.0f);
            m_registry.emplace<ecs::Health>(entity, 1, 1);
            m_registry.emplace<ecs::Hitbox>(entity, baseHitboxRadius);
            m_registry.emplace<ecs::ClickArea>(entity, 32.0f);
        } else {
            m_registry.emplace<ecs::RockTag>(entity);
            m_registry.emplace<ecs::SolidTag>(entity);
            m_registry.emplace<ecs::ResourceData>(entity, data.assetType, false, 0.0f);
            int hits = (data.assetType == 2) ? config::ROCK_HITS_TYPE_2 : config::ROCK_HITS_TYPE_4;
            m_registry.emplace<ecs::Health>(entity, hits, hits);
            m_registry.emplace<ecs::Hitbox>(entity, baseHitboxRadius);
            m_registry.emplace<ecs::ClickArea>(entity, baseHitboxRadius);
        }
    } else if (data.entityClass == 1) {

        spr.setTexture(m_tileHandler.getResourcesAtlas());

        float texWidth = 192.0f;
        float texHeight = (data.assetType == 1 || data.assetType == 2) ? 256.0f : 192.0f;

        float ty = 0.0f;
        if (data.assetType == 1) ty = 0.0f;
        else if (data.assetType == 2) ty = 256.0f;
        else if (data.assetType == 3) ty = 512.0f;
        else if (data.assetType == 4) ty = 704.0f;

        float tx = 0.0f;

        spr.setTextureRect(sf::IntRect(0, static_cast<int>(ty), static_cast<int>(texWidth), static_cast<int>(texHeight)));
        spr.setOrigin(texWidth / 2.0f, texHeight - 32.0f);
        spr.setRotation(0.0f);

        float ox = texWidth / 2.0f;
        float oy = texHeight - 32.0f;

        ecs::CachedQuad quad;
        quad.texture = spr.getTexture();
        quad.localVertices[0] = sf::Vertex({-ox, -oy}, sf::Color::White, {tx, ty});
        quad.localVertices[1] = sf::Vertex({texWidth - ox, -oy}, sf::Color::White, {tx + texWidth, ty});
        quad.localVertices[2] = sf::Vertex({texWidth - ox, texHeight - oy}, sf::Color::White, {tx + texWidth, ty + texHeight});
        quad.localVertices[3] = sf::Vertex({-ox, texHeight - oy}, sf::Color::White, {tx, ty + texHeight});
        m_registry.emplace<ecs::CachedQuad>(entity, quad);

        float c_th = texHeight - 64.0f;
        float c_height = texHeight - 64.0f;

        ecs::CachedCanopyQuad canopyQuad;
        canopyQuad.texture = spr.getTexture();
        canopyQuad.localVertices[0] = sf::Vertex({-ox, -oy}, sf::Color::White, {tx, ty});
        canopyQuad.localVertices[1] = sf::Vertex({texWidth - ox, -oy}, sf::Color::White, {tx + texWidth, ty});
        canopyQuad.localVertices[2] = sf::Vertex({texWidth - ox, c_height - oy}, sf::Color::White, {tx + texWidth, ty + c_th});
        canopyQuad.localVertices[3] = sf::Vertex({-ox, c_height - oy}, sf::Color::White, {tx, ty + c_th});
        m_registry.emplace<ecs::CachedCanopyQuad>(entity, canopyQuad);

        m_registry.emplace<ecs::TreeTag>(entity);
        m_registry.emplace<ecs::SolidTag>(entity);
        m_registry.emplace<ecs::ResourceData>(entity, data.assetType, false, 0.0f);

        float hitboxScale = 1.0f;
        if (data.assetType == 1 || data.assetType == 2) hitboxScale = 0.85f;
        else if (data.assetType == 3 || data.assetType == 4) hitboxScale = 0.70f;

        float finalHitbox = baseHitboxRadius * hitboxScale;
        m_registry.emplace<ecs::Hitbox>(entity, finalHitbox);
        m_registry.emplace<ecs::ClickArea>(entity, finalHitbox * 1.15f);

        int health = config::TREE_HITS_TYPE_4;
        if (data.assetType == 1) health = config::TREE_HITS_TYPE_1;
        else if (data.assetType == 2) health = config::TREE_HITS_TYPE_2;
        else if (data.assetType == 3) health = config::TREE_HITS_TYPE_3;
        m_registry.emplace<ecs::Health>(entity, health, health);

    } else if (data.entityClass == 2) {

        spr.setTexture(m_tileHandler.getResourcesAtlas());

        float texWidth = 128.0f;
        float texHeight = 128.0f;
        float tx = 0.0f;
        float ty = 896.0f + static_cast<float>(data.assetType * 128);

        spr.setTextureRect(sf::IntRect(0, static_cast<int>(ty), static_cast<int>(texWidth), static_cast<int>(texHeight)));

        float ox = texWidth / 2.0f;
        float oy = 76.0f;

        spr.setOrigin(ox, oy);
        spr.setRotation(0.0f);

        ecs::CachedQuad quad;
        quad.texture = spr.getTexture();
        quad.localVertices[0] = sf::Vertex({-ox, -oy}, sf::Color::White, {tx, ty});
        quad.localVertices[1] = sf::Vertex({texWidth - ox, -oy}, sf::Color::White, {tx + texWidth, ty});
        quad.localVertices[2] = sf::Vertex({texWidth - ox, texHeight - oy}, sf::Color::White, {tx + texWidth, ty + texHeight});
        quad.localVertices[3] = sf::Vertex({-ox, texHeight - oy}, sf::Color::White, {tx, ty + texHeight});
        m_registry.emplace<ecs::CachedQuad>(entity, quad);

        m_registry.emplace<ecs::BushTag>(entity);
        m_registry.emplace<ecs::ResourceData>(entity, data.assetType, false, 0.0f);

        float finalClickArea = baseHitboxRadius * 0.85f * 1.15f;
        m_registry.emplace<ecs::ClickArea>(entity, finalClickArea);

        m_registry.emplace<ecs::Health>(entity, config::BUSH_HITS, config::BUSH_HITS);
    }
}

}