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
    struct Voxel
    {
    };
public:
    // returns false if any loading steps fuck up, true if we gucci
    bool load(const std::string& jsonMapFilename);
    void draw(sf::RenderTarget& rt);
    // since the simulation requires a fixed timestep bound by "the CFL condition",
    //  we don't pass the true delta-time between frames since we don't need it
    void stepSimulation();
private:
    // Simulation data //
    sf::VertexArray vaSimGridLines;
    sf::VertexArray vaSimPartitions;
    std::vector<Partition> partitions;
    std::vector<std::vector<Voxel>> voxels;
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

