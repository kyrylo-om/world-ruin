#include "../../../include/Simulation/World/ChunkSimulation.hpp"
#include "../../../include/Simulation/Unit/UnitAnimationSystem.hpp"
#include "../../../include/Simulation/Environment/ResourceUpdateSystem.hpp"
#include "../../../include/Simulation/Environment/DebrisSystem.hpp"
#include "../../../include/Simulation/World/TaskSystem.hpp"
#include "ECS/Components.hpp"
#include <cmath>

namespace wr::simulation {

ChunkSimulation::ChunkSimulation(core::ThreadPool& threadPool, entt::registry& registry, world::ChunkManager& chunkManager)
    : m_threadPool(threadPool), m_registry(registry), m_chunkManager(chunkManager) {
    m_phaseFutures.reserve(128);
}

void ChunkSimulation::update(float dt, rendering::ViewDirection viewDir) {
    const auto& chunks = m_chunkManager.getChunks();
    std::vector<world::Chunk*> activeChunks;
    activeChunks.reserve(chunks.size());

    for (auto& [coord, chunk] : chunks) {
        if (chunk->state.load(std::memory_order_acquire) == world::ChunkState::Active) {
            activeChunks.push_back(chunk.get());
        }
    }

    executePhase(UpdatePhase::EvenX_EvenY, activeChunks, dt);
    executePhase(UpdatePhase::OddX_OddY, activeChunks, dt);
    executePhase(UpdatePhase::EvenX_OddY, activeChunks, dt);
    executePhase(UpdatePhase::OddX_EvenY, activeChunks, dt);

    UnitAnimationSystem::update(m_registry, dt, viewDir);
    ResourceUpdateSystem::update(m_registry, dt, activeChunks, m_chunkManager);
    DebrisSystem::update(m_registry, dt);
    TaskSystem::update(m_registry);
}

UpdatePhase ChunkSimulation::getPhase(const math::Vec2i64& coord) const noexcept {
    bool evenX = (std::abs(coord.x) % 2 == 0);
    bool evenY = (std::abs(coord.y) % 2 == 0);
    if (evenX && evenY) return UpdatePhase::EvenX_EvenY;
    if (!evenX && !evenY) return UpdatePhase::OddX_OddY;
    if (evenX && !evenY) return UpdatePhase::EvenX_OddY;
    return UpdatePhase::OddX_EvenY;
}

void ChunkSimulation::executePhase(UpdatePhase phase, const std::vector<world::Chunk*>& activeChunks, float dt) {
    m_phaseFutures.clear();
    for (world::Chunk* chunk : activeChunks) {
        if (getPhase(chunk->coordinate) == phase) {
            m_phaseFutures.push_back(m_threadPool.enqueue([this, chunk, dt]() {
                this->processChunk(*chunk, dt);
            }));
        }
    }
    for (auto& future : m_phaseFutures) {
        if (future.valid()) future.wait();
    }
}

void ChunkSimulation::processChunk(world::Chunk& chunk, float dt) noexcept {
    for (entt::entity e : chunk.entities) {
        if (auto* anim = m_registry.try_get<ecs::AnimationState>(e)) {
            anim->elapsedTime += dt;
            if (anim->elapsedTime >= anim->frameDuration) {
                anim->elapsedTime = 0.0f;
                anim->currentFrame = static_cast<uint16_t>((anim->currentFrame + 1) % anim->numFrames);
            }
        }
    }
}

}