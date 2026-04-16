#pragma once
#include <cstdint>
#include <string>

namespace wr::world {

    enum class BiomeType : uint8_t {
        Ocean = 0,
        River,
        Plains,
        Plains2,
        Bush,
        SpineForest,
        Count
    };

    struct BiomeData {
        BiomeType type;
        std::string texturePath;
        float threshold;
    };

    class BiomeRegistry {
    public:
        static const BiomeData& get(BiomeType type) {
            return m_registry[static_cast<size_t>(type)];
        }
    private:
        static inline const BiomeData m_registry[] = {
            { BiomeType::Ocean,       "", 0.3f },
            { BiomeType::River,       "", 0.0f },
            { BiomeType::Plains,      "pack/Tiny Swords (Free Pack)/Terrain/Tileset/Tilemap_color1.png", 0.45f },
            { BiomeType::Plains2,     "pack/Tiny Swords (Free Pack)/Terrain/Tileset/Tilemap_color2.png", 0.6f },
            { BiomeType::Bush,        "pack/Tiny Swords (Free Pack)/Terrain/Tileset/Tilemap_color3.png", 0.8f },
            { BiomeType::SpineForest, "pack/Tiny Swords (Free Pack)/Terrain/Tileset/Tilemap_color5.png", 1.0f }
        };
    };

}