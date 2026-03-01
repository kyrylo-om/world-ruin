#pragma once

#include "Core/Math.hpp"
#include "World/Chunk.hpp"
#include "Rendering/Camera.hpp"
#include "Rendering/TileHandler.hpp"
#include "Rendering/Perspective.hpp"
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/Shader.hpp>
#include <vector>

namespace wr::rendering {

    struct ChunkRenderData {
        const world::Chunk* chunk;
        sf::Transform transform;
    };

    struct RenderContext {
        sf::RenderTarget& window;
        const Camera& camera;
        const TileHandler& tiles;
        float currentAngle;
        int dirIdx;
        ViewDirection dir;
        sf::FloatRect viewBounds;
        bool useLOD;
        sf::Shader* foamShader;

        float timeSeconds;
        float pulseScale;

        bool animateWind;
    };

}