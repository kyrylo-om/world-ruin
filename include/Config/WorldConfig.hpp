#pragma once

#include <cstdint>

namespace wr::world::config {

    constexpr float WATER_THRESHOLD = 0.35f;
    constexpr float RIVER_OVERRIDE  = -1.0f;

    constexpr float ELEVATION_T1 = 0.60f;
    constexpr float ELEVATION_T2 = 0.75f;
    constexpr float ELEVATION_T3 = 0.85f;

    constexpr float RAMP_THRESHOLD = 0.0001f;

    constexpr float MOISTURE_PLAINS  = 0.30f;
    constexpr float MOISTURE_PLAINS2 = 0.50f;
    constexpr float MOISTURE_BUSH    = 0.70f;

    constexpr float ENTITY_OFFSET_BASE = 0.2f;
    constexpr float ENTITY_OFFSET_VAR  = 0.6f;

    constexpr int   ROCK_CLIFF_CHECK_RADIUS_SQ = 9;
    constexpr float ROCK_CHANCE_NEAR_CLIFF     = 0.15f;
    constexpr float ROCK_CHANCE_BASE           = 0.015f;
    constexpr float MAX_ROCK_TYPES             = 4.0f;

    constexpr float TREE_SPAWN_PENALTY_NON_SPINE = 0.70f;
    constexpr float TREE_CHANCE_SPINE            = 0.35f;
    constexpr float TREE_CHANCE_BUSH             = 0.10f * TREE_SPAWN_PENALTY_NON_SPINE;
    constexpr float TREE_CHANCE_PLAINS2          = 0.05f * TREE_SPAWN_PENALTY_NON_SPINE;
    constexpr float TREE_CHANCE_PLAINS           = 0.02f * TREE_SPAWN_PENALTY_NON_SPINE;
    constexpr float MAX_TREE_TYPES               = 4.0f;

    constexpr float BUSH_CHANCE_BUSH             = 0.35f;
    constexpr float BUSH_CHANCE_PLAINS2          = 0.15f;
    constexpr float BUSH_CHANCE_SPINE            = 0.10f;
    constexpr float BUSH_CHANCE_PLAINS           = 0.02f;
    constexpr float MAX_BUSH_TYPES               = 4.0f;

    constexpr float TREE_TYPE_SPINE_PINE_RATIO   = 0.90f;
    constexpr float TREE_TYPE_PLAINS_BIRCH_RATIO = 0.90f;

    constexpr uint32_t SEED_OFFSET_X     = 111;
    constexpr uint32_t SEED_OFFSET_Y     = 222;
    constexpr uint32_t SEED_ROCK_ROLL    = 999;
    constexpr uint32_t SEED_ROCK_TYPE    = 888;
    constexpr uint32_t SEED_TREE_ROLL    = 777;
    constexpr uint32_t SEED_TREE_TYPE    = 666;
    constexpr uint32_t SEED_TREE_SUBTYPE = 555;

    constexpr uint32_t SEED_BUSH_ROLL    = 444;
    constexpr uint32_t SEED_BUSH_TYPE    = 333;

}