#pragma once

#include "../thermal/ThermalProcessor.hpp"
#include "../config/Config.hpp"
#include <SDL2/SDL.h>
// #include <SDL2/SDL_ttf.h> // Disabled until installed
#include <string>
#include <vector>
#include <memory>
#include <atomic>

namespace Render
{
    class Renderer
    {
    public:
        Renderer(Config::Config& config);
        ~Renderer();

        // Initialize SDL and window
        bool Initialize(const std::string& title, int width, int height);

        // Cleanup SDL resources
        void Shutdown();

        // Render a thermal frame
        bool RenderFrame(const Thermal::ThermalFrame& frame);

        // Handle SDL events
        void HandleEvents();

        // Check if renderer is running
        bool IsRunning() const;

    private:
        // Internal helpers
        void ApplyColormap(const Thermal::ThermalFrame& frame, uint32_t* outputBuffer);
        void InitializeColormaps();
        void ScaleFrame(const uint32_t* sourceBuffer, uint32_t* destBuffer, int srcWidth, int srcHeight, int destWidth, int destHeight);
        void RenderHUD(const Thermal::ThermalFrame& frame);

        // Font rendering helpers
        void DrawText(const std::string& text, int x, int y, uint32_t color = 0xFFFFFFFF);

        Config::Config& _config;

        SDL_Window* _window;
        SDL_Renderer* _renderer;
        SDL_Texture* _texture;

        // Buffers
        std::vector<uint32_t> _pixelBuffer;
        std::vector<uint32_t> _scaledBuffer;

        // Pre-packed colormaps
        std::vector<std::vector<uint32_t>> _packedColormaps;

        int _windowWidth;
        // int _windowHeight; // Remove if not needed, or keep for init
        std::atomic<bool> _isRunning;
        bool _showHud;
        
        // Mouse interaction
        int _mouseX = -1;
        int _mouseY = -1;
        bool _isProbeEnabled = true; // Default to true or toggleable

        // Performance tracking
        // uint32_t _lastFrameTime;
        int _frameCount;
        int _currentFPS;
        uint32_t _fpsTimer;
    };
}
