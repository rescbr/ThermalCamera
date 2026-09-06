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
        void ApplyColormap(const Thermal::ThermalFrame& frame, void* pixels, int pitch);
        void InitializeColormaps();
        void RenderHUD(const Thermal::ThermalFrame& frame);

        // Font rendering helpers
        void InitializeFont();
        void DrawText(const std::string& text, int x, int y, uint32_t color = 0xFFFFFFFF, bool bold = false);
        int TextWidth(const std::string& text, bool bold = false) const;

        // Probe (temperature spot readout) helpers
        int ProbeStep() const;
        void MoveProbe(int dx, int dy);
        void ApplyStickMovement();
        void ZoomIn() { if (_zoom < 16) _zoom *= 2; }
        void ZoomOut() { if (_zoom > 1) _zoom /= 2; }

        Config::Config& _config;

        SDL_Window* _window;
        SDL_Renderer* _renderer;
        SDL_Texture* _texture;
        
        // Font Atlas
        SDL_Texture* _fontTexture;
        SDL_Texture* _fontTextureBold;
        std::vector<SDL_Rect> _glyphRects;
        std::vector<SDL_Rect> _glyphRectsBold;

        // Buffers
        // std::vector<uint32_t> _pixelBuffer; // Removed
        
        // Pre-packed colormaps
        std::vector<std::vector<uint32_t>> _packedColormaps;

        int _windowWidth;
        // int _windowHeight; // Remove if not needed, or keep for init
        std::atomic<bool> _isRunning;
        bool _showHud;
        int _fontScale = 1; // HUD text multiplier (tracks window scale)
        
        // Mouse interaction
        int _mouseX = -1;
        int _mouseY = -1;
        bool _isProbeEnabled = true; // Default to true or toggleable

        // Temperature probe cursor (window coords, moved by mouse/gamepad)
        int _probeX = -1;
        int _probeY = -1;
        bool _probeActive = false;
        int _stickX = 0;
        int _stickY = 0;
        int _zoom = 1;          // power of two; crop = frame / zoom
        int _cropX = 0;
        int _cropY = 0;
        bool _prevR2 = false;
        bool _prevL2 = false;

        // Performance tracking
        // uint32_t _lastFrameTime;
        int _frameCount;
        int _currentFPS;
        uint32_t _fpsTimer;
    };
}
