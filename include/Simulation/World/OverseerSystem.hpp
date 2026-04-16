#pragma once

#include "ECS/Components.hpp"
#include "World/ChunkManager.hpp"
#include <entt/entt.hpp>
#include <SFML/System/Clock.hpp>
#include <vector>

namespace wr::simulation {

    struct SimBlackboard {
        int popWarrior{0};
        int popLumberjack{0};
        int popMiner{0};
        int popCourier{0};
        int popBuilder{0};
        int totalUnits{0};

        int availWood{0};
        int availRock{0};

        int demandWood{0};
        int demandRock{0};

        int nextStageWood{0};
        int nextStageRock{0};

        int pendingBuildings{0};
        int totalHouses{0};
        float averageDropoffDistance{0.0f};
    };

    struct SpawnedFlag {};

    enum class ExpansionPhase {
        PlaceStorage,
        ClearForStorage,
        ClearHousingArea,
        PlanInitialHousing,
        WaitInitialHousing,
        PlanGrowthHousing,
        WaitGrowthHousing
    };

    class OverseerSystem {
    public:
        OverseerSystem() = default;

        void updateEconomy(entt::registry& registry);
        void manageResources(entt::registry& registry, world::ChunkManager& chunkManager);
        void manageLogistics(entt::registry& registry, world::ChunkManager& chunkManager);
        void planExpansion(entt::registry& registry, world::ChunkManager& chunkManager);
        void processDemographics(entt::registry& registry);

        // Collects idle workers, derives include flags from their types,
        // builds a PendingTaskArea, and uses the shared task pipeline (scanResources + finalizeTask).
        // Adds exactly one courier for auto-collection if the task involves trees or rocks.
        entt::entity createTask(entt::registry& registry,
                                math::Vec2f areaStart, math::Vec2f areaEnd,
                                bool collectFutureDrops, bool useCityStorage,
                                bool hasDropoff, math::Vec2f dropoffStart, math::Vec2f dropoffEnd);

    private:
        sf::Clock m_internalClock;
        float m_lastScanTime{0.0f};
        float m_lastLogisticsTime{0.0f};
        float m_lastBuildTime{0.0f};
        int m_globalTaskId{1000};

        ExpansionPhase m_phase{ExpansionPhase::PlaceStorage};
        std::vector<entt::entity> m_pendingBuildings;
        int m_totalUnitsAtPlanTime{0};

        math::Vec2f m_pendingStoragePos{0.0f, 0.0f};
        entt::entity m_clearingTask{entt::null};
    };

}