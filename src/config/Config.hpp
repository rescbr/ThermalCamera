#pragma once

#include <string>
#include <iostream>
#include <memory>
#include <cstdint>
#include "../vendor/cmdline/cmdline.h"

namespace Config {

class Config {
public:
    Config() {
        SetDefaults();
    }

    // Getters
    const std::string& GetDevicePath() const { return _devicePath; }
    const std::string& GetInputFile() const { return _inputFile; }
    int GetScaleFactor() const { return _scaleFactor; }
    bool GetFullscreen() const { return _fullscreen; }
    int GetColormapIndex() const { return _colormapIndex; }
    bool GetUseCelsius() const { return _useCelsius; }
    int GetRotation() const { return _rotation; }
    bool GetFreezeFrame() const { return _freezeFrame; }
    int GetThreadCount() const { return _threadCount; }
    bool GetVerbose() const { return _verbose; }
    const uint8_t* GetFrozenFrameData() const { return _frozenFrame.get(); }

    // Setters
    void SetDevicePath(const std::string& path) { _devicePath = path; }
    void SetInputFile(const std::string& path) { _inputFile = path; }
    void SetScaleFactor(int scale) { _scaleFactor = scale; }
    void SetFullscreen(bool fs) { _fullscreen = fs; }
    void SetColormapIndex(int index) { _colormapIndex = index; }
    void SetUseCelsius(bool celsius) { _useCelsius = celsius; }
    void SetRotation(int rot) { _rotation = rot; }
    void SetFreezeFrame(bool freeze) { _freezeFrame = freeze; }
    void SetThreadCount(int threads) { _threadCount = threads; }
    void SetVerbose(bool verbose) { _verbose = verbose; }
    
    // Move ownership of frozen frame data
    void SetFrozenFrameData(std::unique_ptr<uint8_t[]> data) { 
        _frozenFrame = std::move(data); 
    }

    void SetDefaults() {
        #ifdef __APPLE__
        _devicePath = "";
#else
        _devicePath = "/dev/video0";
#endif
        _inputFile = "";
        _scaleFactor = 1; // Default 1X
        _fullscreen = false;
        _colormapIndex = 0; // Default
        _useCelsius = false; // Default Fahrenheit
        _rotation = 0;
        _freezeFrame = false;
        _threadCount = 3;
        _verbose = true;
        _frozenFrame.reset();
    }

    void ApplyCLIArguments(const cmdline::parser& cmd) {
        if (cmd.exist("device")) _devicePath = cmd.get<std::string>("device");
        if (cmd.exist("file")) _inputFile = cmd.get<std::string>("file");
        
        if (cmd.exist("scale")) {
            int scale = cmd.get<int>("scale");
            if (scale >= 1 && scale <= 10) {
                _scaleFactor = scale;
            } else {
                if (_verbose) std::cerr << "Warning: Invalid scale factor (1-10). Using default.\n";
            }
        }
        
        if (cmd.exist("fullscreen")) _fullscreen = true;
        
        if (cmd.exist("colormap")) {
            int cmap = cmd.get<int>("colormap");
            if (cmap >= 0 && cmap <= 36) {
                _colormapIndex = cmap;
            } else {
                if (_verbose) std::cerr << "Warning: Invalid colormap index (0-36). Using default.\n";
            }
        }
        
        if (cmd.exist("celsius")) _useCelsius = true;
        if (cmd.exist("quiet")) _verbose = false;
        
        if (cmd.exist("threads")) {
            int threads = cmd.get<int>("threads");
            if (threads > 0) {
                _threadCount = threads;
            }
        }
    }

    void LogCurrentConfig() const {
        if (!_verbose) return;
        std::cerr << "Current Configuration:\n";
        std::cerr << "  Device: " << _devicePath << "\n";
        if (!_inputFile.empty()) std::cerr << "  Input File: " << _inputFile << "\n";
        std::cerr << "  Scale: " << _scaleFactor << "X\n";
        std::cerr << "  Fullscreen: " << (_fullscreen ? "Yes" : "No") << "\n";
        std::cerr << "  Colormap Index: " << _colormapIndex << "\n";
        std::cerr << "  Units: " << (_useCelsius ? "Celsius" : "Fahrenheit") << "\n";
        std::cerr << "  Rotation: " << _rotation << "\n";
        std::cerr << "  Threads: " << _threadCount << "\n";
    }

private:
    std::string _devicePath;
    std::string _inputFile;
    int _scaleFactor;
    bool _fullscreen;
    int _colormapIndex;
    bool _useCelsius;
    int _rotation;
    bool _freezeFrame;
    int _threadCount;
    bool _verbose;
    std::unique_ptr<uint8_t[]> _frozenFrame;
};

} // namespace Config
