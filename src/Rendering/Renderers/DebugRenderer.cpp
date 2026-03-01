#include "Rendering/Renderers/DebugRenderer.hpp"
#include "Core/Types.hpp"

namespace wr::rendering {

void DebugRenderer::render(const RenderContext& ctx, const std::vector<ChunkRenderData>& renderData, sf::Text& debugText) {
    int dxUp = 0, dyUp = 0;
    switch (ctx.dir) {
        case ViewDirection::North: dxUp = 0;  dyUp = -1; break;
        case ViewDirection::East:  dxUp = -1; dyUp = 0;  break;
        case ViewDirection::South: dxUp = 0;  dyUp = 1;  break;
        case ViewDirection::West:  dxUp = 1;  dyUp = 0;  break;
    }

    for (const auto& data : renderData) {
        for (int64_t ly = 0; ly < core::CHUNK_SIZE; ++ly) {
            for (int64_t lx = 0; lx < core::CHUNK_SIZE; ++lx) {
                uint8_t level = data.chunk->getLevel(lx, ly);
                sf::Vector2f localPos(lx * 64.0f + 32.0f, ly * 64.0f + 32.0f);

                if (level == 1 || level == 3 || level == 5) {
                    int64_t nX = lx - dxUp;
                    int64_t nY = ly - dyUp;

                    if (nX >= 0 && nX < core::CHUNK_SIZE && nY >= 0 && nY < core::CHUNK_SIZE) {
                        uint8_t neighborLevel = data.chunk->getLevel(nX, nY);
                        if (neighborLevel == level + 1 || neighborLevel == level + 2) continue;
                    }
                }

                if (level == 2 || level == 3) {
                    localPos += (sf::Vector2f(dxUp, dyUp) * 64.0f);
                } else if (level == 4 || level == 5) {
                    localPos += (sf::Vector2f(dxUp, dyUp) * 128.0f);
                } else if (level == 6 || level == 7) {
                    localPos += (sf::Vector2f(dxUp, dyUp) * 192.0f);
                }

                sf::Vector2f worldPos = data.transform.transformPoint(localPos);

                if (ctx.viewBounds.contains(worldPos)) {
                    debugText.setString(std::to_string(level));
                    sf::FloatRect textRect = debugText.getLocalBounds();
                    debugText.setOrigin(textRect.left + textRect.width / 2.0f,
                                        textRect.top + textRect.height / 2.0f);
                    debugText.setPosition(worldPos);
                    debugText.setRotation(0.0f);
                    ctx.window.draw(debugText);
                }
            }
        }
    }
}

}