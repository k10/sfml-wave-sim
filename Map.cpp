#include "Map.h"
#include "toolbox.h"
const float Map::SOUND_SPEED_METERS_PER_SECOND = 340;
const float Map::MAXIMUM_SOUND_HZ = 2000;
const float Map::SIM_VOXEL_SPACING = SOUND_SPEED_METERS_PER_SECOND/(2*MAXIMUM_SOUND_HZ);
const float Map::SIM_DELTA_TIME = SIM_VOXEL_SPACING/(SOUND_SPEED_METERS_PER_SECOND*sqrtf(3));
bool Map::load(const std::string & jsonMapFilename)
{
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
            vaTiles[4 * tileArrayIndex + 0].position = {float(c*1),float(MAP_PIX_H - r*1)};
            vaTiles[4 * tileArrayIndex + 1].position = { float((c+1)*1),float(MAP_PIX_H - r*1)};
            vaTiles[4 * tileArrayIndex + 2].position = { float((c+1)*1),float(MAP_PIX_H - (r+1)*1) };
            vaTiles[4 * tileArrayIndex + 3].position = { float(c*1),float(MAP_PIX_H - (r+1)*1)};
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
    const unsigned GRID_ROWS = unsigned(mapRows / SIM_VOXEL_SPACING);
    const unsigned GRID_COLS = unsigned(mapCols / SIM_VOXEL_SPACING);
    vaSimGridLines = sf::VertexArray(sf::PrimitiveType::Lines, 2 * (GRID_ROWS + 1) + 2 * (GRID_COLS + 1));
    const float MAP_LEFT = 0;
    const float MAP_RIGHT = float(mapCols);
    for (unsigned r = 0; r < GRID_ROWS + 1; r++)
    {
        vaSimGridLines[2 * r + 0].position = { MAP_LEFT, float(r*SIM_VOXEL_SPACING) };
        vaSimGridLines[2 * r + 1].position = { MAP_RIGHT, float(r*SIM_VOXEL_SPACING) };
    }
    const float MAP_TOP = float(mapRows);
    const float MAP_BOTTOM = 0;
    for (unsigned c = 0; c < GRID_COLS + 1; c++)
    {
        vaSimGridLines[2 * (GRID_ROWS + 1) + 2 * c + 0].position = { float(c*SIM_VOXEL_SPACING), MAP_TOP };
        vaSimGridLines[2 * (GRID_ROWS + 1) + 2 * c + 1].position = { float(c*SIM_VOXEL_SPACING), MAP_BOTTOM };
    }
    // decompose simulation voxels into large rectangular partitions ////////////////////////////////////////////
    partitions.clear();
    std::vector<std::vector<bool>> decomposed(GRID_ROWS, std::vector<bool>(GRID_COLS, false));
    auto checkNextPartitionRow = [&](Partition& p)->bool
    {
        if (p.voxelRow + p.voxelH >= GRID_ROWS)
        {
            return false;
        }
        // we iterate along the +Y edge of the partition 
        //  and return true if all the voxels below are empty space
        for (unsigned c = 0; c < p.voxelW; c++)
        {
            const unsigned vc = p.voxelCol + c;
            const unsigned vr = p.voxelRow + p.voxelH;
            const sf::Vector2f worldPos((vc + 0.5f)*SIM_VOXEL_SPACING,
                MAP_PIX_H - (vr + 0.5f)*SIM_VOXEL_SPACING);
            // because our units are meters, and each map tile is 1m^s,
            //  we can just cast to ints to obtain map tile indexes:
            const unsigned mapRow = unsigned(worldPos.y);
            const unsigned mapCol = unsigned(worldPos.x);
            unsigned tileArrayIndex = mapRow*mapCols + mapCol;
            int tileId = jsonMap["layers"][0]["data"][tileArrayIndex];
            if (tileId > 0 || decomposed[vr][vc])
            {
                return false;
            }
        }
        return true;
    };
    auto checkNextPartitionCol = [&](Partition& p)->bool
    {
        if (p.voxelCol + p.voxelW >= GRID_COLS)
        {
            return false;
        }
        // we iterate along the +X edge of the partition 
        //  and return true if all the voxels below are empty space
        for (unsigned r = 0; r < p.voxelH; r++)
        {
            const unsigned vc = p.voxelCol + p.voxelW;
            const unsigned vr = p.voxelRow + r;
            const sf::Vector2f worldPos((vc + 0.5f)*SIM_VOXEL_SPACING,
                MAP_PIX_H - (vr + 0.5f)*SIM_VOXEL_SPACING);
            // because our units are meters, and each map tile is 1m^s,
            //  we can just cast to ints to obtain map tile indexes:
            const unsigned mapRow = unsigned(worldPos.y);
            const unsigned mapCol = unsigned(worldPos.x);
            unsigned tileArrayIndex = mapRow*mapCols + mapCol;
            int tileId = jsonMap["layers"][0]["data"][tileArrayIndex];
            if (tileId > 0 || decomposed[vr][vc])
            {
                return false;
            }
        }
        return true;
    };
    for (unsigned r = 0; r < GRID_ROWS; r++)
    {
        for (unsigned c = 0; c < GRID_COLS; c++)
        {
            const sf::Vector2f worldPos((c + 0.5f)*SIM_VOXEL_SPACING, 
                MAP_PIX_H - (r + 0.5f)*SIM_VOXEL_SPACING);
            // because our units are meters, and each map tile is 1m^s,
            //  we can just cast to ints to obtain map tile indexes:
            const unsigned mapRow = unsigned(worldPos.y);
            const unsigned mapCol = unsigned(worldPos.x);
            unsigned tileArrayIndex = mapRow*mapCols + mapCol;
            int tileId = jsonMap["layers"][0]["data"][tileArrayIndex];
            if (tileId > 0 || decomposed[r][c])
            {
                // every non-zero tile is considered solid
                //  as well as every previously decomposed voxel
                continue;
            }
            Partition partition = { r,c,1,1 };
            while (checkNextPartitionRow(partition)) 
            {
                partition.voxelH++; 
            }
            while (checkNextPartitionCol(partition)) 
            {
                partition.voxelW++;
            }
            // we need to mark the voxels in this partition as decomposed
            //  so they don't go into new partitions
            for (unsigned vr = partition.voxelRow; vr < partition.voxelRow + partition.voxelH; vr++)
            {
                for (unsigned vc = partition.voxelCol; vc < partition.voxelCol + partition.voxelW; vc++)
                {
                    decomposed[vr][vc] = true;
                }
            }
            partitions.push_back(partition);
        }
    }
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
    return true;
}
void Map::draw(sf::RenderTarget & rt)
{
    rt.draw(vaTiles, sf::RenderStates(&texTileset));
    //rt.draw(vaSimGridLines);
    //rt.draw(vaSimPartitions);
    // Draw boxes around all the partitions //
    sf::RectangleShape rs;
    rs.setFillColor(sf::Color::Transparent);
    rs.setOutlineColor(sf::Color::Cyan);
    rs.setOutlineThickness(0.05f);
    for (const auto& partition : partitions)
    {
        const float pLeft = float(partition.voxelCol*SIM_VOXEL_SPACING);
        const float pRight = float((partition.voxelCol + partition.voxelW)*SIM_VOXEL_SPACING);
        const float pTop = float((partition.voxelRow + partition.voxelH)*SIM_VOXEL_SPACING);
        const float pBottom = float(partition.voxelRow*SIM_VOXEL_SPACING);
        rs.setSize({pRight - pLeft, pTop - pBottom});
        rs.setOrigin({ 0,rs.getSize().y });
        rs.setPosition({ pLeft, pTop });
        rt.draw(rs);
    }
}