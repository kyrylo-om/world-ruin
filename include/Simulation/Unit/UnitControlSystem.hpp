#pragma once
#include "Core/Math.hpp"
#include "World/ChunkManager.hpp"
#include "Rendering/Perspective.hpp"
#include "Simulation/Unit/UnitPhysicsSystem.hpp"
#include "ECS/Components.hpp"
#include <entt/entt.hpp>
#include <vector>
#include <functional>
#include <mutex>
#include <future>

namespace wr::simulation {

    struct DeferredQueues {
        std::vector<entt::entity> removeActionTarget;
        std::vector<entt::entity> removePath;
        std::vector<entt::entity> removePathRequest;
        std::vector<entt::entity> removeWorkerTag;
        std::vector<entt::entity> removeClaimedTag;
        std::vector<entt::entity> destroyEntity;
        std::vector<entt::entity> addSolidTag;
        std::vector<entt::entity> addIdleTag;
        std::vector<std::pair<entt::entity, entt::entity>> setActionTarget;
        std::vector<std::pair<entt::entity, std::shared_future<std::vector<math::Vec2f>>>> setPathRequest;
        std::vector<std::pair<entt::entity, ecs::Path>> setPath;
        std::vector<std::function<void()>> complex;

        void reserve(size_t cap) {
            removeActionTarget.reserve(cap);
            removePath.reserve(cap);
            removePathRequest.reserve(cap);
            removeWorkerTag.reserve(cap);
            removeClaimedTag.reserve(cap);
            destroyEntity.reserve(cap);
            addSolidTag.reserve(cap);
            addIdleTag.reserve(cap);
            setActionTarget.reserve(cap);
            setPathRequest.reserve(cap);
            setPath.reserve(cap);
            complex.reserve(cap);
        }

        void merge(DeferredQueues& other) {
            removeActionTarget.insert(removeActionTarget.end(), other.removeActionTarget.begin(), other.removeActionTarget.end());
            removePath.insert(removePath.end(), other.removePath.begin(), other.removePath.end());
            removePathRequest.insert(removePathRequest.end(), other.removePathRequest.begin(), other.removePathRequest.end());
            removeWorkerTag.insert(removeWorkerTag.end(), other.removeWorkerTag.begin(), other.removeWorkerTag.end());
            removeClaimedTag.insert(removeClaimedTag.end(), other.removeClaimedTag.begin(), other.removeClaimedTag.end());
            destroyEntity.insert(destroyEntity.end(), other.destroyEntity.begin(), other.destroyEntity.end());
            addSolidTag.insert(addSolidTag.end(), other.addSolidTag.begin(), other.addSolidTag.end());
            addIdleTag.insert(addIdleTag.end(), other.addIdleTag.begin(), other.addIdleTag.end());
            setActionTarget.insert(setActionTarget.end(), other.setActionTarget.begin(), other.setActionTarget.end());
            setPathRequest.insert(setPathRequest.end(), other.setPathRequest.begin(), other.setPathRequest.end());
            setPath.insert(setPath.end(), other.setPath.begin(), other.setPath.end());
            complex.insert(complex.end(), other.complex.begin(), other.complex.end());
        }
    };

    class UnitControlSystem {
    public:
        explicit UnitControlSystem(world::ChunkManager& chunkManager);

        void update(entt::registry& registry, float dt, rendering::ViewDirection viewDir, const math::Vec2f& mouseWorldPos, const math::Vec2f& simulationCenterWorld, bool isRightClicking, bool isTaskMode) noexcept;

    private:
        world::ChunkManager& m_chunkManager;
        EntitySpatialGrid m_unitGrid;
        EntitySpatialGrid m_solidGrid;
        bool m_prevSpacePressed{false};
        bool m_prevPPressed{false};
        DeferredQueues m_globalQueues;
        std::mutex m_deferredMutex;
    };

}