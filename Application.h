#pragma once
#include <SFML/Graphics.hpp>
#include "Map.h"
class Application
{
private:
    static const float DEFAULT_ZOOM;
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
    sf::Vector2i mouseLeftClickPosition;
    sf::Vector2f mouseRightClickOrigin;
    sf::Vector2f mouseRightClickCenterScreen;
    bool mouseHeldLeft;
    bool mouseHeldRight;
    float zoomPercent;
    Map map;
};