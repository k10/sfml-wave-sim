#include <SFML/Graphics.hpp>
#include "Application.h"
int main()
{
    sf::RenderWindow window(sf::VideoMode(800, 600), "SFML window");
    Application app(window);
    sf::Clock frameTime;
    while (window.isOpen())
    {
        sf::Event event;
        while (window.pollEvent(event))
        {
            switch (event.type)
            {
            case sf::Event::Closed:
                window.close();
                continue;
            }
            app.onEvent(event);
        }
        window.clear();
        app.tick(frameTime.restart());
        window.display();
    }
    return EXIT_SUCCESS;
}