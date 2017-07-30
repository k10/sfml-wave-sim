#include "Map.h"
#include "toolbox.h"
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
    std::cout << "strTilesetFilename=" << strTilesetFilename << std::endl;
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
    const float MAP_PIX_H = float(mapRows*tilePixH);
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
            vaTiles[4 * tileArrayIndex + 0].position = {float(c*tilePixW),float(MAP_PIX_H - r*tilePixH)};
            vaTiles[4 * tileArrayIndex + 1].position = { float((c+1)*tilePixW),float(MAP_PIX_H - r*tilePixH)};
            vaTiles[4 * tileArrayIndex + 2].position = { float((c+1)*tilePixW),float(MAP_PIX_H - (r+1)*tilePixH) };
            vaTiles[4 * tileArrayIndex + 3].position = { float(c*tilePixW),float(MAP_PIX_H - (r+1)*tilePixH)};
            const int TILESET_ROW = tileId / tilesetColumns;
            const int TILESET_COL = tileId % tilesetColumns;
            const sf::Vector2f TILE_UPPER_LEFT(float(TILESET_COL*tilePixW), float(TILESET_ROW*tilePixH));
            vaTiles[4 * tileArrayIndex + 0].texCoords = TILE_UPPER_LEFT + sf::Vector2f{0.f,0.f};
            vaTiles[4 * tileArrayIndex + 1].texCoords = TILE_UPPER_LEFT + sf::Vector2f{ float(tilePixW),0.f};
            vaTiles[4 * tileArrayIndex + 2].texCoords = TILE_UPPER_LEFT + sf::Vector2f{ float(tilePixW),float(tilePixH) };
            vaTiles[4 * tileArrayIndex + 3].texCoords = TILE_UPPER_LEFT + sf::Vector2f{0.f,float(tilePixH)};
        }
    }
    return true;
}
void Map::draw(sf::RenderTarget & rt)
{
    rt.draw(vaTiles, sf::RenderStates(&texTileset));
}