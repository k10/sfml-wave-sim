#pragma once
#include <SFML/Graphics.hpp>
#include <string>
#include <nlohmann\json.hpp>
using json = nlohmann::json;
#include <fstream>
/*
    In world space, each tile shall take up 1 square meter
*/
class Map
{
private:
    static const float SOUND_SPEED_METERS_PER_SECOND;
    // This value is tweakable, as human hearing limits are around 22khz
    //  but increasing accuracy == HUGE increase in time/space requirements
    static const float MAXIMUM_SOUND_HZ;
    // this refers to the "h" variable in the research paper
    //  restricted by Nyquist theorem
    static const float SIM_VOXEL_SPACING;
    // not entirely sure what this unit is.. probably seconds??
    //  restricted by "the CFL condition"
    static const float SIM_DELTA_TIME;
    struct Partition
    {
        unsigned voxelRow;
        unsigned voxelCol;
        unsigned voxelW;
        unsigned voxelH;
    };
public:
    // returns false if any loading steps fuck up, true if we gucci
    bool load(const std::string& jsonMapFilename);
    void draw(sf::RenderTarget& rt);
private:
    // Simulation data //
    sf::VertexArray vaSimGridLines;
    sf::VertexArray vaSimPartitions;
    std::vector<Partition> partitions;
    // JSON map data //
    json jsonMap;
    sf::Texture texTileset;
    sf::VertexArray vaTiles;
    unsigned mapCols;
    unsigned mapRows;
    unsigned tilePixW;
    unsigned tilePixH;
    unsigned tilesetColumns;
};

