#pragma once
#include <SFML/Graphics.hpp>
#include <string>
#include <nlohmann\json.hpp>
using json = nlohmann::json;
#include <fstream>
class Map
{
public:
    // returns false if any loading steps fuck up, true if we gucci
    bool load(const std::string& jsonMapFilename);
    void draw(sf::RenderTarget& rt);
private:
    json jsonMap;
    sf::Texture texTileset;
    sf::VertexArray vaTiles;
    unsigned mapCols;
    unsigned mapRows;
    unsigned tilePixW;
    unsigned tilePixH;
    unsigned tilesetColumns;
};

