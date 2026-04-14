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

        int pendingBuildings{0};
        int totalHouses{0};
        float averageDropoffDistance{0.0f};
    };

    struct SpawnedFlag {};

    enum class ExpansionPhase {
        PlaceStorage,
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

    private:
        sf::Clock m_internalClock;
        float m_lastScanTime{0.0f};
        float m_lastLogisticsTime{0.0f};
        float m_lastBuildTime{0.0f};
        int m_globalTaskId{1000};

        ExpansionPhase m_phase{ExpansionPhase::PlaceStorage};
        std::vector<entt::entity> m_pendingBuildings;
        int m_totalUnitsAtPlanTime{0};
    };

}