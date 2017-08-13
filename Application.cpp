#include "Application.h"
#include "toolbox.h"
#include <algorithm>
#include <iostream>
const float Application::DEFAULT_ZOOM = 0.03f;
Application::Application(sf::RenderWindow & rw, int argc, char** argv)
    :renderWindow(rw)
    ,view(rw.getDefaultView())
    ,mouseHeldRight(false)
    ,zoomPercent(DEFAULT_ZOOM)
{
    updateViewSize();
    view.setCenter({ 0,0 });
    // process our arg list //
    bool loadedMap = false;
    for (int c = 1; c < argc; c++)
    {
        if (argv[c] == std::string("-map"))
        {
            c++;
            if (c >= argc)
            {
                std::cerr << "ERROR: must specify map filename after \"-map\"\n";
                break;
            }
            if (!map.load(argv[c]))
            {
                exit(EXIT_FAILURE);
            }
            loadedMap = true;
        }
    }
    if (!loadedMap)
    {
        std::cerr << "ERROR: no map loaded! use -map \"filename\" to specify a Tiled JSON map.\n";
        exit(EXIT_FAILURE);
    }
}
void Application::onEvent(const sf::Event & e)
{
    static const float ZOOM_DELTA = -0.00625f;
    static const float MIN_ZOOM = DEFAULT_ZOOM * 2;
    static const float MAX_ZOOM = 0.00125f;
    switch (e.type)
    {
    case sf::Event::KeyPressed:
        switch(e.key.code)
        {
        case sf::Keyboard::F1:
            map.toggleVoxelGrid();
            break;
        case sf::Keyboard::F2:
            map.togglePartitionMeta();
            break;
        case sf::Keyboard::Escape:
            renderWindow.close();
            break;
        case sf::Keyboard::J:
            zoomPercent += ZOOM_DELTA*-1;
            zoomPercent = clampf(zoomPercent, MAX_ZOOM, MIN_ZOOM);
            updateViewSize();
            break;
        case sf::Keyboard::K:
            zoomPercent += ZOOM_DELTA*1;
            zoomPercent = clampf(zoomPercent, MAX_ZOOM, MIN_ZOOM);
            updateViewSize();
            break;
        }
        break;
    case sf::Event::MouseButtonPressed:
        switch (e.mouseButton.button)
        {
        case sf::Mouse::Left:
            mouseHeldLeft = true;
            mouseLeftClickPosition = { e.mouseButton.x, e.mouseButton.y };
            break;
        case sf::Mouse::Right:
            mouseHeldRight = true;
            mouseRightClickOrigin = { float(e.mouseButton.x), float(e.mouseButton.y) };
            mouseRightClickCenterScreen = view.getCenter();
            break;
        case sf::Mouse::Middle:
            zoomPercent = DEFAULT_ZOOM;
            updateViewSize();
            break;
        }
        break;
    case sf::Event::MouseButtonReleased:
        switch (e.mouseButton.button)
        {
        case sf::Mouse::Left:
            mouseHeldLeft = false;
            break;
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
            view.setCenter(mouseRightClickCenterScreen + fromClickOrigin*zoomPercent);
        }
        break;
    case sf::Event::MouseWheelScrolled:
        {
            //std::cout << "wheelDelta=" << e.mouseWheelScroll.delta << std::endl;
            zoomPercent += ZOOM_DELTA*e.mouseWheelScroll.delta;
            zoomPercent = clampf(zoomPercent, MAX_ZOOM, MIN_ZOOM);
            updateViewSize();
        }
        break;
    }
}
void Application::tick(const sf::Time & deltaTime)
{
    renderWindow.setView(view);
    if (mouseHeldLeft)
    {
        map.touch(renderWindow.mapPixelToCoords(mouseLeftClickPosition));
    }
    map.stepSimulation();
    map.draw(renderWindow);
    drawOrigin();
}
void Application::drawOrigin()
{
    static const float ORIGIN_LINE_SIZE = 1;
    // First, we get the boundaries of the world that we're drawing
    //  so that we can prevent the origin from being drawn off-screen //
    auto viewCenter = view.getCenter();
    auto viewSize = view.getSize();
    viewSize.y *= -1;// need to do this because we invert the view size to fix the y-axis
    auto viewBottomLeft = viewCenter - viewSize*0.5f;
    auto viewTopRight = viewBottomLeft + viewSize;
    sf::Vector2f drawLocation(
        clampf(viewBottomLeft.x + 1.f/999, 0.f, viewTopRight.x - 1.f/9),
        clampf(viewBottomLeft.y + 1.f/999, 0.f, viewTopRight.y - 1.f/9));
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