#include "Application.h"
#include "toolbox.h"
#include <algorithm>
#include <iostream>
Application::Application(sf::RenderWindow & rw)
    :renderWindow(rw)
    ,view(rw.getDefaultView())
    ,mouseHeldRight(false)
    ,zoomPercent(1.f)
{
    updateViewSize();
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
        case sf::Mouse::Right:
            mouseHeldRight = true;
            mouseRightClickOrigin = { float(e.mouseButton.x), float(e.mouseButton.y) };
            mouseRightClickCenterScreen = view.getCenter();
            break;
        case sf::Mouse::Middle:
            zoomPercent = 1.f;
            updateViewSize();
            break;
        }
        break;
    case sf::Event::MouseButtonReleased:
        switch (e.mouseButton.button)
        {
        case sf::Mouse::Right:
            mouseHeldRight = false;
            break;
        }
        break;
    case sf::Event::MouseMoved:
        if (mouseHeldRight)
        {
            const sf::Vector2f mousePosF(float(e.mouseMove.x), float(e.mouseMove.y));
            sf::Vector2f fromClickOrigin = mouseRightClickOrigin - mousePosF;
            fromClickOrigin.y *= -1.f;// need to invert y because it's inverted in screen-space
            view.setCenter(mouseRightClickCenterScreen + fromClickOrigin);
        }
        break;
    case sf::Event::MouseWheelScrolled:
        {
            static const float ZOOM_DELTA = -0.05f;
            //std::cout << "wheelDelta=" << e.mouseWheelScroll.delta << std::endl;
            zoomPercent += ZOOM_DELTA*e.mouseWheelScroll.delta;
            zoomPercent = clampf(zoomPercent, 0.05f, 2.f);
            updateViewSize();
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
    static const float ORIGIN_LINE_SIZE = 10;
    // First, we get the boundaries of the world that we're drawing
    //  so that we can prevent the origin from being drawn off-screen //
    auto viewCenter = view.getCenter();
    auto viewSize = view.getSize();
    viewSize.y *= -1;// need to do this because we invert the view size to fix the y-axis
    auto viewBottomLeft = viewCenter - viewSize*0.5f;
    auto viewTopRight = viewBottomLeft + viewSize;
    sf::Vector2f drawLocation(
        clampf(viewBottomLeft.x + 1, 0.f, viewTopRight.x - ORIGIN_LINE_SIZE),
        clampf(viewBottomLeft.y + 1, 0.f, viewTopRight.y - ORIGIN_LINE_SIZE));
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
void Application::updateViewSize()
{
    auto winSize = renderWindow.getSize();
    // Invert the camera's y-axis //
    sf::Vector2f newSize(float(winSize.x), winSize.y*-1.f);
    view.setSize(newSize*zoomPercent);
}