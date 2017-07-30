#pragma once
#include <SFML/Graphics.hpp>
#include "Map.h"
class Application
{
public:
    Application(sf::RenderWindow& rw, int argc, char** argv);
    void onEvent(const sf::Event& e);
    void tick(const sf::Time& deltaTime);
private:
    void drawOrigin();
    void updateViewSize();
private:
    sf::RenderWindow& renderWindow;
    sf::View view;
    sf::Vector2f mouseRightClickOrigin;
    sf::Vector2f mouseRightClickCenterScreen;
    bool mouseHeldRight;
    float zoomPercent;
    Map map;
};