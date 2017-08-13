#include <SFML/Graphics.hpp>
#include "Application.h"
#include <fftw3.h>/// DEBUG
#include <iostream>/// DEBUG
int main(int argc, char** argv)
{
    /// DEBUG testing out fftw~ //////////////////////////////////////////////
    auto dumpVec = [](std::vector<double> v)->void
    {
        std::cout << "{";
        for (size_t c = 0; c < v.size(); c++)
        {
            if (c > 0) std::cout << ", ";
            std::cout << v[c];
        }
        std::cout << "}";
    };
    auto dumpArray = [](double* a, size_t lx, size_t ly)->void
    {
        std::cout << "{";
        for (size_t y = 0; y < ly; y++)
        {
            std::cout << "{";
            for(size_t x = 0; x < lx; x++)
            {
                if (x > 0) std::cout << ", ";
                const size_t i = y*lx + x;
                std::cout << a[i];
            }
            std::cout << "}";
            if (y < ly - 1)std::cout << std::endl;
        }
        std::cout << "}";
    };
    std::vector<double> vec = { .5, 0, -.5, 0, .5 };
    std::cout << "originalVec="; dumpVec(vec); std::cout << std::endl;
    fftw_plan plan = fftw_plan_r2r_1d(vec.size(), &vec[0], &vec[0], FFTW_REDFT10, FFTW_ESTIMATE);
    fftw_execute(plan);
    for (auto& value : vec)
    {
        value /= sqrt(2 * vec.size());
    }
    std::cout << "DCT="; dumpVec(vec); std::cout << std::endl;
    fftw_plan planI = fftw_plan_r2r_1d(vec.size(), &vec[0], &vec[0], FFTW_REDFT01, FFTW_ESTIMATE);
    fftw_execute(planI);
    for (auto& value : vec)
    {
        value /= sqrt(2 * vec.size());
    }
    std::cout << "IDCT="; dumpVec(vec); std::cout << std::endl;
    // testing out mode->pressure plan because weird NaN shit is happening... //
    const size_t gridSizeX = 4;
    const size_t gridSizeY = 3;
    const size_t gridSize = gridSizeX*gridSizeY;
    double* modes = new double[gridSize];
    double* pressures = new double[gridSize];
    for (size_t i = 0; i < gridSize; i++)
    {
        modes[i] = pressures[i] = 0;
    }
    modes[0 * gridSizeX + 0] = 3;
    modes[0 * gridSizeX + 1] = 2;
    modes[1 * gridSizeX + 0] = 2;
    modes[1 * gridSizeX + 1] = 1;
    fftw_plan planModesToPressures = fftw_plan_r2r_2d(
        gridSizeY, gridSizeX,
        modes, pressures,
        FFTW_REDFT01, FFTW_REDFT01, FFTW_ESTIMATE);
    fftw_plan planPressuresToModes = fftw_plan_r2r_2d(
        gridSizeY, gridSizeX,
        pressures, modes,
        FFTW_REDFT10, FFTW_REDFT10, FFTW_ESTIMATE);
    fftw_execute(planModesToPressures);
    for (int i = 0; i < gridSize; i++)
    {
        //pressures[i] /= 2*2*gridSize;/// why does this work...?......
        pressures[i] /= 2*gridSizeX*2*gridSizeY;/// why does this work...?......
    }
    std::cout << "Modes=\n"; dumpArray(modes, gridSizeX, gridSizeY); std::cout << std::endl;
    std::cout << "Pressures=\n"; dumpArray(pressures, gridSizeX, gridSizeY); std::cout << std::endl;
    fftw_execute(planPressuresToModes);
    std::cout << "Modes=\n"; dumpArray(modes, gridSizeX, gridSizeY); std::cout << std::endl;
    /// //////////////////////////////////////////////////////////////////////
    sf::RenderWindow window(sf::VideoMode(800, 600), "SFML window");
    Application app(window, argc, argv);
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