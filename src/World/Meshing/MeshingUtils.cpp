#include "World/Meshing/MeshingContext.hpp"
#include <algorithm>

namespace wr::world {

void addQuad(sf::VertexArray& va, float px, float py, sf::IntRect tex, const MeshingContext& ctx,
             BiomeType biome, float size, bool rotate180, bool applyGroundRot, bool applyCliffRot) {

    sf::Vector2i offsetUV = ctx.tileHandler.getBiomeOffset(biome);
    tex.left += offsetUV.x;
    tex.top += offsetUV.y;

    float offset = (size - 64.0f) / 2.0f;
    float x = px - offset;
    float y = py - offset;

    sf::Vector2f tl(tex.left, tex.top), tr(tex.left + tex.width, tex.top);
    sf::Vector2f br(tex.left + tex.width, tex.top + tex.height), bl(tex.left, tex.top + tex.height);
    sf::Vector2f uv0 = tl, uv1 = tr, uv2 = br, uv3 = bl;

    if (applyCliffRot) {
        switch (ctx.currentGenDir) {
            case rendering::ViewDirection::East:  uv0=bl; uv1=tl; uv2=tr; uv3=br; break;
            case rendering::ViewDirection::South: uv0=br; uv1=bl; uv2=tl; uv3=tr; break;
            case rendering::ViewDirection::West:  uv0=tr; uv1=br; uv2=bl; uv3=tl; break;
            default: break;
        }
    } else if (applyGroundRot) {
        switch (ctx.currentGenDir) {
            case rendering::ViewDirection::East:  uv0=tr; uv1=br; uv2=bl; uv3=tl; break;
            case rendering::ViewDirection::South: uv0=br; uv1=bl; uv2=tl; uv3=tr; break;
            case rendering::ViewDirection::West:  uv0=bl; uv1=tl; uv2=tr; uv3=br; break;
            default: break;
        }
    }

    if (rotate180) { std::swap(uv0, uv2); std::swap(uv1, uv3); }

    va.append({{x, y}, sf::Color::White, uv0});
    va.append({{x + size, y}, sf::Color::White, uv1});
    va.append({{x + size, y + size}, sf::Color::White, uv2});
    va.append({{x, y + size}, sf::Color::White, uv3});
}

bool isShadowVisible(rendering::ViewDirection view, int dx, int dy) {
    if (view == rendering::ViewDirection::North && dy < 0) return false;
    if (view == rendering::ViewDirection::South && dy > 0) return false;
    if (view == rendering::ViewDirection::East  && dx < 0) return false;
    if (view == rendering::ViewDirection::West  && dx > 0) return false;
    return true;
}

bool getCliffStyle(int64_t lx, int64_t ly, const MeshingContext& ctx) {
    int waterCount = 0;
    int lowGroundCount = 0;
    int highGroundCount = 0;

    int offs[4][2] = {{0,-1}, {0,1}, {-1,0}, {1,0}};
    for (auto& o : offs) {
        TileInfo n = ctx.getInfo(lx + o[0], ly + o[1]);
        if (n.type == core::TerrainType::Water) {
            waterCount++;
        } else if (n.type == core::TerrainType::Ground) {
            if (n.elevationLevel <= 2) {
                lowGroundCount++;
            } else if (n.elevationLevel >= 3) {
                highGroundCount++;
            }
        }
    }

    if (waterCount == 0) return false;

    if (lowGroundCount >= 2) return false;

    if (lowGroundCount >= 1 && highGroundCount >= 1) return false;

    return true;
}

}