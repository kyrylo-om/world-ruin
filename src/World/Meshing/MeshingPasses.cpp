#include "World/Meshing/MeshingPasses.hpp"
#include <algorithm>

namespace wr::world::passes {

void processFoundations(ChunkMeshData& data, const MeshingContext& ctx, int64_t lx, int64_t ly, const TileInfo& info) {
    bool isBaseG = ((info.elevationLevel == 1 || info.elevationLevel == 2) && info.type == core::TerrainType::Ground);
    bool isCliffU = ((info.elevationLevel >= 3) && !getCliffStyle(lx, ly, ctx));
    bool isWaterCliff = ((info.elevationLevel >= 3) && getCliffStyle(lx, ly, ctx));

    bool drawWater = false;

    if (info.type == core::TerrainType::Water && info.elevationLevel == 1) {
        drawWater = true;
    } else if (isWaterCliff || isBaseG) {

        int card[4][2] = {{0,-1}, {0,1}, {-1,0}, {1,0}};
        for (auto& o : card) {
            if (ctx.getInfo(lx+o[0], ly+o[1]).type == core::TerrainType::Water) {
                drawWater = true;
                break;
            }
        }
    }

    if (drawWater) {
        sf::IntRect wRect(0, 0, 64, 64);
        if (info.biome == BiomeType::River) addQuad(data.riverVertices[ctx.dirIdx], lx*64.f, ly*64.f, wRect, ctx, BiomeType::Ocean);
        else addQuad(data.oceanVertices[ctx.dirIdx], lx*64.f, ly*64.f, wRect, ctx, BiomeType::Ocean);
    }

    if (isBaseG || isCliffU) {
        uint8_t mask = 0;

        auto isS = [&](int dx, int dy) {
            TileInfo n = ctx.getInfo(lx+dx, ly+dy);
            if (n.type != core::TerrainType::Ground) return false;

            if (n.elevationLevel >= 3) {

                return !getCliffStyle(lx+dx, ly+dy, ctx);
            }
            return n.elevationLevel >= 1;
        };

        if (isS(ctx.dxUp, ctx.dyUp)) mask |= rendering::North;
        if (isS(ctx.dxRight, ctx.dyRight)) mask |= rendering::East;
        if (isS(ctx.dxLeft, ctx.dyLeft)) mask |= rendering::West;
        if (isS(ctx.dxDown, ctx.dyDown)) mask |= rendering::South;

        if (ctx.currentGenDir == rendering::ViewDirection::East || ctx.currentGenDir == rendering::ViewDirection::West) {
            uint8_t flp = mask & ~(rendering::East | rendering::West);
            if (mask & rendering::East) flp |= rendering::West;
            if (mask & rendering::West) flp |= rendering::East;
            mask = flp;
        }

        sf::VertexArray& va = data.groundLayers[ctx.dirIdx];

        addQuad(va, lx*64.f, ly*64.f, ctx.grassOverWater.getByMask(mask), ctx, info.biome, 64.f, false, true, false);
    }
}

void processWaterFoam(ChunkMeshData& data, const MeshingContext& ctx, int64_t lx, int64_t ly, const TileInfo& info) {
    if (info.type == core::TerrainType::Ground) {
        bool hasVisibleWater = false;
        if (ctx.getInfo(lx + ctx.dxDown, ly + ctx.dyDown).type == core::TerrainType::Water) hasVisibleWater = true;
        else if (ctx.getInfo(lx + ctx.dxLeft, ly + ctx.dyLeft).type == core::TerrainType::Water) hasVisibleWater = true;
        else if (ctx.getInfo(lx + ctx.dxRight, ly + ctx.dyRight).type == core::TerrainType::Water) hasVisibleWater = true;

        if (hasVisibleWater) {
            sf::IntRect fRect(0, 0, 192, 192);
            float sx = lx * 64.0f;
            float sy = ly * 64.0f;
            addQuad(data.foamLayers[ctx.dirIdx], sx, sy, fRect, ctx, BiomeType::Ocean, 192.f, false, true, false);
        }
    }
}

void processCliffsAndShadows(ChunkMeshData& data, const MeshingContext& ctx, int64_t lx, int64_t ly, const TileInfo& info) {
    if (info.elevationLevel >= 3) {
        uint8_t pL = info.elevationLevel;
        if (pL == 4) pL = 3;
        if (pL == 6) pL = 5;

        float shift = (pL == 7) ? 128.0f : (pL == 5 ? 64.0f : 0.0f);
        float sx = lx * 64.0f + (ctx.dxUp * shift);
        float sy = ly * 64.0f + (ctx.dyUp * shift);
        sf::IntRect sRect(0, 0, 192, 192);

        auto isLowerForShadow = [&](int nx, int ny) {
            uint8_t nL = ctx.getInfo(lx+nx, ly+ny).elevationLevel;
            return nL < pL && !(nL == pL - 1);
        };

        sf::VertexArray* vaShad = (pL == 7) ? &data.shadow6Vertices[ctx.dirIdx] : (pL == 5 ? &data.shadow4Vertices[ctx.dirIdx] : &data.shadowVertices[ctx.dirIdx]);
        if (isLowerForShadow(ctx.dxDown, ctx.dyDown) && isShadowVisible(ctx.currentGenDir, ctx.dxDown, ctx.dyDown)) addQuad(*vaShad, sx, sy, sRect, ctx, BiomeType::Ocean, 192.f, false, false, true);
        else if (isLowerForShadow(ctx.dxLeft, ctx.dyLeft) && isShadowVisible(ctx.currentGenDir, ctx.dxLeft, ctx.dyLeft)) addQuad(*vaShad, sx, sy, sRect, ctx, BiomeType::Ocean, 192.f, false, false, true);
        else if (isLowerForShadow(ctx.dxRight, ctx.dyRight) && isShadowVisible(ctx.currentGenDir, ctx.dxRight, ctx.dyRight)) addQuad(*vaShad, sx, sy, sRect, ctx, BiomeType::Ocean, 192.f, false, false, true);

        auto isLowerForWall = [&](int nx, int ny) {
            uint8_t nL = ctx.getInfo(lx+nx, ly+ny).elevationLevel;
            return nL < pL;
        };

        if (isLowerForWall(ctx.dxDown, ctx.dyDown)) {
            TileInfo infoL = ctx.getInfo(lx+ctx.dxLeft, ly+ctx.dyLeft);
            TileInfo infoR = ctx.getInfo(lx+ctx.dxRight, ly+ctx.dyRight);

            bool hL = infoL.elevationLevel >= pL || infoL.elevationLevel == pL - 1;
            bool hR = infoR.elevationLevel >= pL || infoR.elevationLevel == pL - 1;

            bool rot = (ctx.currentGenDir == rendering::ViewDirection::East || ctx.currentGenDir == rendering::ViewDirection::West);
            if (rot) std::swap(hL, hR);

            sf::IntRect tex = ((pL == 3) && getCliffStyle(lx, ly, ctx)) ? ctx.waterCliffs.getPiece(hL, hR) : ctx.stoneCliffs.getPiece(hL, hR);

            sf::VertexArray& vaClf = (pL == 7) ? data.cliff6Layers[ctx.dirIdx] : (pL == 5 ? data.cliff4Layers[ctx.dirIdx] : data.cliffLayers[ctx.dirIdx]);
            addQuad(vaClf, sx, sy, tex, ctx, info.biome, 64.f, rot, false, true);
        }
    }
}

void processRamps(ChunkMeshData& data, const MeshingContext& ctx, int64_t lx, int64_t ly, const TileInfo& info) {
    if (info.elevationLevel == 2 || info.elevationLevel == 4 || info.elevationLevel == 6) {
        uint8_t targetL = info.elevationLevel + 1;

        bool isDown  = ctx.getInfo(lx + ctx.dxDown,  ly + ctx.dyDown).elevationLevel >= targetL;
        bool isLeft  = ctx.getInfo(lx + ctx.dxLeft,  ly + ctx.dyLeft).elevationLevel >= targetL;
        bool isRight = ctx.getInfo(lx + ctx.dxRight, ly + ctx.dyRight).elevationLevel >= targetL;

        sf::VertexArray* vaShad = nullptr;
        sf::VertexArray* vaBotArray = nullptr;
        sf::VertexArray* vaTopArray = nullptr;

        float shiftBot = 0.0f;
        float shiftTop = 64.0f;

        if (info.elevationLevel == 2) {
            vaShad = &data.shadowVertices[ctx.dirIdx];
            vaBotArray = &data.rampLayers[ctx.dirIdx];
            vaTopArray = &data.level3Layers[ctx.dirIdx];
        } else if (info.elevationLevel == 4) {
            vaShad = &data.shadow4Vertices[ctx.dirIdx];
            vaBotArray = &data.ramp4Layers[ctx.dirIdx];
            vaTopArray = &data.level5Layers[ctx.dirIdx];
            shiftBot = 64.0f;
            shiftTop = 128.0f;
        } else if (info.elevationLevel == 6) {
            vaShad = &data.shadow6Vertices[ctx.dirIdx];
            vaBotArray = &data.ramp6Layers[ctx.dirIdx];
            vaTopArray = &data.level7Layers[ctx.dirIdx];
            shiftBot = 128.0f;
            shiftTop = 192.0f;
        }

        if (!isDown && vaShad) {
            sf::IntRect sRect(0, 0, 192, 192);
            float shadowX = lx * 64.0f + ctx.dxUp * shiftBot;
            float shadowY = ly * 64.0f + ctx.dyUp * shiftBot;
            addQuad(*vaShad, shadowX, shadowY, sRect, ctx, BiomeType::Ocean, 192.f, false, false, true);
        }

        bool texIsLeft = isRight;
        if (!isLeft && !isRight) texIsLeft = true;

        int uvRot = 0;
        if (ctx.currentGenDir == rendering::ViewDirection::West) {
            texIsLeft = !texIsLeft;
            uvRot = 1;
        } else if (ctx.currentGenDir == rendering::ViewDirection::East) {
            texIsLeft = !texIsLeft;
            uvRot = -1;
        } else if (ctx.currentGenDir == rendering::ViewDirection::South) {
            uvRot = 2;
        }

        sf::IntRect texTop = ctx.ramps.getPiece(texIsLeft, true);
        sf::IntRect texBot = ctx.ramps.getPiece(texIsLeft, false);

        sf::Vector2i biomeOffset = ctx.tileHandler.getBiomeOffset(info.biome);
        texTop.left += biomeOffset.x; texTop.top += biomeOffset.y;
        texBot.left += biomeOffset.x; texBot.top += biomeOffset.y;

        auto appendRotatedRamp = [&](sf::VertexArray& va, float x, float y, sf::IntRect tex) {
            sf::Vector2f tl(tex.left, tex.top), tr(tex.left + tex.width, tex.top);
            sf::Vector2f br(tex.left + tex.width, tex.top + tex.height), bl(tex.left, tex.top + tex.height);
            sf::Vector2f uv0 = tl, uv1 = tr, uv2 = br, uv3 = bl;

            if (uvRot == -1) { uv0 = tr; uv1 = br; uv2 = bl; uv3 = tl; }
            else if (uvRot == 1) { uv0 = bl; uv1 = tl; uv2 = tr; uv3 = br; }
            else if (uvRot == 2) { uv0 = br; uv1 = bl; uv2 = tl; uv3 = tr; }

            va.append({{x, y}, sf::Color::White, uv0});
            va.append({{x + 64.f, y}, sf::Color::White, uv1});
            va.append({{x + 64.f, y + 64.f}, sf::Color::White, uv2});
            va.append({{x, y + 64.f}, sf::Color::White, uv3});
        };

        if (isDown) {
            float sx = lx * 64.0f + ctx.dxUp * shiftTop;
            float sy = ly * 64.0f + ctx.dyUp * shiftTop;
            appendRotatedRamp(*vaTopArray, sx, sy, texTop);
        } else {
            float bx = lx * 64.0f + ctx.dxUp * shiftBot;
            float by = ly * 64.0f + ctx.dyUp * shiftBot;
            appendRotatedRamp(*vaBotArray, bx, by, texBot);

            float tx = lx * 64.0f + ctx.dxUp * shiftTop;
            float ty = ly * 64.0f + ctx.dyUp * shiftTop;
            appendRotatedRamp(*vaTopArray, tx, ty, texTop);
        }
    }
}

void processPlateauTops(ChunkMeshData& data, const MeshingContext& ctx, int64_t lx, int64_t ly, const TileInfo& info) {
    uint8_t pL = 0;
    if (info.elevationLevel == 3 || info.elevationLevel == 5 || info.elevationLevel == 7) pL = info.elevationLevel;
    else if (info.elevationLevel == 4) pL = 3;
    else if (info.elevationLevel == 6) pL = 5;

    if (pL != 0) {
        float shift = (pL == 7) ? 192.0f : (pL == 5 ? 128.0f : 64.0f);
        float sx = lx * 64.0f + (ctx.dxUp * shift);
        float sy = ly * 64.0f + (ctx.dyUp * shift);
        uint8_t mask = 0;

        auto isSolid = [&](int dx, int dy) {
            uint8_t nL = ctx.getInfo(lx+dx, ly+dy).elevationLevel;
            if (nL >= pL) return true;

            if (nL == pL - 1) {
                return (dx == ctx.dxLeft && dy == ctx.dyLeft) ||
                       (dx == ctx.dxRight && dy == ctx.dyRight);
            }
            return false;
        };

        if (isSolid(ctx.dxUp, ctx.dyUp)) mask |= rendering::North;
        if (isSolid(ctx.dxRight, ctx.dyRight)) mask |= rendering::East;
        if (isSolid(ctx.dxLeft, ctx.dyLeft)) mask |= rendering::West;
        if (isSolid(ctx.dxDown, ctx.dyDown)) mask |= rendering::South;

        if (ctx.currentGenDir == rendering::ViewDirection::East || ctx.currentGenDir == rendering::ViewDirection::West) {
            uint8_t flp = mask & ~(rendering::East | rendering::West);
            if (mask & rendering::East) flp |= rendering::West;
            if (mask & rendering::West) flp |= rendering::East;
            mask = flp;
        }

        sf::VertexArray& va = (pL == 7) ? data.level7Layers[ctx.dirIdx] : (pL == 5 ? data.level5Layers[ctx.dirIdx] : data.level3Layers[ctx.dirIdx]);
        addQuad(va, sx, sy, ctx.grassOverStone.getByMask(mask), ctx, info.biome, 64.f, false, true, false);
    }
}

}