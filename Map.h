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
            { Y_POSITIVE, Y_NEGATIVE, X_NEGATIVE, X_POSITIVE };
        Direction dir;
        unsigned voxelX;
        unsigned voxelY;
        unsigned voxelLengthX;
        unsigned voxelLengthY;
    };
    struct PointSource
    {
        enum class Type : uint8_t
            {CLICK, GAUSIAN_PULSE};
        size_t voxelIndex;
        Type type;
        float timeLeft;
        float totalTime;
        float printMeTime;
        PointSource(size_t voxelIndex = 0, float time = 0.f, Type t = Type::CLICK);
        double step();
    };
    struct Partition
    {
        Partition(unsigned y, unsigned x, unsigned lx, unsigned ly);
        Partition(const Partition& other);
        ~Partition();
        unsigned voxelY;//Bottom
        unsigned voxelX;//Left
        unsigned voxelLengthX;
        unsigned voxelLengthY;
        std::vector<PartitionInterface> interfaces;
        double* voxelModes;
        double* voxelModesPrevious;
        double* voxelForcingTerms;
        double* voxelPressures;
        fftw_plan planModeToPressure;
        fftw_plan planForcingToModes;
        PointSource ps;
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
    void touch(const sf::Vector2f& worldSpaceLocation);
private:
    // loading/precomputation functions //
    bool loadJsonMap(const std::string& jsonMapFilename);
    bool loadTileset(const std::string& jsonMapFilename);
    void buildMapTileVBO();
    void buildVoxelGridVBO();
    void buildVoxelPressureVBO();
    void decomposeVoxelsIntoPartitions();
    void buildPartitionVBO();
    void calculatePartitionInterfaces();
    void buildInterfaceVBO();
    // /////////////////////////////// //
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
    unsigned voxelGridLengthY;
    unsigned voxelGridLengthX;
    std::vector<std::vector<double*>> globalPressureLookupTable;
    // precomputation meta //
    std::vector<std::vector<VoxelMeta>> voxelMeta;
    float mapPixelHeight;
    unsigned numInterfaces;
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