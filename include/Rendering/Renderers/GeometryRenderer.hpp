#pragma once

#include "../RenderContext.hpp"
#include <vector>

namespace wr::rendering {

    class GeometryRenderer {
    public:
        static void renderLOD(const RenderContext& ctx, const std::vector<ChunkRenderData>& renderData);

        static void renderWater(const RenderContext& ctx, const std::vector<ChunkRenderData>& renderData);
        static void renderBaseGround(const RenderContext& ctx, const std::vector<ChunkRenderData>& renderData);

        static void renderTier1_Faces(const RenderContext& ctx, const std::vector<ChunkRenderData>& renderData);
        static void renderTier1_Tops(const RenderContext& ctx, const std::vector<ChunkRenderData>& renderData);

        static void renderTier2_Faces(const RenderContext& ctx, const std::vector<ChunkRenderData>& renderData);
        static void renderTier2_Tops(const RenderContext& ctx, const std::vector<ChunkRenderData>& renderData);

        static void renderTier3_Faces(const RenderContext& ctx, const std::vector<ChunkRenderData>& renderData);
        static void renderTier3_Tops(const RenderContext& ctx, const std::vector<ChunkRenderData>& renderData);
    };

}