#include "Application.h"
#include "toolbox.h"
#include <algorithm>
#include <iostream>
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
            sf::Vector2f fromClickOrigin = mouseLeftClickOrigin - mousePosF;
            fromClickOrigin.y *= -1.f;// need to invert y because it's inverted in screen-space
            view.setCenter(mouseLeftClickCenterScreen + fromClickOrigin);
        }
        break;
    case sf::Event::MouseWheelScrolled:
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
    static const float ORIGIN_LINE_SIZE = 10;
    // First, we get the boundaries of the world that we're drawing
    //  so that we can prevent the origin from being drawn off-screen //
    auto viewCenter = view.getCenter();
    auto viewSize = view.getSize();
    viewSize.y *= -1;// need to do this because we invert the view size to fix the y-axis
    auto viewBottomLeft = viewCenter - viewSize*0.5f;
    auto viewTopRight = viewBottomLeft + viewSize;
    sf::Vector2f drawLocation(
        std::min(std::max(viewBottomLeft.x + 1, 0.f), viewTopRight.x - ORIGIN_LINE_SIZE),
        std::min(std::max(viewBottomLeft.y + 1, 0.f), viewTopRight.y - ORIGIN_LINE_SIZE));
    // Finally, we draw the origin graphics //
    sf::VertexArray va(sf::PrimitiveType::Lines, 4);
    va[0].position = drawLocation + sf::Vector2f{ 0,0 };
    va[1].position = drawLocation + sf::Vector2f{ 0,ORIGIN_LINE_SIZE };
    va[0].color = sf::Color::Green;
    va[1].color = sf::Color::Green;
    va[2].position = drawLocation + sf::Vector2f{ 0,0 };
    va[3].position = drawLocation + sf::Vector2f{ ORIGIN_LINE_SIZE,0 };
    va[2].color = sf::Color::Red;
    va[3].color = sf::Color::Red;
    renderWindow.draw(va);
}