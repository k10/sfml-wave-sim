#include "Application.h"
Application::Application(sf::RenderWindow & rw)
    :renderWindow(rw)
    ,view(rw.getDefaultView())
    , mouseHeldLeft(false)
{
    // Invert the camera's y-axis //
    view.setSize(
        float(renderWindow.getSize().x),
        renderWindow.getSize().y*-1.f);
    view.setCenter({ 0,0 });
}
void Application::onEvent(const sf::Event & e)
{
    switch (e.type)
    {
    case sf::Event::KeyPressed:
        if (e.key.code == sf::Keyboard::Escape)
        {
            renderWindow.close();
        }
        break;
    case sf::Event::MouseButtonPressed:
        switch (e.mouseButton.button)
        {
        case sf::Mouse::Left:
            mouseHeldLeft = true;
            //mouseLeftClickOrigin = renderWindow.mapPixelToCoords( { e.mouseButton.x, e.mouseButton.y } );
            mouseLeftClickOrigin = { float(e.mouseButton.x), float(e.mouseButton.y) };
            mouseLeftClickCenterScreen = view.getCenter();
            break;
        }
        break;
    case sf::Event::MouseButtonReleased:
        switch (e.mouseButton.button)
        {
        case sf::Mouse::Left:
            mouseHeldLeft = false;
            break;
        }
        break;
    case sf::Event::MouseMoved:
        if (mouseHeldLeft)
        {
            //sf::Vector2f mousePos = renderWindow.mapPixelToCoords({ e.mouseMove.x, e.mouseMove.y });
            //view.setCenter(view.getCenter() + sf::Vector2f{ float(e.mouseMove.x), float(e.mouseMove.y) });
            const sf::Vector2f mousePosF(float(e.mouseMove.x), float(e.mouseMove.y));
            sf::Vector2f fromClickOrigin = mousePosF - mouseLeftClickOrigin;
            fromClickOrigin.x *= -1.f;// ??????????? no idea
            view.setCenter(mouseLeftClickCenterScreen + fromClickOrigin);
        }
        break;
    }
}
void Application::tick(const sf::Time & deltaTime)
{
    renderWindow.setView(view);
    drawOrigin();
}
void Application::drawOrigin()
{
    sf::VertexArray va(sf::PrimitiveType::Lines, 4);
    va[0].position = { 0,0 };
    va[1].position = { 0,10 };
    va[0].color = sf::Color::Green;
    va[1].color = sf::Color::Green;
    va[2].position = { 0,0 };
    va[3].position = { 10,0 };
    va[2].color = sf::Color::Red;
    va[3].color = sf::Color::Red;
    renderWindow.draw(va);
}