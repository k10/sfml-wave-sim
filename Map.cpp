#include "Map.h"
#include "toolbox.h"
const float Map::SOUND_SPEED_METERS_PER_SECOND = 340;
const float Map::MAXIMUM_SOUND_HZ = 2000;
const float Map::SIM_VOXEL_SPACING = SOUND_SPEED_METERS_PER_SECOND/(2*MAXIMUM_SOUND_HZ);
const float Map::SIM_DELTA_TIME = SIM_VOXEL_SPACING/(SOUND_SPEED_METERS_PER_SECOND*sqrtf(3));
Map::Map()
    :m_showVoxelGrid(false)
    ,m_showPartitionMeta(false)
{
}
Map::~Map()
{
    nullify();
}
bool Map::load(const std::string & jsonMapFilename)
{
    nullify();
    // first, load the map.json file //
    std::ifstream fileJsonMap(jsonMapFilename);
    if (fileJsonMap.is_open())
    {
        fileJsonMap >> jsonMap;
    }
    else
    {
        std::cerr << "ERROR: could not open \""<<jsonMapFilename<<"\"\n";
        return false;
    }
    // extract the map's folder string //
    std::string strMapAssetFolder;
    size_t folderSlashIndex = jsonMapFilename.find_last_of("/");
    if (folderSlashIndex != std::string::npos)
    {
        strMapAssetFolder = jsonMapFilename.substr(0, folderSlashIndex + 1);
    }
    // extract first tileset image filename //
    std::string strTilesetFilename = jsonMap["tilesets"][0]["image"];
    strTilesetFilename = strMapAssetFolder + strTilesetFilename;
    // load the tileset image into a Sprite //
    if (!texTileset.loadFromFile(strTilesetFilename))
    {
        std::cerr << "ERROR: could not load tileset image! \"" << strTilesetFilename << "\"";
        return false;
    }
    // iterate over all the tiles and load them all into a VBO //
    tilePixW = jsonMap["tilesets"][0]["tilewidth"];
    tilePixH = jsonMap["tilesets"][0]["tileheight"];
    tilesetColumns = jsonMap["tilesets"][0]["columns"];
    mapRows = jsonMap["layers"][0]["height"];
    mapCols = jsonMap["layers"][0]["width"];
    const float MAP_PIX_H = float(mapRows);// *tilePixH);
    vaTiles = sf::VertexArray(sf::PrimitiveType::Quads, 4 * mapRows*mapCols);
    for (unsigned r = 0; r < mapRows; r++)
    {
        for (unsigned c = 0; c < mapCols; c++)
        {
            unsigned tileArrayIndex = r*mapCols + c;
            int tileId = jsonMap["layers"][0]["data"][tileArrayIndex] - 1;
            if (tileId < 0)
            {
                continue;
            }
            vaTiles[4 * tileArrayIndex + 0].position = { float(c*1),    float(MAP_PIX_H - (r)*1)};
            vaTiles[4 * tileArrayIndex + 1].position = { float((c+1)*1),float(MAP_PIX_H - (r)*1)};
            vaTiles[4 * tileArrayIndex + 2].position = { float((c+1)*1),float(MAP_PIX_H - (r+1)*1)};
            vaTiles[4 * tileArrayIndex + 3].position = { float(c*1),    float(MAP_PIX_H - (r+1)*1)};
            const int TILESET_ROW = tileId / tilesetColumns;
            const int TILESET_COL = tileId % tilesetColumns;
            const sf::Vector2f TILE_UPPER_LEFT(float(TILESET_COL*tilePixW), float(TILESET_ROW*tilePixH));
            vaTiles[4 * tileArrayIndex + 0].texCoords = TILE_UPPER_LEFT + sf::Vector2f{0.f,0.f};
            vaTiles[4 * tileArrayIndex + 1].texCoords = TILE_UPPER_LEFT + sf::Vector2f{ float(tilePixW),0.f};
            vaTiles[4 * tileArrayIndex + 2].texCoords = TILE_UPPER_LEFT + sf::Vector2f{ float(tilePixW),float(tilePixH) };
            vaTiles[4 * tileArrayIndex + 3].texCoords = TILE_UPPER_LEFT + sf::Vector2f{0.f,float(tilePixH)};
        }
    }
    // build VBO for the simulation grid lines //
    voxelRows = unsigned(mapRows / SIM_VOXEL_SPACING);
    voxelCols = unsigned(mapCols / SIM_VOXEL_SPACING);
    std::cout << "voxel grid={" << voxelCols << "x" << voxelRows << "}\n";
    vaSimGridLines = sf::VertexArray(sf::PrimitiveType::Lines, 2 * (voxelRows + 1) + 2 * (voxelCols + 1));
    const float MAP_LEFT = 0;
    const float MAP_RIGHT = float(mapCols);
    for (unsigned r = 0; r < voxelRows + 1; r++)
    {
        vaSimGridLines[2 * r + 0].position = { MAP_LEFT, float(r*SIM_VOXEL_SPACING) };
        vaSimGridLines[2 * r + 1].position = { MAP_RIGHT, float(r*SIM_VOXEL_SPACING) };
    }
    const float MAP_TOP = float(mapRows);
    const float MAP_BOTTOM = 0;
    for (unsigned c = 0; c < voxelCols + 1; c++)
    {
        vaSimGridLines[2 * (voxelRows + 1) + 2 * c + 0].position = { float(c*SIM_VOXEL_SPACING), MAP_TOP };
        vaSimGridLines[2 * (voxelRows + 1) + 2 * c + 1].position = { float(c*SIM_VOXEL_SPACING), MAP_BOTTOM };
    }
    // decompose simulation voxels into large rectangular partitions //////////////////////////////
    voxelMeta.resize(voxelRows, std::vector<VoxelMeta>(voxelCols));
    auto checkNextPartitionRow = [&](unsigned partitionBottomRow, unsigned partitionLeftCol,
        unsigned currPartitionW, unsigned currPartitionH )->bool
    {
        if (partitionBottomRow + currPartitionH >= voxelRows)
        {
            return false;
        }
        // we iterate along the -Y edge of the partition
        //      (because of the way the json map stores the map tiles)
        //  and return true if all the voxels below are empty space
        for (unsigned c = 0; c < currPartitionW; c++)
        {
            const unsigned vc = partitionLeftCol + c;
            const unsigned vr = partitionBottomRow + currPartitionH;
            const sf::Vector2f worldPos((vc + 0.5f)*SIM_VOXEL_SPACING,
                MAP_PIX_H - (vr + 0.5f)*SIM_VOXEL_SPACING);
            // because our units are meters, and each map tile is 1m^s,
            //  we can just cast to ints to obtain map tile indexes:
            const unsigned mapRow = unsigned(worldPos.y);
            const unsigned mapCol = unsigned(worldPos.x);
            unsigned tileArrayIndex = mapRow*mapCols + mapCol;
            int tileId = jsonMap["layers"][0]["data"][tileArrayIndex];
            if (tileId > 0 || voxelMeta[vr][vc].partitionIndex >= 0)
            {
                return false;
            }
        }
        return true;
    };
    auto checkNextPartitionCol = [&](unsigned partitionBottomRow, unsigned partitionLeftCol,
        unsigned currPartitionW, unsigned currPartitionH)->bool
    {
        if (partitionLeftCol + currPartitionW >= voxelCols)
        {
            return false;
        }
        // we iterate along the +X edge of the partition 
        //  and return true if all the voxels below are empty space
        for (unsigned r = 0; r < currPartitionH; r++)
        {
            const unsigned vc = partitionLeftCol + currPartitionW;
            const unsigned vr = partitionBottomRow + r;
            const sf::Vector2f worldPos((vc + 0.5f)*SIM_VOXEL_SPACING,
                MAP_PIX_H - (vr + 0.5f)*SIM_VOXEL_SPACING);
            // because our units are meters, and each map tile is 1m^s,
            //  we can just cast to ints to obtain map tile indexes:
            const unsigned mapRow = unsigned(worldPos.y);
            const unsigned mapCol = unsigned(worldPos.x);
            unsigned tileArrayIndex = mapRow*mapCols + mapCol;
            int tileId = jsonMap["layers"][0]["data"][tileArrayIndex];
            if (tileId > 0 || voxelMeta[vr][vc].partitionIndex >= 0)
            {
                return false;
            }
        }
        return true;
    };
    unsigned simulationVoxelTotal = 0;///DEBUG
    for (unsigned r = 0; r < voxelRows; r++)
    {
        for (unsigned c = 0; c < voxelCols; c++)
        {
            const sf::Vector2f worldPos((c + 0.5f)*SIM_VOXEL_SPACING, 
                MAP_PIX_H - (r + 0.5f)*SIM_VOXEL_SPACING);
            // because our units are meters, and each map tile is 1m^s,
            //  we can just cast to ints to obtain map tile indexes:
            const unsigned mapRow = unsigned(worldPos.y);
            const unsigned mapCol = unsigned(worldPos.x);
            unsigned tileArrayIndex = mapRow*mapCols + mapCol;
            int tileId = jsonMap["layers"][0]["data"][tileArrayIndex];
            if (tileId > 0 || voxelMeta[r][c].partitionIndex >= 0)
            {
                // every non-zero tile is considered solid
                //  as well as every previously decomposed voxel
                continue;
            }
            unsigned partitionW = 1;
            unsigned partitionH = 1;
            while (checkNextPartitionRow(r, c, partitionW, partitionH)) 
            {
                partitionH++;
            }
            while (checkNextPartitionCol(r, c, partitionW, partitionH))
            {
                partitionW++;
            }
            // we need to mark the voxels in this partition as decomposed
            //  so they don't go into new partitions
            for (unsigned vr = r; vr < r + partitionH; vr++)
            {
                for (unsigned vc = c; vc < c + partitionW; vc++)
                {
                    voxelMeta[vr][vc].partitionIndex = partitions.size();
                }
            }
            simulationVoxelTotal += partitionW*partitionH;
            partitions.push_back({ r,c,partitionW,partitionH });
        }
    }
    std::cout << "simulationVoxelTotal=" << simulationVoxelTotal << std::endl;
    // Accumulate VBO for drawing the partitions //
    vaSimPartitions = sf::VertexArray(sf::PrimitiveType::Quads, 4 * partitions.size());
    for (size_t p = 0; p < partitions.size(); p++)
    {
        const auto& partition = partitions[p];
        const float pLeft = float(partition.voxelCol*SIM_VOXEL_SPACING);
        const float pRight = float((partition.voxelCol + partition.voxelW)*SIM_VOXEL_SPACING);
        const float pTop = float((partition.voxelRow + partition.voxelH)*SIM_VOXEL_SPACING);
        const float pBottom = float(partition.voxelRow*SIM_VOXEL_SPACING);
        const sf::Color color(255, 255, 255, 64);
        vaSimPartitions[4 * p + 0].position = {pLeft, pBottom};
        vaSimPartitions[4 * p + 1].position = {pRight, pBottom};
        vaSimPartitions[4 * p + 2].position = {pRight, pTop};
        vaSimPartitions[4 * p + 3].position = {pLeft, pTop};
        vaSimPartitions[4 * p + 0].color = color;
        vaSimPartitions[4 * p + 1].color = color;
        vaSimPartitions[4 * p + 2].color = color;
        vaSimPartitions[4 * p + 3].color = color;
    }
    // Calculate partition interfaces //
    auto addPartitionInterfaceMeta = [&](const PartitionInterface& i)->void
    {
        for (unsigned r = i.voxelBottomRow; r < i.voxelBottomRow + i.voxelH; r++)
        {
            for (unsigned c = i.voxelLeftCol; c < i.voxelLeftCol + i.voxelW; c++)
            {
                voxelMeta[r][c].interfacedDirectionFlags |= (1 << int(i.dir));
            }
        }
    };
    unsigned numInterfaces = 0;/// DEBUG
    auto addTransientInterface = [&](Map::Partition& partition,
        PartitionInterface& transientInterface,
        PartitionInterface::Direction opposingInterfaceDir,
        const sf::Vector2i& edgeNeighborOffset,
        size_t partitionIndex)->void
    {
        const sf::Vector2i interfaceBaseVoxelIndex(
            transientInterface.voxelLeftCol, 
            transientInterface.voxelBottomRow);
        const sf::Vector2i edgeNeighborIndex = interfaceBaseVoxelIndex +edgeNeighborOffset;
        Map::VoxelMeta& edgeNeighborVoxel = voxelMeta[edgeNeighborIndex.y][edgeNeighborIndex.x];
        transientInterface.partitionIndexOther = size_t(edgeNeighborVoxel.partitionIndex);
        partition.interfaces.push_back(transientInterface);
        addPartitionInterfaceMeta(transientInterface);
        // Add the corresponding interface for the the adjacent partition
        //  as well as the meta info so we don't repeat any interfaces!
        transientInterface.dir = opposingInterfaceDir;
        transientInterface.voxelLeftCol += edgeNeighborOffset.x;
        transientInterface.voxelBottomRow += edgeNeighborOffset.y;
        transientInterface.partitionIndexOther = partitionIndex;
        partitions[edgeNeighborVoxel.partitionIndex].interfaces.push_back(transientInterface);
        addPartitionInterfaceMeta(transientInterface);
        numInterfaces += 2;
    };
    auto processPartitionEdge = [&](Map::Partition& partition,
        const sf::Vector2i& partitionEdgeVoxelIndex,
        const sf::Vector2i& edgeNeighborOffset,
        PartitionInterface::Direction interfaceDir,
        PartitionInterface::Direction opposingInterfaceDir,
        PartitionInterface& transientInterface,
        int& transientNeighborPartitionIndex)->void
    {
        const sf::Vector2i edgeNeighborIndex = partitionEdgeVoxelIndex + edgeNeighborOffset;
        if (edgeNeighborIndex.x < 0 ||
            edgeNeighborIndex.x >= int(voxelCols) ||
            edgeNeighborIndex.y < 0 ||
            edgeNeighborIndex.y >= int(voxelRows))
        {
            return;
        }
        Map::VoxelMeta& edgeVoxel = voxelMeta[partitionEdgeVoxelIndex.y][partitionEdgeVoxelIndex.x];
        Map::VoxelMeta& edgeNeighborVoxel = voxelMeta[edgeNeighborIndex.y][edgeNeighborIndex.x];
        if (edgeNeighborVoxel.partitionIndex >= 0)
        {
            uint8_t iFlags = edgeVoxel.interfacedDirectionFlags;
            if (!(iFlags & (1 << int(interfaceDir))))
            {
                if (edgeNeighborVoxel.partitionIndex == transientNeighborPartitionIndex)
                {
                    // extend the size of our transient interface //
                    if (edgeNeighborOffset.x != 0)
                    {
                        transientInterface.voxelH++;
                    }
                    else
                    {
                        transientInterface.voxelW++;
                    }
                }
                else
                {
                    // if we have been building an interface already, 
                    //  add the previous interface to our list
                    if (transientNeighborPartitionIndex >= 0)
                    {
                        addTransientInterface(partition,
                            transientInterface, 
                            opposingInterfaceDir, 
                            edgeNeighborOffset,
                            edgeVoxel.partitionIndex);
                    }
                    transientNeighborPartitionIndex = edgeNeighborVoxel.partitionIndex;
                    // in any case, reset our transient interface object
                    transientInterface = { interfaceDir,
                        unsigned(partitionEdgeVoxelIndex.y),
                        unsigned(partitionEdgeVoxelIndex.x), 1,1 };
                }
            }
        }
    };
    size_t partitionIndex = 0;
    for (auto& partition : partitions)
    {
        const unsigned partitionTop = partition.voxelRow + partition.voxelH;
        const unsigned partitionRight = partition.voxelCol + partition.voxelW;
        // Search along the left & right sides to find partition interfaces //
        PartitionInterface transientInterfaceLeft;
        int partitionIndexLeft = -1;
        PartitionInterface transientInterfaceRight;
        int partitionIndexRight = -1;
        for (unsigned r = partition.voxelRow; r < partitionTop; r++)
        {
            // Left Side... //
            processPartitionEdge(partition,
                {int(partition.voxelCol), int(r)}, { -1, 0 },
                PartitionInterface::Direction::LEFT,
                PartitionInterface::Direction::RIGHT,
                transientInterfaceLeft, partitionIndexLeft);
            // Right Side... //
            processPartitionEdge(partition,
                { int(partitionRight - 1), int(r) }, { 1, 0 },
                PartitionInterface::Direction::RIGHT,
                PartitionInterface::Direction::LEFT,
                transientInterfaceRight, partitionIndexRight);
        }
        // Add the last transient interfaces, as long as we have found one
        if (partitionIndexLeft >= 0)
        {
            addTransientInterface(partition, 
                transientInterfaceLeft, 
                PartitionInterface::Direction::RIGHT,
                { -1, 0 },
                partitionIndex);
        }
        if (partitionIndexRight >= 0)
        {
            addTransientInterface(partition,
                transientInterfaceRight,
                PartitionInterface::Direction::LEFT,
                { 1,0 },
                partitionIndex);
        }
        // Search along the top and bottom sides to find partition interfaces //
        PartitionInterface transientInterfaceTop;
        int partitionIndexTop = -1;
        PartitionInterface transientInterfaceBottom;
        int partitionIndexBottom = -1;
        for (unsigned c = partition.voxelCol; c < partitionRight; c++)
        {
            // Top side... //
            processPartitionEdge(partition,
            { int(c), int(partitionTop - 1) }, { 0, 1 },
                PartitionInterface::Direction::UP,
                PartitionInterface::Direction::DOWN,
                transientInterfaceTop, partitionIndexTop);
            // Bottom side... //
            processPartitionEdge(partition,
            { int(c), int(partition.voxelRow) }, { 0, -1 },
                PartitionInterface::Direction::DOWN,
                PartitionInterface::Direction::UP,
                transientInterfaceBottom, partitionIndexBottom);
        }
        // Add the last transient interfaces, as long as we have found one
        if (partitionIndexTop >= 0)
        {
            addTransientInterface(partition,
                transientInterfaceTop,
                PartitionInterface::Direction::DOWN,
                { 0,1 },
                partitionIndex);
        }
        if (partitionIndexBottom >= 0)
        {
            addTransientInterface(partition,
                transientInterfaceBottom,
                PartitionInterface::Direction::UP,
                { 0,-1 },
                partitionIndex);
        }
        partitionIndex++;
    }
    std::cout << "numInterfaces=" << numInterfaces << std::endl;
    return true;
}
void Map::draw(sf::RenderTarget & rt)
{
    rt.draw(vaTiles, sf::RenderStates(&texTileset));
    if (m_showVoxelGrid)
    {
        rt.draw(vaSimGridLines);
    }
    if (m_showPartitionMeta)
    {
        //rt.draw(vaSimPartitions);
        // Draw boxes around all the partitions //
        sf::RectangleShape rs;
        rs.setOutlineColor(sf::Color::Transparent);
        static const float OUTLINE_MARGIN = SIM_VOXEL_SPACING;
        for (const auto& partition : partitions)
        {
            const float pLeft = float(partition.voxelCol*SIM_VOXEL_SPACING);
            const float pRight = float((partition.voxelCol + partition.voxelW)*SIM_VOXEL_SPACING);
            const float pTop = float((partition.voxelRow + partition.voxelH)*SIM_VOXEL_SPACING);
            const float pBottom = float(partition.voxelRow*SIM_VOXEL_SPACING);
            rs.setSize({pRight - pLeft, pTop - pBottom});
            rs.setOrigin({ 0,rs.getSize().y });
            rs.setPosition({ pLeft, pTop });
            rs.setFillColor(sf::Color(0, 255, 255,64));
            rt.draw(rs);
            rs.setSize({ pRight - pLeft - (2* SIM_VOXEL_SPACING), 
                pTop - pBottom - (2* SIM_VOXEL_SPACING) });
            rs.setOrigin({ 0,rs.getSize().y });
            rs.setPosition({ pLeft + SIM_VOXEL_SPACING, pTop - SIM_VOXEL_SPACING });
            rs.setFillColor(sf::Color(0,0,0,128));
            rt.draw(rs);
        }
        // Draw our partition interfaces //
        sf::VertexArray vaInterfaceArrows[4];
        vaInterfaceArrows[size_t(PartitionInterface::Direction::UP)] = sf::VertexArray(sf::PrimitiveType::Triangles, 3);
        vaInterfaceArrows[size_t(PartitionInterface::Direction::UP)][0].position = {-SIM_VOXEL_SPACING /4,0};
        vaInterfaceArrows[size_t(PartitionInterface::Direction::UP)][1].position = {SIM_VOXEL_SPACING /4,0};
        vaInterfaceArrows[size_t(PartitionInterface::Direction::UP)][2].position = {0,SIM_VOXEL_SPACING /2};
        vaInterfaceArrows[size_t(PartitionInterface::Direction::DOWN)] = sf::VertexArray(sf::PrimitiveType::Triangles, 3);
        vaInterfaceArrows[size_t(PartitionInterface::Direction::DOWN)][0].position = {-SIM_VOXEL_SPACING /4,0};
        vaInterfaceArrows[size_t(PartitionInterface::Direction::DOWN)][1].position = {SIM_VOXEL_SPACING /4,0};
        vaInterfaceArrows[size_t(PartitionInterface::Direction::DOWN)][2].position = {0,-SIM_VOXEL_SPACING /2};
        vaInterfaceArrows[size_t(PartitionInterface::Direction::LEFT)] = sf::VertexArray(sf::PrimitiveType::Triangles, 3);
        vaInterfaceArrows[size_t(PartitionInterface::Direction::LEFT)][0].position = {0,SIM_VOXEL_SPACING /4};
        vaInterfaceArrows[size_t(PartitionInterface::Direction::LEFT)][1].position = {0,-SIM_VOXEL_SPACING /4};
        vaInterfaceArrows[size_t(PartitionInterface::Direction::LEFT)][2].position = {-SIM_VOXEL_SPACING /2,0};
        vaInterfaceArrows[size_t(PartitionInterface::Direction::RIGHT)] = sf::VertexArray(sf::PrimitiveType::Triangles, 3);
        vaInterfaceArrows[size_t(PartitionInterface::Direction::RIGHT)][0].position = {0,SIM_VOXEL_SPACING /4};
        vaInterfaceArrows[size_t(PartitionInterface::Direction::RIGHT)][1].position = {0,-SIM_VOXEL_SPACING /4};
        vaInterfaceArrows[size_t(PartitionInterface::Direction::RIGHT)][2].position = {SIM_VOXEL_SPACING /2,0};
        for (size_t c = 0; c < 4; c++)
        {
            for (size_t v = 0; v < 3; v++)
            {
                vaInterfaceArrows[c][v].color = sf::Color(255, 128, 0, 64);
            }
        }
        for (const auto& partition : partitions)
        {
            for (const auto& interface : partition.interfaces)
            {
                const float iLeft = float(interface.voxelLeftCol*SIM_VOXEL_SPACING);
                const float iRight = float((interface.voxelLeftCol + interface.voxelW)*SIM_VOXEL_SPACING);
                const float iTop = float((interface.voxelBottomRow + interface.voxelH)*SIM_VOXEL_SPACING);
                const float iBottom = float(interface.voxelBottomRow*SIM_VOXEL_SPACING);
                rs.setSize({ iRight - iLeft, iTop - iBottom });
                rs.setOrigin({ 0,rs.getSize().y });
                rs.setPosition({ iLeft, iTop });
                rs.setFillColor(sf::Color(255, 128, 0, 64));
                rt.draw(rs);
            }
        }
        /// TODO: draw a line from each partition to its neighbor via partitionIndexOther
        /// so I can actually tell where the fuck they are actually going to read data from
    }
}
void Map::stepSimulation()
{
    // Update modes within each partition using equation (8) //
    // Transform modes to pressure values via IDCT //
    // Compute & accumulate forcing terms at each cell.
    //  for cells at interfaces, use equation (9),
    //  and for cells with point sources, use the sample value //
    // Transform forcing terms back to modal space via DCT //
}
void Map::toggleVoxelGrid()
{
    m_showVoxelGrid = !m_showVoxelGrid;
}
void Map::togglePartitionMeta()
{
    m_showPartitionMeta = !m_showPartitionMeta;
}
void Map::nullify()
{
    partitions.clear();
    voxelMeta.clear();
}
Map::VoxelMeta::VoxelMeta(int partitionIndex, uint8_t interfacedDirs)
    :partitionIndex(partitionIndex)
    ,interfacedDirectionFlags(interfacedDirs)
{
}
Map::Partition::Partition(unsigned rowBottom, unsigned colLeft, unsigned w, unsigned h)
    :voxelRow(rowBottom)
    ,voxelCol(colLeft)
    ,voxelW(w)
    ,voxelH(h)
{
    const size_t sizeComplex = (voxelW*voxelH) / 2 + 1;
    const size_t sizeReal = voxelW*voxelH;
    voxelModes = new fftw_complex[sizeComplex];
    for (size_t c = 0; c < sizeComplex; c++)
    {
        voxelModes[c][0] = voxelModes[c][1] = 0;
    }
    voxelPressures = new double[sizeReal];
    for (size_t c = 0; c < sizeReal; c++)
    {
        voxelPressures[c] = 0;
    }
}
Map::Partition::Partition(const Partition & other)
    :voxelRow(other.voxelRow)
    ,voxelCol(other.voxelCol)
    ,voxelW(other.voxelW)
    ,voxelH(other.voxelH)
    ,interfaces(other.interfaces)
{
    const size_t sizeComplex = (voxelW*voxelH) / 2 + 1;
    const size_t sizeReal = voxelW*voxelH;
    voxelModes = new fftw_complex[sizeComplex];
    for (size_t c = 0; c < sizeComplex; c++)
    {
        voxelModes[c][0] = other.voxelModes[c][0];
        voxelModes[c][1] = other.voxelModes[c][1];
    }
    voxelPressures = new double[sizeReal];
    for (size_t c = 0; c < sizeReal; c++)
    {
        voxelPressures[c] = other.voxelPressures[c];
    }
}
Map::Partition::~Partition()
{
    if (voxelModes) delete[] voxelModes;
    if (voxelPressures) delete[] voxelPressures;
}