#pragma once

#include "../RenderContext.hpp"
#include <SFML/Graphics/Text.hpp>

namespace wr::rendering {

    class DebugRenderer {
    public:
        static void render(const RenderContext& ctx, const std::vector<ChunkRenderData>& renderData, sf::Text& debugText);
    };

}