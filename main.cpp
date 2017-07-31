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
    std::vector<double> vec = { 0.5f, 0.6f, 0.7f, 0.8f };
    std::cout << "originalVec="; dumpVec(vec); std::cout << std::endl;
    fftw_plan plan = fftw_plan_r2r_1d(vec.size(), &vec[0], &vec[0], FFTW_REDFT10, FFTW_ESTIMATE);
    fftw_execute(plan);
    std::cout << "DCT="; dumpVec(vec); std::cout << std::endl;
    fftw_plan planI = fftw_plan_r2r_1d(vec.size(), &vec[0], &vec[0], FFTW_REDFT01, FFTW_ESTIMATE);
    fftw_execute(planI);
    for (auto& value : vec)
    {
        value /= 2 * vec.size();
    }
    std::cout << "IDCT="; dumpVec(vec); std::cout << std::endl;
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