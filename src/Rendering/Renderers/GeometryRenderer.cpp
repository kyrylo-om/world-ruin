#include "Rendering/Renderers/GeometryRenderer.hpp"

namespace wr::rendering {

namespace {
    void drawLayer(const RenderContext& ctx, const std::vector<ChunkRenderData>& renderData,
                   std::array<sf::VertexBuffer, 4> world::Chunk::* bufferMember,
                   const sf::Texture& tex, sf::Shader* shader = nullptr) {
        sf::RenderStates states;
        states.texture = &tex;
        states.shader = shader;
        for (const auto& data : renderData) {
            const sf::VertexBuffer& buffer = (data.chunk->*bufferMember)[ctx.dirIdx];
            if (buffer.getVertexCount() > 0) {
                states.transform = data.transform;
                ctx.window.draw(buffer, states);
            }
        }
    }
}

void GeometryRenderer::renderLOD(const RenderContext& ctx, const std::vector<ChunkRenderData>& renderData) {
    sf::RenderStates states;
    for (const auto& data : renderData) {
        if (data.chunk->lodBuffer.getVertexCount() > 0) {
            states.transform = data.transform;
            states.texture = &data.chunk->lodTexture;
            ctx.window.draw(data.chunk->lodBuffer, states);
        }
    }
}

void GeometryRenderer::renderWater(const RenderContext& ctx, const std::vector<ChunkRenderData>& renderData) {
    drawLayer(ctx, renderData, &world::Chunk::oceanVertices, ctx.tiles.getWaterTexture());
    drawLayer(ctx, renderData, &world::Chunk::riverVertices, ctx.tiles.getRiverTexture());
    drawLayer(ctx, renderData, &world::Chunk::foamLayers, ctx.tiles.getFoamTexture(), ctx.foamShader);
}

void GeometryRenderer::renderBaseGround(const RenderContext& ctx, const std::vector<ChunkRenderData>& renderData) {
    drawLayer(ctx, renderData, &world::Chunk::groundLayers, ctx.tiles.getAtlasTexture());
}

void GeometryRenderer::renderTier1_Faces(const RenderContext& ctx, const std::vector<ChunkRenderData>& renderData) {
    drawLayer(ctx, renderData, &world::Chunk::shadowVertices, ctx.tiles.getShadowTexture());
    drawLayer(ctx, renderData, &world::Chunk::rampLayers, ctx.tiles.getAtlasTexture());
    drawLayer(ctx, renderData, &world::Chunk::cliffLayers, ctx.tiles.getAtlasTexture());
}
void GeometryRenderer::renderTier1_Tops(const RenderContext& ctx, const std::vector<ChunkRenderData>& renderData) {
    drawLayer(ctx, renderData, &world::Chunk::level3Layers, ctx.tiles.getAtlasTexture());
}

void GeometryRenderer::renderTier2_Faces(const RenderContext& ctx, const std::vector<ChunkRenderData>& renderData) {
    drawLayer(ctx, renderData, &world::Chunk::shadow4Vertices, ctx.tiles.getShadowTexture());
    drawLayer(ctx, renderData, &world::Chunk::ramp4Layers, ctx.tiles.getAtlasTexture());
    drawLayer(ctx, renderData, &world::Chunk::cliff4Layers, ctx.tiles.getAtlasTexture());
}
void GeometryRenderer::renderTier2_Tops(const RenderContext& ctx, const std::vector<ChunkRenderData>& renderData) {
    drawLayer(ctx, renderData, &world::Chunk::level5Layers, ctx.tiles.getAtlasTexture());
}

void GeometryRenderer::renderTier3_Faces(const RenderContext& ctx, const std::vector<ChunkRenderData>& renderData) {
    drawLayer(ctx, renderData, &world::Chunk::shadow6Vertices, ctx.tiles.getShadowTexture());
    drawLayer(ctx, renderData, &world::Chunk::ramp6Layers, ctx.tiles.getAtlasTexture());
    drawLayer(ctx, renderData, &world::Chunk::cliff6Layers, ctx.tiles.getAtlasTexture());
}
void GeometryRenderer::renderTier3_Tops(const RenderContext& ctx, const std::vector<ChunkRenderData>& renderData) {
    drawLayer(ctx, renderData, &world::Chunk::level7Layers, ctx.tiles.getAtlasTexture());
}

}