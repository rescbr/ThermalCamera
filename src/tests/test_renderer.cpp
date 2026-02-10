#include "../render/Renderer.hpp"
#include "../config/Config.hpp"
#include <iostream>
#include <vector>
#include <cmath>
#include <thread>
#include <chrono>

using namespace Render;
using namespace Thermal;

void CreateGradientFrame(ThermalFrame& frame, int width, int height, float t)
{
    frame._width = width;
    frame._height = height;
    frame._data.resize(width * height);
    
    // Create a moving gradient
    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            float u = (float)x / width;
            float v = (float)y / height;
            
            // Value oscillates between 0C (273.15K) and 100C (373.15K)
            float val = 273.15f + 50.0f * (1.0f + std::sin(u * 10.0f + t)) * (0.5f + 0.5f * std::cos(v * 10.0f));
            
            frame._data[y * width + x] = static_cast<uint16_t>(val * 64.0f);
        }
    }
    
    // Set dummy stats
    frame._min._kelvin = 273 * 64;
    frame._min._celsius = 0;
    frame._min._fahrenheit = 32;

    frame._max._kelvin = 373 * 64;
    frame._max._celsius = 100;
    frame._max._fahrenheit = 212;
    
    frame._avg = frame._min;
    frame._center = frame._max;
}

int main(int argc, char** argv)
{
    (void)argc; (void)argv;
    Config::Config config;
    Renderer renderer(config);
    if (!renderer.Initialize("Thermal Renderer Test", 256, 192))
    {
        std::cerr << "Failed to initialize renderer" << std::endl;
        return 1;
    }
    
    ThermalFrame frame;
    
    std::cout << "Running renderer test..." << std::endl;
    
    auto startTime = std::chrono::steady_clock::now();
    int frames = 0;
    
    while (renderer.IsRunning())
    {
        renderer.HandleEvents();
        
        auto now = std::chrono::steady_clock::now();
        float t = std::chrono::duration<float>(now - startTime).count();
        CreateGradientFrame(frame, 256, 192, t);
        
        if (!renderer.RenderFrame(frame))
        {
            std::cerr << "Failed to render frame" << std::endl;
            break;
        }
        
        frames++;
        
        // Auto-quit after 5 seconds for automated testing
        if (t > 5.0f)
        {
            std::cout << "Test completed successfully." << std::endl;
            break;
        }

        // Test scaling and colormap switching
        if (frames % 100 == 0)
        {
            static int scale = 1;
            scale = (scale % 4) + 1;
            config.SetScaleFactor(scale);
            std::cout << "Scale: " << scale << std::endl;
            
            static int cmap = 0;
            cmap = (cmap + 1) % 7;
            config.SetColormapIndex(cmap);
             std::cout << "Colormap: " << cmap << std::endl;
        }
    }
    
    renderer.Shutdown();
    return 0;
}
