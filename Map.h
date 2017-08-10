#pragma once
#include <SFML/Graphics.hpp>
#include <string>
#include <nlohmann\json.hpp>
using json = nlohmann::json;
#include <fstream>
#include <fftw3.h>
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
    struct PartitionInterface
    {
        enum class Direction : uint8_t
            { UP, DOWN, LEFT, RIGHT };
        Direction dir;
        unsigned voxelBottomRow;
        unsigned voxelLeftCol;
        unsigned voxelW;
        unsigned voxelH;
        size_t partitionIndexOther;
    };
    struct Partition
    {
        Partition(unsigned rowBottom, unsigned colLeft, unsigned w, unsigned h);
        Partition(const Partition& other);
        ~Partition();
        unsigned voxelRow;//Bottom
        unsigned voxelCol;//Left
        unsigned voxelW;
        unsigned voxelH;
        std::vector<PartitionInterface> interfaces;
        double* voxelModes;
        double* voxelModesPrevious;
        double* voxelForcingTerms;
        double* voxelPressures;
        fftw_plan planModeToPressure;
        fftw_plan planForcingToModes;
    };
    struct VoxelMeta
    {
        VoxelMeta(int partitionIndex = -1, uint8_t interfacedDirs = 0);
        int partitionIndex;
        uint8_t interfacedDirectionFlags;
    };
public:
    Map();
    ~Map();
    // returns false if any loading steps fuck up, true if we gucci
    bool load(const std::string& jsonMapFilename);
    void draw(sf::RenderTarget& rt);
    // since the simulation requires a fixed timestep bound by "the CFL condition",
    //  we don't pass the true delta-time between frames since we don't need it
    void stepSimulation();
    void toggleVoxelGrid();
    void togglePartitionMeta();
private:
    void nullify();
private:
    // MISC //
    bool m_showVoxelGrid;
    bool m_showPartitionMeta;
    // Simulation data //
    sf::VertexArray vaSimGridLines;
    sf::VertexArray vaSimPartitions;
    sf::VertexArray vaSimPartitionInterfaces;
    sf::VertexArray vaSimGridPressures;
    std::vector<Partition> partitions;
    unsigned voxelRows;
    unsigned voxelCols;
    // precomputation meta //
    std::vector<std::vector<VoxelMeta>> voxelMeta;
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