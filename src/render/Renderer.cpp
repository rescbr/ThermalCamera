#include "Renderer.hpp"
#include "../Error.hpp"
#include "../Profile.hpp"
#include "../colormaps.hpp"
#include "../fonts/EspySans_14.h"
#include "../fonts/EspySansBold_14.h"
#include <iostream>
#include <stdexcept>
#include "../thermal/ThermalSimd.hpp"
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
        , _fontTexture(nullptr)
        , _fontTextureBold(nullptr)
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

        if (_fontTexture)
        {
            SDL_DestroyTexture(_fontTexture);
            _fontTexture = nullptr;
        }

        if (_fontTextureBold)
        {
            SDL_DestroyTexture(_fontTextureBold);
            _fontTextureBold = nullptr;
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
        _fontScale = (scale > 2) ? 2 : scale; // HUD text grows with window, capped at 2x (14pt base)
        int targetW = frame._width * scale;
        int targetH = frame._height * scale;

        // Check if texture needs recreation (source size changed)
        bool recreateTexture = false;
        if (!_texture) {
            recreateTexture = true;
        } else {
            int texW, texH;
            SDL_QueryTexture(_texture, nullptr, nullptr, &texW, &texH);
            if (texW != frame._width || texH != frame._height) {
                recreateTexture = true;
            }
        }

        if (recreateTexture) {
            if (_texture) SDL_DestroyTexture(_texture);
            // Create texture at source resolution
            _texture = SDL_CreateTexture(_renderer, SDL_PIXELFORMAT_ARGB8888, 
                                         SDL_TEXTUREACCESS_STREAMING, 
                                         frame._width, frame._height);
            if (!_texture) {
                std::cerr << "Failed to create texture: " << SDL_GetError() << std::endl;
                return false;
            }
            
            // If not fullscreen, resize window to match target size
            if (!_config.GetFullscreen()) {
                SDL_SetWindowSize(_window, targetW, targetH);
            }
        }

        // Check fullscreen state - only apply config-initiated transitions.
        // Never force-exit compositor-driven fullscreen (e.g. sway fullscreen
        // on the console): SDL would toggle it back off every frame.
        uint32_t flags = SDL_GetWindowFlags(_window);
        bool isFullscreen = (flags & SDL_WINDOW_FULLSCREEN_DESKTOP) != 0;
        if (_config.GetFullscreen() && !isFullscreen) {
            SDL_SetWindowFullscreen(_window, SDL_WINDOW_FULLSCREEN_DESKTOP);
        } else if (!_config.GetFullscreen()) {
            // Ensure window size matches scale factor, but only when the desired
            // size actually changes - reissuing SetWindowSize every frame fights
            // compositor-driven resizes (e.g. sway fullscreen on the console).
            static int lastDesiredW = 0, lastDesiredH = 0;
            if (lastDesiredW != targetW || lastDesiredH != targetH) {
                int w, h;
                SDL_GetWindowSize(_window, &w, &h);
                if (w != targetW || h != targetH) {
                    SDL_SetWindowSize(_window, targetW, targetH);
                }
                lastDesiredW = targetW;
                lastDesiredH = targetH;
            }
        }


        // --- Rendering Steps ---

        // All drawing happens in real window pixels; the image is letterboxed
        // manually so the HUD can overlay the entire window.
        SDL_GetRendererOutputSize(_renderer, &_winW, &_winH);
        _fontScale = std::clamp(_winH / 320, 1, 2); // HUD text grows with window height

        // Apply analog stick probe movement (per frame)
        ApplyStickMovement();

        // Step 1: Lock Texture to write pixels directly
        void* pixels;
        int pitch;
        if (SDL_LockTexture(_texture, nullptr, &pixels, &pitch) != 0)
        {
             std::cerr << "SDL_LockTexture Error: " << SDL_GetError() << std::endl;
             return false;
        }

        // Step 2: Apply colormap to texture memory
        {
            PROFILE_SCOPE("ApplyColormap");
            ApplyColormap(frame, pixels, pitch);
        }

        SDL_UnlockTexture(_texture);

        // Step 3: Render texture to screen (GPU Scaling)
        // Zoom: render a crop of the frame centered on the probe, scaled to
        // fill the window while preserving the frame aspect (letterboxed).
        const int maxZoom = std::max(1, std::min(frame._width, frame._height) / 48);
        if (_zoom > maxZoom) _zoom = maxZoom;
        const int cropW = std::max(1, frame._width / _zoom);
        const int cropH = std::max(1, frame._height / _zoom);
        const int probeFrameX = _probeX * frame._width / std::max(1, _winW);
        const int probeFrameY = _probeY * frame._height / std::max(1, _winH);
        _cropX = std::clamp(probeFrameX - cropW / 2, 0, frame._width - cropW);
        _cropY = std::clamp(probeFrameY - cropH / 2, 0, frame._height - cropH);

        // Aspect-correct destination rect (letterbox) inside the window
        const float fit = std::min((float)_winW / cropW, (float)_winH / cropH);
        _viewW = (int)(cropW * fit);
        _viewH = (int)(cropH * fit);
        _viewX = (_winW - _viewW) / 2;
        _viewY = (_winH - _viewH) / 2;

        SDL_SetRenderDrawColor(_renderer, 0, 0, 0, 255);
        SDL_RenderClear(_renderer);
        SDL_Rect srcRect = { _cropX, _cropY, cropW, cropH };
        SDL_Rect dstRect = { _viewX, _viewY, _viewW, _viewH };
        SDL_RenderCopy(_renderer, _texture, &srcRect, &dstRect);
        
        // Step 4: Render HUD
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
    
    void Renderer::ApplyColormap(const Thermal::ThermalFrame& frame, void* pixels, int pitch)
    {
        if (frame._data.empty() || !pixels) return;

        uint16_t minK = frame._min._kelvin;
        uint16_t maxK = frame._max._kelvin;

        if (minK >= maxK) {
            maxK = minK + 1;
        }

        int cmapIdx = _config.GetColormapIndex();
        if (cmapIdx < 0) cmapIdx = 0;
        const uint32_t* palette = _packedColormaps[cmapIdx % COLORMAP_COUNT].data();

        const uint16_t* input = frame._data.data();
        int width = frame._width;
        int height = frame._height;

        // Fast path for pitch == width * 4 (Contiguous)
        if (pitch == width * 4)
        {
            Thermal::Simd::ColormapLookup(
                input, static_cast<uint32_t*>(pixels),
                static_cast<size_t>(width) * height, minK, maxK, palette);
        }
        else
        {
            // Slow path (Row-by-row)
            uint8_t* rowPtr = static_cast<uint8_t*>(pixels);
            for (int y = 0; y < height; ++y)
            {
                Thermal::Simd::ColormapLookup(
                    input + y * width, reinterpret_cast<uint32_t*>(rowPtr),
                    width, minK, maxK, palette);
                rowPtr += pitch;
            }
        }
    }
    
    void Renderer::InitializeColormaps()
    {
        _packedColormaps.clear();
        _packedColormaps.reserve(COLORMAP_COUNT);

        for (int cmapIdx = 0; cmapIdx < COLORMAP_COUNT; ++cmapIdx)
        {
            const ColormapEntry& map = AVAILABLE_COLORMAPS[cmapIdx];
            std::vector<uint32_t> packed(256);

            for (int i = 0; i < 256; ++i)
            {
                const auto& color = map.data[i];
                packed[i] = (255 << 24) | (color.r << 16) | (color.g << 8) | color.b;
            }

            _packedColormaps.push_back(std::move(packed));
        }
    }

    // Builds a horizontal glyph atlas texture from a generated EspySans font namespace.
    template <typename GlyphT>
    SDL_Texture* BuildFontAtlas(SDL_Renderer* renderer, const GlyphT (&glyphs)[256], std::vector<SDL_Rect>& glyphRects)
    {
        int totalWidth = 0;
        int maxHeight = 0;

        glyphRects.resize(256);

        // First pass: calculate dimensions
        for (int i = 0; i < 256; ++i)
        {
            const auto& glyph = glyphs[i];
            if (!glyph.bitmap) {
                // For non-renderable chars (like space), we still need an advance but no bitmap
                glyphRects[i] = { totalWidth, 0, 0, 0 }; // No texture rect
                continue;
            }

            glyphRects[i].x = totalWidth;
            glyphRects[i].y = 0;
            glyphRects[i].w = glyph.width;
            glyphRects[i].h = glyph.height;

            totalWidth += glyph.width + 1; // +1 for padding
            if (glyph.height > maxHeight) maxHeight = glyph.height;
        }

        if (totalWidth == 0) return nullptr;

        SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, totalWidth, maxHeight, 32, SDL_PIXELFORMAT_ARGB8888);
        if (!surface)
        {
            std::cerr << "Failed to create font surface: " << SDL_GetError() << std::endl;
            return nullptr;
        }

        std::memset(surface->pixels, 0, surface->pitch * surface->h);

        uint32_t* pixels = (uint32_t*)surface->pixels;
        int pitch = surface->pitch / 4; // in uint32 pixels

        // Second pass: draw glyphs to surface
        for (int i = 0; i < 256; ++i)
        {
            const auto& glyph = glyphs[i];
            if (!glyph.bitmap) continue;

            const SDL_Rect& rect = glyphRects[i];

            int bytesPerRow = (glyph.width + 7) / 8;

            for (int r = 0; r < glyph.height; ++r) {
                for (int c = 0; c < glyph.width; ++c) {
                    int byteIndex = r * bytesPerRow + (c / 8);
                    int bitIndex = 7 - (c % 8);

                    if (glyph.bitmap[byteIndex] & (1 << bitIndex)) {
                        // Set pixel to white (color modulation is applied at draw time)
                        int surfX = rect.x + c;
                        int surfY = rect.y + r;
                        pixels[surfY * pitch + surfX] = 0xFFFFFFFF;
                    }
                }
            }
        }

        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
        SDL_FreeSurface(surface);

        if (!texture) {
            std::cerr << "Failed to create font texture: " << SDL_GetError() << std::endl;
            return nullptr;
        }

        SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
        return texture;
    }

    void Renderer::InitializeFont()
    {
        // EspySans regular for labels, bold for temperature readouts
        _fontTexture = BuildFontAtlas(_renderer, Fonts::EspySans_14::glyphs, _glyphRects);
        _fontTextureBold = BuildFontAtlas(_renderer, Fonts::EspySans_Bold_14::glyphs, _glyphRectsBold);
    }

    void Renderer::DrawText(const std::string& text, int x, int y, uint32_t color, bool bold)
    {
        SDL_Texture* texture = bold ? _fontTextureBold : _fontTexture;
        if (!texture) return;

        // Set texture color modulation
        uint8_t a = (color >> 24) & 0xFF;
        uint8_t r = (color >> 16) & 0xFF;
        uint8_t g = (color >> 8) & 0xFF;
        uint8_t b = (color & 0xFF);

        SDL_SetTextureColorMod(texture, r, g, b);
        SDL_SetTextureAlphaMod(texture, a);

        int fs = _fontScale;
        int pen_x = x;
        int pen_y = y;

        for (char c : text) {
            uint8_t idx = static_cast<uint8_t>(c);

            int yOff, gh, advance;
            bool hasBitmap;
            if (bold) {
                const auto& glyph = Fonts::EspySans_Bold_14::glyphs[idx];
                yOff = glyph.y_offset; gh = glyph.height;
                advance = glyph.advance; hasBitmap = glyph.bitmap != nullptr;

                if (hasBitmap) {
                    const SDL_Rect& srcRect = _glyphRectsBold[idx];
                    SDL_Rect dstRect;
                    dstRect.x = pen_x + glyph.x_offset * fs;
                    dstRect.y = pen_y - (yOff + gh) * fs;
                    dstRect.w = srcRect.w * fs;
                    dstRect.h = srcRect.h * fs;
                    SDL_RenderCopy(_renderer, texture, &srcRect, &dstRect);
                }
            } else {
                const auto& glyph = Fonts::EspySans_14::glyphs[idx];
                yOff = glyph.y_offset; gh = glyph.height;
                advance = glyph.advance; hasBitmap = glyph.bitmap != nullptr;

                if (hasBitmap) {
                    const SDL_Rect& srcRect = _glyphRects[idx];
                    SDL_Rect dstRect;
                    dstRect.x = pen_x + glyph.x_offset * fs;
                    dstRect.y = pen_y - (yOff + gh) * fs;
                    dstRect.w = srcRect.w * fs;
                    dstRect.h = srcRect.h * fs;
                    SDL_RenderCopy(_renderer, texture, &srcRect, &dstRect);
                }
            }

            pen_x += advance * fs;
        }
    }

    int Renderer::TextWidth(const std::string& text, bool bold) const
    {
        const auto& regular = Fonts::EspySans_14::glyphs;
        const auto& boldGlyphs = Fonts::EspySans_Bold_14::glyphs;
        int w = 0;
        for (char c : text) {
            w += (bold ? boldGlyphs[static_cast<uint8_t>(c)].advance
                       : regular[static_cast<uint8_t>(c)].advance);
        }
        return w * _fontScale;
    }

    int Renderer::ProbeStep() const
    {
        return std::max(1, _config.GetScaleFactor() / 2); // fine control in frame pixels
    }

    void Renderer::MoveProbe(int dx, int dy)
    {
        // Probe lives in real window pixels; starts from the window center
        if (!_probeActive)
        {
            _probeX = _winW / 2;
            _probeY = _winH / 2;
            _probeActive = true;
        }
        _probeX = std::clamp(_probeX + dx, 0, std::max(0, _winW - 1));
        _probeY = std::clamp(_probeY + dy, 0, std::max(0, _winH - 1));
    }

    void Renderer::ApplyStickMovement()
    {
        // Analog stick: 1 frame-pixel per frame at deadzone, faster the further pushed
        const int deadzone = 6000;
        if (std::abs(_stickX) > deadzone)
        {
            MoveProbe((_stickX / 16000) * ProbeStep(), 0);
        }
        if (std::abs(_stickY) > deadzone)
        {
            MoveProbe(0, (_stickY / 16000) * ProbeStep());
        }
    }

    void Renderer::RenderHUD(const Thermal::ThermalFrame& frame)
    {
        int fs = _fontScale; // font scale (14pt base, grows with window height)
        int width = _winW;   // HUD overlays the entire window
        int height = _winH;
        int margin = 10 * fs;
        int lineStep = 19 * fs;
        int topBase = 24 * fs; // baseline of first HUD line

        // Probe position on screen: through the zoom crop into the letterbox rect
        const int frameX = _probeX * frame._width / std::max(1, _winW);
        const int frameY = _probeY * frame._height / std::max(1, _winH);
        const float fit = std::min((float)_winW / std::max(1, frame._width / _zoom),
                                   (float)_winH / std::max(1, frame._height / _zoom));
        int crossX = _viewX + (int)((frameX - _cropX) * fit);
        int crossY = _viewY + (int)((frameY - _cropY) * fit);

        // Fall back to frame center when the probe has never been moved
        if (!_probeActive) {
            crossX = _viewX + _viewW / 2;
            crossY = _viewY + _viewH / 2;
        }
        crossX = std::clamp(crossX, _viewX, _viewX + _viewW);
        crossY = std::clamp(crossY, _viewY, _viewY + _viewH);

        // Crosshair marks the measured spot
        int size = 10 * fs;
        SDL_SetRenderDrawColor(_renderer, 255, 255, 255, 255);
        SDL_RenderDrawLine(_renderer, crossX - size, crossY, crossX + size, crossY);
        SDL_RenderDrawLine(_renderer, crossX, crossY - size, crossX, crossY + size);
        
        bool useCelsius = _config.GetUseCelsius();
        
        char buffer[32];
        auto formatTemp = [useCelsius, &buffer](float c, float f) -> const char* {
             if (useCelsius) snprintf(buffer, sizeof(buffer), "%.1fC", c);
             else snprintf(buffer, sizeof(buffer), "%.1fF", f);
             return buffer;
        };

        // Top Left: FPS and Temps
        snprintf(buffer, sizeof(buffer), "FPS: %d", _currentFPS);
        DrawText(buffer, margin, topBase, 0xFFFFFFFF);
        
        // Labels in regular, temperature values in bold
        auto drawLabeledTemp = [&](const char* label, const char* value, int x, int y, uint32_t color) {
            DrawText(label, x, y, 0xFFFFFFFF);
            DrawText(value, x + TextWidth(label), y, color, /*bold=*/true);
        };

        drawLabeledTemp("Min: ", formatTemp(frame._min._celsius, frame._min._fahrenheit), margin, topBase + lineStep, 0xFF00FFFF); // Cyan

        drawLabeledTemp("Max: ", formatTemp(frame._max._celsius, frame._max._fahrenheit), margin, topBase + 2 * lineStep, 0xFFFF0000); // Red

        drawLabeledTemp("Avg: ", formatTemp(frame._avg._celsius, frame._avg._fahrenheit), margin, topBase + 3 * lineStep, 0xFF00FF00); // Green
        
        // Top Right: Colormap & Scale (right-aligned)
        int cmapIdx = _config.GetColormapIndex();
        if (cmapIdx < 0) cmapIdx = 0;
        const std::string cmapName = AVAILABLE_COLORMAPS[cmapIdx % COLORMAP_COUNT].name;
        int rightX = width - margin - TextWidth(cmapName);
        DrawText(cmapName, rightX, topBase);
        
        snprintf(buffer, sizeof(buffer), "Zoom: %dx", _zoom);
        rightX = width - margin - TextWidth(buffer);
        DrawText(buffer, rightX, topBase + lineStep);
        
        if (_config.GetFreezeFrame()) {
            const std::string frozen = "[FROZEN]";
            DrawText(frozen, width - margin - TextWidth(frozen), topBase + 2 * lineStep, 0xFF00FFFF);
        }

        // Single measured-temperature readout at the crosshair.
        // Shows the probe temperature when active, frame center otherwise.
        {
            int readX = frameX, readY = frameY;
            if (!_probeActive) {
                readX = (frame._width * _zoom) / 2 + _cropX;
                readY = (frame._height * _zoom) / 2 + _cropY;
            }

            if (readX >= 0 && readX < frame._width && readY >= 0 && readY < frame._height) {
                Thermal::Temperature t = Thermal::ThermalProcessor::GetTemperatureAt(frame, readY, readX);

                std::string probeT = formatTemp(t._celsius, t._fahrenheit);
                DrawText(probeT, crossX + 12 * fs, crossY + 12 * fs, 0xFFFFFFFF, /*bold=*/true);

                // Small marker box at the exact measured point
                SDL_SetRenderDrawColor(_renderer, 255, 255, 0, 255);
                SDL_Rect rect = {crossX - 2 * fs, crossY - 2 * fs, 5 * fs, 5 * fs};
                 SDL_RenderDrawRect(_renderer, &rect);
            }
        }

        // Bottom: Controls hint
        if (height > 100) {
            DrawText("A:Map X:Frz Y:Unit R:Rot L2/R2:Zoom", margin, height - 12 * fs);
            DrawText("[H]UD Sel:HUD Start:Quit +/-:Zoom", margin, height - 26 * fs);
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
                // SDL maps window coords to our logical size automatically
                _probeX = event.motion.x;
                _probeY = event.motion.y;
                _probeActive = true;
            }
            else if (event.type == SDL_MOUSEBUTTONDOWN)
            {
                if (event.button.button == SDL_BUTTON_RIGHT) {
                    _isProbeEnabled = !_isProbeEnabled;
                }
            }
            else if (event.type == SDL_CONTROLLERBUTTONDOWN)
            {
                switch (event.cbutton.button)
                {
                    case SDL_CONTROLLER_BUTTON_START:
                    case SDL_CONTROLLER_BUTTON_GUIDE:
                        _isRunning = false;
                        break;
                    case SDL_CONTROLLER_BUTTON_BACK:
                        _showHud = !_showHud;
                        break;
                    case SDL_CONTROLLER_BUTTON_DPAD_UP:
                        MoveProbe(0, -ProbeStep());
                        break;
                    case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
                        MoveProbe(0, ProbeStep());
                        break;
                    case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
                        MoveProbe(-ProbeStep(), 0);
                        break;
                    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
                        MoveProbe(ProbeStep(), 0);
                        break;
                    case SDL_CONTROLLER_BUTTON_A:
                    case SDL_CONTROLLER_BUTTON_B:
                    {
                        int c = _config.GetColormapIndex();
                        int dir = (event.cbutton.button == SDL_CONTROLLER_BUTTON_A) ? 1 : -1;
                        _config.SetColormapIndex((c + COLORMAP_COUNT + dir) % COLORMAP_COUNT);
                    }
                        break;
                    case SDL_CONTROLLER_BUTTON_X:
                        _config.SetFreezeFrame(!_config.GetFreezeFrame());
                        break;
                    case SDL_CONTROLLER_BUTTON_Y:
                        _config.SetUseCelsius(!_config.GetUseCelsius());
                        break;
                    case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
                        _isProbeEnabled = !_isProbeEnabled;
                        break;
                    case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
                    {
                        int r = _config.GetRotation();
                        _config.SetRotation((r + 1) % 4);
                    }
                        break;
                }
            }
            else if (event.type == SDL_CONTROLLERAXISMOTION)
            {
                if (event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTX)
                {
                    _stickX = event.caxis.value;
                }
                else if (event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTY)
                {
                    _stickY = event.caxis.value;
                }
                else if (event.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT)
                {
                    const bool held = event.caxis.value > 8000;
                    if (held && !_prevR2) ZoomIn();
                    _prevR2 = held;
                }
                else if (event.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT)
                {
                    const bool held = event.caxis.value > 8000;
                    if (held && !_prevL2) ZoomOut();
                    _prevL2 = held;
                }
            }
            else if (event.type == SDL_CONTROLLERDEVICEADDED)
            {
                SDL_GameControllerOpen(event.cdevice.which);
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
                         ZoomIn();
                         break;
                    case SDLK_MINUS:
                    case SDLK_KP_MINUS:
                         ZoomOut();
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

        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) != 0)
        {
            LOG_ERROR(std::string("SDL_Init Error: ") + SDL_GetError());
            return false;
        }

        // Open all connected gamepads (handheld console controls)
        for (int i = 0; i < SDL_NumJoysticks(); ++i)
        {
            if (SDL_IsGameController(i))
            {
                SDL_GameControllerOpen(i);
            }
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
        // Initialize pre-packed colormaps
        InitializeColormaps();
        InitializeFont();

        // Create texture at source resolution (256x192 usually)
        // SDL will handle scaling on GPU during RenderCopy
        _texture = SDL_CreateTexture(_renderer, SDL_PIXELFORMAT_ARGB8888, 
                                     SDL_TEXTUREACCESS_STREAMING, 
                                     width, height);
        
        _isRunning = true;
        _fpsTimer = SDL_GetTicks();
        
        std::cerr << "Renderer initialized successfully." << std::endl;
        return true;
    }
}
