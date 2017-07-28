#pragma once
#include <SFML/Graphics.hpp>
class Application
{
public:
    Application(sf::RenderWindow& rw);
    void onEvent(const sf::Event& e);
    void tick(const sf::Time& deltaTime);
private:
    void drawOrigin();
private:
    sf::RenderWindow& renderWindow;
    sf::View view;
    sf::Vector2f mouseLeftClickOrigin;
    sf::Vector2f mouseLeftClickCenterScreen;
    bool mouseHeldLeft;
};