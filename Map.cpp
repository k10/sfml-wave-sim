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
    if (!loadJsonMap(jsonMapFilename))
    {
        return false;
    }
    if (!loadTileset(jsonMapFilename))
    {
        return false;
    }
    buildMapTileVBO();
    buildVoxelGridVBO();
    buildVoxelPressureVBO();
    decomposeVoxelsIntoPartitions();
    buildPartitionVBO();
    calculatePartitionInterfaces();
    buildInterfaceVBO();
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
        rt.draw(vaSimPartitions);
        rt.draw(vaSimPartitionInterfaces);
        /// TODO: draw a line from each partition to its neighbor via partitionIndexOther
        /// so I can actually tell where the fuck they are actually going to read data from
    }
    rt.draw(vaSimGridPressures);
}
void Map::stepSimulation()
{
    // Update modes within each partition using equation (8) //
    for (auto& partition : partitions)
    {
        for (size_t y = 0; y < partition.voxelH; y++)
        {
            for (size_t x = 0; x < partition.voxelW; x++)
            {
                const size_t i = y*partition.voxelW + x;
                // first, we need to calculate omega[i] //
                ///TODO: precompute these terms
                ///TODO: use world-space to compute "k" instead of local index space?..
                /// (does it even actually matter?..)
                const double k_i_2 = pow(PI, 2)*
                    (pow(x,2)/pow(partition.voxelW,2) +
                     pow(y,2)/pow(partition.voxelH,2));
                const double k_i = sqrt(k_i_2);
                const double omega_i = SOUND_SPEED_METERS_PER_SECOND*k_i;
                // then, accumulate the modal terms //
                ///TODO: pecompute the cosTerms as well
                const double cosTerm = cos(omega_i*SIM_DELTA_TIME);
                const double currMode = partition.voxelModes[i];
                const double forcingModalComponent =
                    (2 * partition.voxelForcingTerms[i] / pow(omega_i, 2))*(1 - cosTerm);
                assert(!_isnan(currMode));
                // Equation (8):
                partition.voxelModes[i] =
                    2 * currMode*cosTerm -
                    partition.voxelModesPrevious[i] +
                    ///TODO: figure out why this is fucked probably?
                    (omega_i > 0 ? forcingModalComponent : 0);//some bullshit right here
                assert(!_isnan(partition.voxelModes[i]));
                partition.voxelModesPrevious[i] = currMode;
            }
        }
    }
    // Transform modes to pressure values via IDCT //
    for (auto& partition : partitions)
    {
        fftw_execute(partition.planModeToPressure);
        // normalize the iDCT result by dividing each cell by 2*size //
        ///TODO: figure out if I even need this???
        //const double normalization = 2 * partition.voxelH * 2 * partition.voxelW;
        //for (size_t y = 0; y < partition.voxelH; y++)
        //{
        //    for (size_t x = 0; x < partition.voxelW; x++)
        //    {
        //        const size_t i = y*partition.voxelW + x;
        //        partition.voxelPressures[i] /= normalization;
        //    }
        //}
    }
    // Compute & accumulate forcing terms at each cell.
    //  for cells at interfaces, use equation (9),
    //  and for cells with point sources, use the sample value //
    for (auto& partition : partitions)
    {
        for (size_t y = 0; y < partition.voxelH; y++)
        {
            for (size_t x = 0; x < partition.voxelW; x++)
            {
                // zero out the forcing terms first //
                const size_t i = y*partition.voxelW + x;
                partition.voxelForcingTerms[i] = 0;
                // While we're at it, let's update the visuals for grid pressures //
                const size_t globalGridX = partition.voxelCol + x;
                const size_t globalGridY = partition.voxelRow + y;
                const size_t v = globalGridY*voxelCols + globalGridX;
                const size_t vLocal = y*partition.voxelW + x;
                /// TODO: figure out wtf this even should be?? and wtf does it mean??
                static const double MAX_PRESSURE_MAGNITUDE = 0.0005;
                const double alphaPercent =
                    std::min(abs(partition.voxelPressures[vLocal]) / MAX_PRESSURE_MAGNITUDE, 1.0);
                const sf::Uint8 alpha = sf::Uint8(alphaPercent * 255);
                sf::Color color = partition.voxelPressures[vLocal] > 0 ?
                    sf::Color(0, 0, 255, alpha) : sf::Color(255, 0, 0, alpha);
                if (_isnan(partition.voxelPressures[vLocal]))
                {
                    color = sf::Color::Green;
                }
                for (unsigned i = 0; i < 4; i++)
                {
                    vaSimGridPressures[4 * v + i].color = color;
                }
            }
        }
        static const sf::Vector2i DIRECTION_VECS[] = {
            {0,1}, {0,-1}, {-1,0}, {1,0}
        };
        for (auto& iFace : partition.interfaces)
        {
            const unsigned iFaceRight = iFace.voxelLeftCol + iFace.voxelW;
            const unsigned iFaceTop = iFace.voxelBottomRow + iFace.voxelH;
            const sf::Vector2i& iFaceDirection = DIRECTION_VECS[size_t(iFace.dir)];
            for (unsigned x = iFace.voxelLeftCol; x < iFaceRight; x++)
            {
                for (unsigned y = iFace.voxelBottomRow; y < iFaceTop; y++)
                {
                    const sf::Vector2i i{ int(x),int(y) };
                    double pressureStencil = 0;
                    static const double STENCIL_WEIGHTS[] = {
                        -2, 27, -270, 270, -27, 2
                    };
                    for (int di = -2; di <= 3; di++)
                    {
                        const sf::Vector2i stencil_i = i + iFaceDirection*di;
                        if (stencil_i.x < 0 || stencil_i.x >= int(voxelCols) ||
                            stencil_i.y < 0 || stencil_i.y >= int(voxelRows))
                        {
                            // Just discard parts of the stencil that lie out of bounds??...
                            continue;
                        }
                        double* pPressure = globalPressureLookupTable[stencil_i.y][stencil_i.x];
                        if (!pPressure)
                        {
                            // Just discard parts of the stencil that are outside partitions??...
                            continue;
                        }
                        assert(!_isnan(*pPressure));
                        pressureStencil += STENCIL_WEIGHTS[di + 2] * (*pPressure);
                    }
                    unsigned partitionVoxelX = x - partition.voxelCol;
                    unsigned partitionVoxelY = y - partition.voxelRow;
                    const size_t partitionI = partitionVoxelY*partition.voxelW + partitionVoxelX;
                    // Equation (9): (hopefully?..)
                    partition.voxelForcingTerms[partitionI] = pow(SOUND_SPEED_METERS_PER_SECOND, 2)*
                        (1.0 / 180 * pow(SIM_VOXEL_SPACING,2))*pressureStencil;
                    assert(!_isnan(partition.voxelForcingTerms[partitionI]));
                }
            }
        }
        // if this partition has an active point-source, apply its pressure value //
        if (partition.ps.timeLeft > 0)
        {
            partition.voxelForcingTerms[partition.ps.voxelIndex] = partition.ps.step();
            assert(!_isnan(partition.voxelForcingTerms[partition.ps.voxelIndex]));
        }
    }
    // Transform forcing terms back to modal space via DCT //
    for (auto& partition : partitions)
    {
        fftw_execute(partition.planForcingToModes);
    }
}
void Map::toggleVoxelGrid()
{
    m_showVoxelGrid = !m_showVoxelGrid;
}
void Map::togglePartitionMeta()
{
    m_showPartitionMeta = !m_showPartitionMeta;
}
void Map::touch(const sf::Vector2f & worldSpaceLocation)
{
    std::cout << "worldSpaceLocation=" << worldSpaceLocation<<std::endl;
    // first, we need to find out which partition we're in, if any //
    for (auto& partition : partitions)
    {
        const float pLeft = float(partition.voxelCol*SIM_VOXEL_SPACING);
        const float pRight = float((partition.voxelCol + partition.voxelW)*SIM_VOXEL_SPACING);
        const float pTop = float((partition.voxelRow + partition.voxelH)*SIM_VOXEL_SPACING);
        const float pBottom = float(partition.voxelRow*SIM_VOXEL_SPACING);
        if (worldSpaceLocation.x >= pLeft && worldSpaceLocation.x < pRight &&
            worldSpaceLocation.y >= pBottom && worldSpaceLocation.y < pTop)
        {
            std::cout << "\tpartition[x,y]=[" << partition.voxelCol << "," << partition.voxelRow << "]\n";
            std::cout << "\tpartition[w,h]=[" << partition.voxelW << "," << partition.voxelH << "]\n";
            // next, we need to update the simulation to assign 
            //  a forcing term at this cell during the simulation's step //
            const sf::Vector2f localPosition = worldSpaceLocation - sf::Vector2f{ pLeft,pBottom };
            const size_t localGridX = size_t(localPosition.x / SIM_VOXEL_SPACING);
            const size_t localGridY = size_t(localPosition.y / SIM_VOXEL_SPACING);
            const size_t i = localGridY*partition.voxelW + localGridX;
            partition.ps = {i, SIM_DELTA_TIME , PointSource::Type::CLICK};
            std::cout << "\t added a click! pressure="<<partition.voxelPressures[i]<<"\n";
            return;
        }
    }
}
bool Map::loadJsonMap(const std::string& jsonMapFilename)
{
    std::ifstream fileJsonMap(jsonMapFilename);
    if (fileJsonMap.is_open())
    {
        fileJsonMap >> jsonMap;
    }
    else
    {
        std::cerr << "ERROR: could not open \"" << jsonMapFilename << "\"\n";
        return false;
    }
    return true;
}
bool Map::loadTileset(const std::string& jsonMapFilename)
{
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
    return true;
}
void Map::buildMapTileVBO()
{
    tilePixW = jsonMap["tilesets"][0]["tilewidth"];
    tilePixH = jsonMap["tilesets"][0]["tileheight"];
    tilesetColumns = jsonMap["tilesets"][0]["columns"];
    mapRows = jsonMap["layers"][0]["height"];
    mapCols = jsonMap["layers"][0]["width"];
    mapPixelHeight = float(mapRows);// *tilePixH);
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
            vaTiles[4 * tileArrayIndex + 0].position = { float(c * 1),    float(mapPixelHeight - (r) * 1) };
            vaTiles[4 * tileArrayIndex + 1].position = { float((c + 1) * 1),float(mapPixelHeight - (r) * 1) };
            vaTiles[4 * tileArrayIndex + 2].position = { float((c + 1) * 1),float(mapPixelHeight - (r + 1) * 1) };
            vaTiles[4 * tileArrayIndex + 3].position = { float(c * 1),    float(mapPixelHeight - (r + 1) * 1) };
            const int TILESET_ROW = tileId / tilesetColumns;
            const int TILESET_COL = tileId % tilesetColumns;
            const sf::Vector2f TILE_UPPER_LEFT(float(TILESET_COL*tilePixW), float(TILESET_ROW*tilePixH));
            vaTiles[4 * tileArrayIndex + 0].texCoords = TILE_UPPER_LEFT + sf::Vector2f{ 0.f,0.f };
            vaTiles[4 * tileArrayIndex + 1].texCoords = TILE_UPPER_LEFT + sf::Vector2f{ float(tilePixW),0.f };
            vaTiles[4 * tileArrayIndex + 2].texCoords = TILE_UPPER_LEFT + sf::Vector2f{ float(tilePixW),float(tilePixH) };
            vaTiles[4 * tileArrayIndex + 3].texCoords = TILE_UPPER_LEFT + sf::Vector2f{ 0.f,float(tilePixH) };
        }
    }
}
void Map::buildVoxelGridVBO()
{
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
}
void Map::buildVoxelPressureVBO()
{
    vaSimGridPressures = sf::VertexArray(sf::PrimitiveType::Quads, voxelRows*voxelCols * 4);
    for (size_t v = 0; v < voxelRows*voxelCols; v++)
    {
        const unsigned gridX = v % voxelCols;
        const unsigned gridY = v / voxelCols;
        const float left = gridX*SIM_VOXEL_SPACING;
        const float right = (gridX + 1)*SIM_VOXEL_SPACING;
        const float bottom = gridY*SIM_VOXEL_SPACING;
        const float top = (gridY + 1)*SIM_VOXEL_SPACING;
        vaSimGridPressures[4 * v + 0].position = { left, bottom };
        vaSimGridPressures[4 * v + 1].position = { right, bottom };
        vaSimGridPressures[4 * v + 2].position = { right, top };
        vaSimGridPressures[4 * v + 3].position = { left, top };
        for (size_t i = 0; i < 4; i++)
        {
            vaSimGridPressures[4 * v + i].color = sf::Color::Transparent;
        }
    }
}
void Map::decomposeVoxelsIntoPartitions()
{
    globalPressureLookupTable.clear();
    globalPressureLookupTable.resize(voxelRows, std::vector<double*>(voxelCols, nullptr));
    voxelMeta.clear();
    voxelMeta.resize(voxelRows, std::vector<VoxelMeta>(voxelCols));
    auto checkNextPartitionRow = [&](unsigned partitionBottomRow, unsigned partitionLeftCol,
        unsigned currPartitionW, unsigned currPartitionH)->bool
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
                mapPixelHeight - (vr + 0.5f)*SIM_VOXEL_SPACING);
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
                mapPixelHeight - (vr + 0.5f)*SIM_VOXEL_SPACING);
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
                mapPixelHeight - (r + 0.5f)*SIM_VOXEL_SPACING);
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
    // add all partition's pressure pointers to the globalPressureLookupTable //
    //  need to do this after they have all been allocated
    //  because memory addresses will change if the vector gets resized I think!!!
    for (auto& partition : partitions)
    {
        for (size_t y = 0; y < partition.voxelH; y++)
        {
            for (size_t x = 0; x < partition.voxelW; x++)
            {
                const size_t i = y*partition.voxelW + x;
                globalPressureLookupTable[partition.voxelRow + y][partition.voxelCol + x] =
                    &(partition.voxelPressures[i]);
            }
        }
    }
}
void Map::buildPartitionVBO()
{
    vaSimPartitions = sf::VertexArray(sf::PrimitiveType::Quads, 4 * 4 * partitions.size());
    for (size_t p = 0; p < partitions.size(); p++)
    {
        static const sf::Color color(0, 255, 255, 64);
        static const float OUTLINE_SIZE = SIM_VOXEL_SPACING*0.5f;
        const auto& partition = partitions[p];
        const float pLeft = float(partition.voxelCol*SIM_VOXEL_SPACING);
        const float pRight = float((partition.voxelCol + partition.voxelW)*SIM_VOXEL_SPACING);
        const float pTop = float((partition.voxelRow + partition.voxelH)*SIM_VOXEL_SPACING);
        const float pBottom = float(partition.voxelRow*SIM_VOXEL_SPACING);
        // left side //
        vaSimPartitions[4 * 4 * p + 0].position = { pLeft, pBottom };
        vaSimPartitions[4 * 4 * p + 1].position = { pLeft + OUTLINE_SIZE, pBottom };
        vaSimPartitions[4 * 4 * p + 2].position = { pLeft + OUTLINE_SIZE, pTop };
        vaSimPartitions[4 * 4 * p + 3].position = { pLeft, pTop };
        // right side //
        vaSimPartitions[4 * 4 * p + 4].position = { pRight - OUTLINE_SIZE, pBottom };
        vaSimPartitions[4 * 4 * p + 5].position = { pRight, pBottom };
        vaSimPartitions[4 * 4 * p + 6].position = { pRight, pTop };
        vaSimPartitions[4 * 4 * p + 7].position = { pRight - OUTLINE_SIZE, pTop };
        // top side //
        vaSimPartitions[4 * 4 * p + 8].position = { pLeft + OUTLINE_SIZE, pTop - OUTLINE_SIZE };
        vaSimPartitions[4 * 4 * p + 9].position = { pRight - OUTLINE_SIZE, pTop - OUTLINE_SIZE };
        vaSimPartitions[4 * 4 * p + 10].position = { pRight - OUTLINE_SIZE, pTop };
        vaSimPartitions[4 * 4 * p + 11].position = { pLeft + OUTLINE_SIZE, pTop };
        // bottom side //
        vaSimPartitions[4 * 4 * p + 12].position = { pLeft + OUTLINE_SIZE, pBottom };
        vaSimPartitions[4 * 4 * p + 13].position = { pRight - OUTLINE_SIZE, pBottom };
        vaSimPartitions[4 * 4 * p + 14].position = { pRight - OUTLINE_SIZE, pBottom + OUTLINE_SIZE };
        vaSimPartitions[4 * 4 * p + 15].position = { pLeft + OUTLINE_SIZE, pBottom + OUTLINE_SIZE };
        for (size_t i = 0; i < 4 * 4; i++)
        {
            vaSimPartitions[4 * 4 * p + i].color = color;
        }
    }
}
void Map::calculatePartitionInterfaces()
{
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
    numInterfaces = 0;
    auto addTransientInterface = [&](Map::Partition& partition,
        PartitionInterface& transientInterface,
        PartitionInterface::Direction opposingInterfaceDir,
        const sf::Vector2i& edgeNeighborOffset,
        size_t partitionIndex)->void
    {
        const sf::Vector2i interfaceBaseVoxelIndex(
            transientInterface.voxelLeftCol,
            transientInterface.voxelBottomRow);
        const sf::Vector2i edgeNeighborIndex = interfaceBaseVoxelIndex + edgeNeighborOffset;
        Map::VoxelMeta& edgeNeighborVoxel = voxelMeta[edgeNeighborIndex.y][edgeNeighborIndex.x];
        //transientInterface.partitionIndexOther = size_t(edgeNeighborVoxel.partitionIndex);
        partition.interfaces.push_back(transientInterface);
        addPartitionInterfaceMeta(transientInterface);
        // Add the corresponding interface for the the adjacent partition
        //  as well as the meta info so we don't repeat any interfaces!
        transientInterface.dir = opposingInterfaceDir;
        transientInterface.voxelLeftCol += edgeNeighborOffset.x;
        transientInterface.voxelBottomRow += edgeNeighborOffset.y;
        //transientInterface.partitionIndexOther = partitionIndex;
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
            { int(partition.voxelCol), int(r) }, { -1, 0 },
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
}
void Map::buildInterfaceVBO()
{
    vaSimPartitionInterfaces = sf::VertexArray(sf::PrimitiveType::Quads, 4 * numInterfaces);
    unsigned currInterface = 0;
    for (const auto& partition : partitions)
    {
        for (const auto& interface : partition.interfaces)
        {
            static const sf::Color color(255, 128, 0, 64);
            const float iLeft = float(interface.voxelLeftCol*SIM_VOXEL_SPACING);
            const float iRight = float((interface.voxelLeftCol + interface.voxelW)*SIM_VOXEL_SPACING);
            const float iTop = float((interface.voxelBottomRow + interface.voxelH)*SIM_VOXEL_SPACING);
            const float iBottom = float(interface.voxelBottomRow*SIM_VOXEL_SPACING);
            vaSimPartitionInterfaces[4 * currInterface + 0].position = { iLeft, iBottom };
            vaSimPartitionInterfaces[4 * currInterface + 1].position = { iRight, iBottom };
            vaSimPartitionInterfaces[4 * currInterface + 2].position = { iRight, iTop };
            vaSimPartitionInterfaces[4 * currInterface + 3].position = { iLeft, iTop };
            for (unsigned i = 0; i < 4; i++)
            {
                vaSimPartitionInterfaces[4 * currInterface + i].color = color;
            }
            currInterface++;
        }
    }
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
    const size_t gridSize = voxelW*voxelH;
    voxelModes = new double[gridSize];
    voxelModesPrevious = new double[gridSize];
    voxelForcingTerms = new double[gridSize];
    voxelPressures = new double[gridSize];
    for (size_t c = 0; c < gridSize; c++)
    {
        //voxelModes[c][0] = voxelModes[c][1] = 0;
        //voxelModesPrevious[c][0] = voxelModesPrevious[c][1] = 0;
        //voxelPressures[c][0] = voxelPressures[c][1] = 0;
        voxelModes[c] = 0;
        voxelModesPrevious[c] = 0;
        voxelForcingTerms[c] = 0;
        voxelPressures[c] = 0;
    }
    planModeToPressure = fftw_plan_r2r_2d(voxelH, voxelW,
        voxelModes, voxelPressures,
        FFTW_REDFT01, FFTW_REDFT01, FFTW_ESTIMATE);
    planForcingToModes = fftw_plan_r2r_2d(voxelH, voxelW,
        voxelForcingTerms, voxelForcingTerms,
        FFTW_REDFT10, FFTW_REDFT10, FFTW_ESTIMATE);
}
Map::Partition::Partition(const Partition & other)
    :voxelRow(other.voxelRow)
    ,voxelCol(other.voxelCol)
    ,voxelW(other.voxelW)
    ,voxelH(other.voxelH)
    ,interfaces(other.interfaces)
{
    const size_t gridSize = voxelW*voxelH;
    voxelModes = new double[gridSize];
    voxelModesPrevious = new double[gridSize];
    voxelForcingTerms = new double[gridSize];
    voxelPressures = new double[gridSize];
    for (size_t c = 0; c < gridSize; c++)
    {
        //for (size_t i = 0; i < 2; i++)
        //{
        //    voxelModes[c][i] = other.voxelModes[c][i];
        //    voxelModesPrevious[c][i] = other.voxelModesPrevious[c][i];
        //    voxelPressures[c][i] = other.voxelPressures[c][i];
        //}
        voxelModes[c] = other.voxelModes[c];
        voxelModesPrevious[c] = other.voxelModesPrevious[c];
        voxelForcingTerms[c] = other.voxelModesPrevious[c];
        voxelPressures[c] = other.voxelPressures[c];
    }
    planModeToPressure = fftw_plan_r2r_2d(voxelH, voxelW,
        voxelModes, voxelPressures,
        FFTW_REDFT01, FFTW_REDFT01, FFTW_ESTIMATE);
    planForcingToModes = fftw_plan_r2r_2d(voxelH, voxelW,
        voxelForcingTerms, voxelForcingTerms,
        FFTW_REDFT10, FFTW_REDFT10, FFTW_ESTIMATE);
}
Map::Partition::~Partition()
{
    if (voxelModes) delete[] voxelModes;
    if (voxelModesPrevious) delete[] voxelModesPrevious;
    if (voxelForcingTerms) delete[] voxelForcingTerms;
    if (voxelPressures) delete[] voxelPressures;
}
Map::PointSource::PointSource(size_t voxelIndex, float time, Type t)
    :voxelIndex(voxelIndex)
    ,type(t)
    ,timeLeft(time)
    ,totalTime(time)
{
}
double Map::PointSource::step()
{
    timeLeft -= SIM_DELTA_TIME;
    switch (type)
    {
    case PointSource::Type::CLICK:
        std::cout << "\tclick stepped!\n";
        return 1.0;///WTF does this even mean?..  what units are  these?..
    case PointSource::Type::GAUSIAN_PULSE:
        ///TODO: calculate a broadband gausian pulse of unit amplitude or w/e
        /// Kinda like this: http://www.gaussianwaves.com/2014/07/generating-basic-signals-gaussian-pulse-and-power-spectral-density-using-fft/
    default:
        return 0;
    }
}