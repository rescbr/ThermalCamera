#include "Renderer.hpp"
#include "../Error.hpp"
#include "../Profile.hpp"
#include "../colormaps.hpp"
#include "../fonts/EspySans_10.h"
#include <iostream>
#include <stdexcept>
#include <cstring>
#include <algorithm>
#include <vector>
#include <iomanip>
#include <sstream>

namespace Render
{
    // Define available colormaps
    struct ColormapEntry {
        const char* name;
        const Colormaps::colormap_color_t* data;
        int size;
    };

    static const ColormapEntry AVAILABLE_COLORMAPS[] = {
        { "Magma", Colormaps::magma, 256 },
        { "Inferno", Colormaps::inferno, 256 },
        { "Plasma", Colormaps::plasma, 256 },
        { "RdBu", Colormaps::rdbu, 256 },
        { "Oleron", Colormaps::oleron, 256 },
        { "Roma", Colormaps::roma, 256 },
        { "Berlin", Colormaps::berlin, 256 }
    };
    
    static const int COLORMAP_COUNT = sizeof(AVAILABLE_COLORMAPS) / sizeof(ColormapEntry);

    Renderer::Renderer(Config::Config& config)
        : _config(config)
        , _window(nullptr)
        , _renderer(nullptr)
        , _texture(nullptr)
        , _windowWidth(0)
        , _isRunning(false)
        , _showHud(true)
        , _frameCount(0)
        , _currentFPS(0)
        , _fpsTimer(0)
    {
    }

    Renderer::~Renderer()
    {
        Shutdown();
    }

    void Renderer::Shutdown()
    {
        _isRunning = false;

        if (_texture)
        {
            SDL_DestroyTexture(_texture);
            _texture = nullptr;
        }

        if (_renderer)
        {
            SDL_DestroyRenderer(_renderer);
            _renderer = nullptr;
        }

        if (_window)
        {
            SDL_DestroyWindow(_window);
            _window = nullptr;
        }

        SDL_Quit();
    }

    bool Renderer::RenderFrame(const Thermal::ThermalFrame& frame)
    {
        PROFILE_SCOPE("Renderer::RenderFrame");

        if (!_isRunning || !_renderer)
        {
            return false;
        }

        // --- Handle Resizing / Fullscreen ---
        int scale = _config.GetScaleFactor();
        int targetW = frame._width * scale;
        int targetH = frame._height * scale;

        // Check if texture needs recreation
        bool recreateTexture = false;
        if (!_texture) {
            recreateTexture = true;
        } else {
            int texW, texH;
            SDL_QueryTexture(_texture, nullptr, nullptr, &texW, &texH);
            if (texW != targetW || texH != targetH) {
                recreateTexture = true;
            }
        }

        if (recreateTexture) {
            if (_texture) SDL_DestroyTexture(_texture);
            _texture = SDL_CreateTexture(_renderer, SDL_PIXELFORMAT_ARGB8888, 
                                         SDL_TEXTUREACCESS_STREAMING, 
                                         targetW, targetH);
            if (!_texture) {
                std::cerr << "Failed to create texture: " << SDL_GetError() << std::endl;
                return false;
            }
            try {
                _scaledBuffer.resize(targetW * targetH);
            } catch (...) {
                return false;
            }
            
            // If not fullscreen, resize window to match
            if (!_config.GetFullscreen()) {
                SDL_SetWindowSize(_window, targetW, targetH);
            }
        }

        // Check fullscreen state
        uint32_t flags = SDL_GetWindowFlags(_window);
        bool isFullscreen = (flags & SDL_WINDOW_FULLSCREEN_DESKTOP) != 0;
        if (_config.GetFullscreen() != isFullscreen) {
            SDL_SetWindowFullscreen(_window, _config.GetFullscreen() ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
            if (!_config.GetFullscreen()) {
                // Restore size if exiting fullscreen
                SDL_SetWindowSize(_window, targetW, targetH);
            }
        }


        // --- Rendering Steps ---

        // Step 1: Apply colormap to thermal data
        // Writes to _pixelBuffer (Source Size)
        // Ensure pixel buffer is large enough
        if (_pixelBuffer.size() < frame._data.size()) {
            _pixelBuffer.resize(frame._data.size());
        }
        {
            PROFILE_SCOPE("ApplyColormap");
            ApplyColormap(frame, _pixelBuffer.data());
        }

        // Step 2: Scale to display size (CPU Scaling)
        // Reads from _pixelBuffer, writes to _scaledBuffer (Target Size)
        {
            PROFILE_SCOPE("ScaleFrame");
            ScaleFrame(_pixelBuffer.data(), _scaledBuffer.data(),
                       frame._width, frame._height,
                       targetW, targetH);
        }

        // Step 3: Update SDL texture
        void* pixels;
        int pitch;
        if (SDL_LockTexture(_texture, nullptr, &pixels, &pitch) != 0)
        {
             std::cerr << "SDL_LockTexture Error: " << SDL_GetError() << std::endl;
             return false;
        }

        // Copy scaled buffer to texture
        // Assuming texture width matches scaled width (targetW)
        if (pitch == targetW * 4)
        {
            std::memcpy(pixels, _scaledBuffer.data(), _scaledBuffer.size() * sizeof(uint32_t));
        }
        else
        {
            // Row-by-row copy
            uint8_t* dst = static_cast<uint8_t*>(pixels);
            const uint32_t* src = _scaledBuffer.data();
            for (int i = 0; i < targetH; ++i)
            {
                std::memcpy(dst, src, targetW * sizeof(uint32_t));
                dst += pitch;
                src += targetW;
            }
        }

        SDL_UnlockTexture(_texture);

        // Step 4: Render texture to screen
        SDL_RenderClear(_renderer);
        // Copy texture to entire renderer target (automatically handles scaling if fullscreen)
        SDL_RenderCopy(_renderer, _texture, nullptr, nullptr);
        
        // Step 5: Render HUD
        if (_showHud) {
            PROFILE_SCOPE("RenderHUD");
            RenderHUD(frame);
        }

        SDL_RenderPresent(_renderer);
        
        // Update FPS
        _frameCount++;
        if (SDL_GetTicks() - _fpsTimer >= 1000)
        {
            _currentFPS = _frameCount;
            _frameCount = 0;
            _fpsTimer = SDL_GetTicks();
        }

        return true;
    }
    
    void Renderer::ApplyColormap(const Thermal::ThermalFrame& frame, uint32_t* outputBuffer)
    {
        if (frame._data.empty() || !outputBuffer) return;

        uint16_t minK = frame._min._kelvin;
        uint16_t maxK = frame._max._kelvin;
        
        if (minK >= maxK) {
            maxK = minK + 1;
        }
        
        float range = static_cast<float>(maxK - minK);
        float scale = 255.0f / range;
        
        int cmapIdx = _config.GetColormapIndex();
        if (cmapIdx < 0) cmapIdx = 0;
        const ColormapEntry& map = AVAILABLE_COLORMAPS[cmapIdx % COLORMAP_COUNT];
        const Colormaps::colormap_color_t* palette = map.data;

        const size_t pixelCount = frame._data.size();
        const uint16_t* input = frame._data.data();

        // Process pixels
        for (size_t i = 0; i < pixelCount; ++i)
        {
            uint16_t val = input[i];
            
            // Clamp value
            if (val < minK) val = minK;
            if (val > maxK) val = maxK;
            
            // Map to 0-255
            int index = static_cast<int>((val - minK) * scale);
            if (index < 0) index = 0;
            if (index > 255) index = 255;
            
            const auto& color = palette[index];
            
            // Pack ARGB
            outputBuffer[i] = (255 << 24) | (color.r << 16) | (color.g << 8) | color.b;
        }
    }
    
    void Renderer::ScaleFrame(const uint32_t* sourceBuffer, uint32_t* destBuffer, int srcWidth, int srcHeight, int destWidth, int destHeight)
    {
        // Simple Bilinear Interpolation
        if (!sourceBuffer || !destBuffer) return;
        
        if (srcWidth == destWidth && srcHeight == destHeight)
        {
            std::memcpy(destBuffer, sourceBuffer, srcWidth * srcHeight * sizeof(uint32_t));
            return;
        }

        const float xRatio = static_cast<float>(srcWidth - 1) / destWidth;
        const float yRatio = static_cast<float>(srcHeight - 1) / destHeight;
        
        for (int y = 0; y < destHeight; ++y)
        {
            float srcY = y * yRatio;
            int y0 = static_cast<int>(srcY);
            int y1 = (y0 < srcHeight - 1) ? y0 + 1 : y0;
            float yDiff = srcY - y0;
            float yInv = 1.0f - yDiff;
            
            for (int x = 0; x < destWidth; ++x)
            {
                float srcX = x * xRatio;
                int x0 = static_cast<int>(srcX);
                int x1 = (x0 < srcWidth - 1) ? x0 + 1 : x0;
                float xDiff = srcX - x0;
                float xInv = 1.0f - xDiff;
                
                uint32_t p00 = sourceBuffer[y0 * srcWidth + x0];
                uint32_t p10 = sourceBuffer[y0 * srcWidth + x1];
                uint32_t p01 = sourceBuffer[y1 * srcWidth + x0];
                uint32_t p11 = sourceBuffer[y1 * srcWidth + x1];
                
                float b = ((p00 & 0xFF) * xInv + (p10 & 0xFF) * xDiff) * yInv +
                          ((p01 & 0xFF) * xInv + (p11 & 0xFF) * xDiff) * yDiff;
                
                float g = (((p00 >> 8) & 0xFF) * xInv + ((p10 >> 8) & 0xFF) * xDiff) * yInv +
                          (((p01 >> 8) & 0xFF) * xInv + ((p11 >> 8) & 0xFF) * xDiff) * yDiff;

                float r = (((p00 >> 16) & 0xFF) * xInv + ((p10 >> 16) & 0xFF) * xDiff) * yInv +
                          (((p01 >> 16) & 0xFF) * xInv + ((p11 >> 16) & 0xFF) * xDiff) * yDiff;
                          
                destBuffer[y * destWidth + x] = (255 << 24) | 
                                                (static_cast<uint8_t>(r) << 16) | 
                                                (static_cast<uint8_t>(g) << 8) | 
                                                static_cast<uint8_t>(b);
            }
        }
    }

    void Renderer::DrawText(const std::string& text, int x, int y, uint32_t color)
    {
        int pen_x = x;
        int pen_y = y;
        
        uint8_t a = (color >> 24) & 0xFF;
        uint8_t r = (color >> 16) & 0xFF;
        uint8_t g = (color >> 8) & 0xFF;
        uint8_t b = (color & 0xFF);
        
        SDL_SetRenderDrawColor(_renderer, r, g, b, a);
        
        for (char c : text) {
            const auto& glyph = Fonts::EspySans_10::glyphs[static_cast<uint8_t>(c)];
            if (!glyph.bitmap) continue;
            
            int draw_x = pen_x + glyph.x_offset;
            int draw_y = pen_y - (glyph.y_offset + glyph.height);
            
            int bytesPerRow = (glyph.width + 7) / 8;
            
            for (int row = 0; row < glyph.height; ++row) {
                for (int col = 0; col < glyph.width; ++col) {
                    int byteIndex = row * bytesPerRow + (col / 8);
                    int bitIndex = 7 - (col % 8); 
                    
                    if (glyph.bitmap[byteIndex] & (1 << bitIndex)) {
                        SDL_RenderDrawPoint(_renderer, draw_x + col, draw_y + row);
                    }
                }
            }
            pen_x += glyph.advance;
        }
    }

    void Renderer::RenderHUD(const Thermal::ThermalFrame& frame)
    {
        int scale = _config.GetScaleFactor();
        int width = frame._width * scale;
        int height = frame._height * scale;
        int centerX = width / 2;
        int centerY = height / 2;
        int size = 10 * scale;

        // Draw crosshair at center
        SDL_SetRenderDrawColor(_renderer, 255, 255, 255, 255);
        SDL_RenderDrawLine(_renderer, centerX - size, centerY, centerX + size, centerY);
        SDL_RenderDrawLine(_renderer, centerX, centerY - size, centerX, centerY + size);
        
        bool useCelsius = _config.GetUseCelsius();
        auto formatTemp = [useCelsius](float c, float f) {
            std::stringstream ss;
            ss << std::fixed << std::setprecision(1);
            if (useCelsius) ss << c << "C";
            else ss << f << "F";
            return ss.str();
        };

        // Top Left: FPS and Temps
        std::stringstream ssLeft;
        ssLeft << "FPS: " << _currentFPS;
        DrawText(ssLeft.str(), 10, 20, 0xFFFFFFFF);
        
        ssLeft.str("");
        ssLeft << "Min: " << formatTemp(frame._min._celsius, frame._min._fahrenheit);
        DrawText(ssLeft.str(), 10, 35, 0xFF00FFFF); // Cyan

        ssLeft.str("");
        ssLeft << "Max: " << formatTemp(frame._max._celsius, frame._max._fahrenheit);
        DrawText(ssLeft.str(), 10, 50, 0xFFFF0000); // Red

        ssLeft.str("");
        ssLeft << "Avg: " << formatTemp(frame._avg._celsius, frame._avg._fahrenheit);
        DrawText(ssLeft.str(), 10, 65, 0xFF00FF00); // Green
        
        // Top Right: Colormap & Scale
        int rightX = width - 120;
        int cmapIdx = _config.GetColormapIndex();
        if (cmapIdx < 0) cmapIdx = 0;
        DrawText(AVAILABLE_COLORMAPS[cmapIdx % COLORMAP_COUNT].name, rightX, 20);
        
        std::stringstream ssRight;
        ssRight << "Scale: " << scale << "X";
        DrawText(ssRight.str(), rightX, 35);
        
        if (_config.GetFreezeFrame()) {
             DrawText("[FROZEN]", rightX, 50, 0xFF00FFFF);
        }

        // Center Temp
        std::string centerT = formatTemp(frame._center._celsius, frame._center._fahrenheit);
        DrawText(centerT, centerX + 5, centerY - 5);
        
        // Mouse Probe
        if (_isProbeEnabled && _mouseX >= 0 && _mouseY >= 0) {
            // Map mouse to frame coordinates
            // Need to account for scale AND rotation?
            // The frame is already rotated in ThermalProcessor before RenderFrame gets it.
            // But we scale it here.
            
            // Mouse is in Window coordinates (which are scaled)
            int frameX = _mouseX / scale;
            int frameY = _mouseY / scale;
            
            if (frameX >= 0 && frameX < frame._width && frameY >= 0 && frameY < frame._height) {
                Thermal::Temperature t = Thermal::ThermalProcessor::GetTemperatureAt(frame, frameY, frameX);
                
                std::string probeT = formatTemp(t._celsius, t._fahrenheit);
                DrawText(probeT, _mouseX + 10, _mouseY);
                
                // Draw small box/cross at cursor
                SDL_SetRenderDrawColor(_renderer, 255, 255, 0, 255);
                SDL_Rect rect = {_mouseX - 2, _mouseY - 2, 5, 5};
                SDL_RenderDrawRect(_renderer, &rect);
            }
        }
        
        // Bottom: Controls hint
        if (height > 100) {
            DrawText("[H] Toggle HUD  [Space] Freeze  [Q] Quit", 10, height - 10);
        }
    }

    void Renderer::HandleEvents()
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_QUIT)
            {
                _isRunning = false;
            }
            else if (event.type == SDL_MOUSEMOTION)
            {
                _mouseX = event.motion.x;
                _mouseY = event.motion.y;
            }
            else if (event.type == SDL_MOUSEBUTTONDOWN)
            {
                if (event.button.button == SDL_BUTTON_RIGHT) {
                    _isProbeEnabled = !_isProbeEnabled;
                }
            }
            else if (event.type == SDL_KEYDOWN)
            {
                switch (event.key.keysym.sym)
                {
                    case SDLK_ESCAPE:
                    case SDLK_q:
                        _isRunning = false;
                        break;
                    case SDLK_f:
                    case SDLK_F11:
                        _config.SetFullscreen(!_config.GetFullscreen());
                        break;
                    case SDLK_PLUS:
                    case SDLK_KP_PLUS:
                    case SDLK_EQUALS:
                         {
                             int s = _config.GetScaleFactor();
                             if (s < 10) _config.SetScaleFactor(s + 1);
                         }
                         break;
                    case SDLK_MINUS:
                    case SDLK_KP_MINUS:
                         {
                             int s = _config.GetScaleFactor();
                             if (s > 1) _config.SetScaleFactor(s - 1);
                         }
                         break;
                    case SDLK_LEFTBRACKET:
                         {
                             int r = _config.GetRotation();
                             _config.SetRotation((r + 1) % 4);
                         }
                         break;
                    case SDLK_RIGHTBRACKET:
                         {
                             int r = _config.GetRotation();
                             _config.SetRotation((r + 3) % 4);
                         }
                         break;
                    case SDLK_c:
                         _config.SetUseCelsius(!_config.GetUseCelsius());
                         break;
                    case SDLK_m:
                    case SDLK_COMMA:
                         {
                             int c = _config.GetColormapIndex();
                             _config.SetColormapIndex((c + 1) % COLORMAP_COUNT);
                         }
                         break;
                    case SDLK_n:
                    case SDLK_PERIOD:
                         {
                             int c = _config.GetColormapIndex();
                             _config.SetColormapIndex((c + COLORMAP_COUNT - 1) % COLORMAP_COUNT);
                         }
                         break;
                    case SDLK_SPACE:
                         _config.SetFreezeFrame(!_config.GetFreezeFrame());
                         break;
                    case SDLK_r:
                         _config.SetDefaults();
                         break;
                    case SDLK_h:
                         _showHud = !_showHud;
                         break;
                }
            }
            // TODO: Mouse Input (Task 5.4)
        }
    }

    bool Renderer::IsRunning() const
    {
        return _isRunning;
    }

    bool Renderer::Initialize(const std::string& title, int width, int height)
    {
        _windowWidth = width;
        // _windowHeight = height; // Not stored currently

        if (SDL_Init(SDL_INIT_VIDEO) != 0)
        {
            LOG_ERROR(std::string("SDL_Init Error: ") + SDL_GetError());
            return false;
        }

        // Create window
        int scale = _config.GetScaleFactor();
        uint32_t flags = SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE;
        if (_config.GetFullscreen()) flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;

        _window = SDL_CreateWindow(title.c_str(),
                                   SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                   width * scale, height * scale,
                                   flags);
        if (!_window)
        {
            LOG_ERROR(std::string("SDL_CreateWindow Error: ") + SDL_GetError());
            Shutdown();
            return false;
        }

        // Create renderer
        _renderer = SDL_CreateRenderer(_window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        if (!_renderer)
        {
            LOG_ERROR(std::string("SDL_CreateRenderer Error: ") + SDL_GetError());
            Shutdown();
            return false;
        }

        // Texture and buffers will be created in RenderFrame on first frame if missing,
        // or we can create them here.
        // Let's create them here to fail early if memory issue.
        int targetW = width * scale;
        int targetH = height * scale;
        
        _texture = SDL_CreateTexture(_renderer, SDL_PIXELFORMAT_ARGB8888, 
                                     SDL_TEXTUREACCESS_STREAMING, 
                                     targetW, targetH);
        
        try {
            _pixelBuffer.resize(width * height);
            _scaledBuffer.resize(targetW * targetH);
        } catch (const std::exception& e) {
            std::cerr << "Failed to allocate buffers: " << e.what() << std::endl;
            Shutdown();
            return false;
        }

        _isRunning = true;
        _fpsTimer = SDL_GetTicks();
        
        std::cerr << "Renderer initialized successfully." << std::endl;
        return true;
    }
}
