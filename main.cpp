#include <SFML/Audio.hpp>
#include <SFML/Graphics.hpp>
int main()
{
    sf::RenderWindow window(sf::VideoMode(800, 600), "SFML window");
    while (window.isOpen())
    {
        sf::Event event;
        while (window.pollEvent(event))
        {
            switch (event.type)
            {
            case sf::Event::KeyPressed:
                if (event.key.code == sf::Keyboard::Escape)
                {
                    window.close();
                }
                break;
            case sf::Event::Closed:
                window.close();
                break;
            }
        }
        window.clear();
        window.display();
    }
    return EXIT_SUCCESS;
}