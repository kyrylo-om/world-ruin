#pragma once

namespace wr::simulation::config {

    struct BuildingCost {
        int wood{0};
        int rock{0};
    };

    struct BuildingDef {
        float width;
        float height;
        float hitboxWidth;
        float hitboxHeight;
        float resourceZoneWidth;
        float resourceZoneHeight;
        float buildTime;
        BuildingCost cost;
    };

    constexpr BuildingDef HOUSE_1 = { 3.0f, 3.0f, 2.2f, 2.0f, 4.0f, 4.0f, 30.0f, { 20, 12 } };

    constexpr BuildingDef CITY_STORAGE = { 5.0f, 5.0f, 5.0f, 5.0f, 5.0f, 5.0f, 0.0f, { 0, 0 } };

    constexpr int UNITS_PER_HOUSE = 4;

}