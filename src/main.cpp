#include "camera/Camera.hpp"
#include "thermal/ThermalProcessor.hpp"
#include "render/Renderer.hpp"
#include "FrameBuffer.hpp"
#include "config/Config.hpp"
#include "Error.hpp"
#include "vendor/cmdline/cmdline.h"
#include "vendor/ctpl/ctpl_stl_tls.h"

#include "version.hpp"

#include <iostream>
#include <vector>
#include <atomic>
#include <chrono>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <csignal>
#include <memory>
#include <cstring>

// Shared State Structure
struct SharedState
{
    // Configuration
    Config::Config config;

    // Raw frame double buffer
    FrameBuffer rawBuffer;
    
    // Processed frame double buffer
    std::vector<Thermal::ThermalFrame> processedBuffer;
    std::atomic<int> processedWriteIndex;
    std::atomic<int> processedReadIndex;
    std::mutex processedMutex;
    std::condition_variable processedReady;
    
    // Application state flags
    std::atomic<bool> isRunning;
    std::atomic<bool> cameraConnected;
    std::atomic<bool> processingActive;
    bool isAutoDetect;

    SharedState(size_t frameSize) 
        : rawBuffer(frameSize)
        , processedBuffer(2)
        , processedWriteIndex(0)
        , processedReadIndex(1)
        , isRunning(true)
        , cameraConnected(false)
        , processingActive(true)
        , isAutoDetect(false)
    {
        // Pre-allocate thermal frames
        for(auto& frame : processedBuffer) {
             frame._data.resize(CameraConstants::FRAME_WIDTH * CameraConstants::FRAME_HEIGHT);
             frame._width = CameraConstants::FRAME_WIDTH;
             frame._height = CameraConstants::FRAME_HEIGHT;
        }
    }

    // Helper to get write buffer for processing thread
    Thermal::ThermalFrame& GetProcessedWriteFrame() {
        std::lock_guard<std::mutex> lock(processedMutex);
        return processedBuffer[processedWriteIndex];
    }

    // Helper to swap and signal render thread
    void SwapProcessedFrames() {
        std::lock_guard<std::mutex> lock(processedMutex);
        int temp = processedWriteIndex;
        processedWriteIndex = processedReadIndex.load();
        processedReadIndex = temp;
        processedReady.notify_one();
    }

    // Helper for render thread to get latest frame
    // Returns pointer to frame or nullptr if not ready/timeout
    const Thermal::ThermalFrame* GetProcessedReadFrame() {
        std::lock_guard<std::mutex> lock(processedMutex);
        return &processedBuffer[processedReadIndex];
    }

    // Signal shutdown
    void Shutdown() {
        isRunning = false;
        processingActive = false;
        // Wake up threads waiting on condition variables
        rawBuffer.NotifyAll();
        processedReady.notify_all();
    }
};

// Thread Local Storage
struct ThreadContext
{
    int threadId;
    std::string name;
    
    ThreadContext() : threadId(-1), name("Unknown") {}
};

// Global for signal handler
std::shared_ptr<SharedState> g_sharedState = nullptr;

void SignalHandler(int) {
    if (g_sharedState) {
        g_sharedState->Shutdown();
    }
}

int main(int argc, char** argv)
{
    try
    {
        // 1. Parse Arguments
        cmdline::parser cmd;
        cmd.add<std::string>("device", 'd', "camera device identifier (Linux: /dev/video0, macOS: location ID)", false, "");
        cmd.add<std::string>("file", 'f', "offline raw file input", false, "");
        cmd.add<int>("scale", 0, "initial scale factor (1-10)", false, 1);
        cmd.add("fullscreen", 0, "start in fullscreen mode");
        cmd.add<int>("colormap", 0, "initial colormap index (0-6)", false, 0);
        cmd.add("fahrenheit", 0, "use Fahrenheit units (default Celsius)");
        cmd.add("quiet", 0, "suppress verbose output");
        cmd.add<int>("threads", 0, "override thread count (default 3)", false, 3);
        cmd.add("list", 'l', "list all available cameras and their formats");
        cmd.add("help", 'h', "display this help and exit");
        cmd.add("version", 'v', "display version information");

        if (!cmd.parse(argc, argv) || cmd.exist("help"))
        {
            std::cerr << cmd.error_full() << cmd.usage();
            return 0;
        }

        if (cmd.exist("version"))
        {
            std::cout << "ThermalCamera v" << THERMALCAMERA_VERSION_STRING << "\n";
            return 0;
        }

        if (cmd.exist("list"))
        {
            Camera::ListCameras();
            return 0;
        }

        // Test timeout check via environment variable
        const char* timeoutEnv = std::getenv("TEST_TIMEOUT_SEC");
        int testTimeout = timeoutEnv ? std::atoi(timeoutEnv) : 0;
        auto startTime = std::chrono::steady_clock::now();

        // 2. Initialize Shared State
        auto sharedState = std::make_shared<SharedState>(CameraConstants::FRAME_SIZE);
        g_sharedState = sharedState; // Set global for signal handler
        
        // Apply Config
        sharedState->config.ApplyCLIArguments(cmd);
        sharedState->config.LogCurrentConfig();

        std::string devicePath = sharedState->config.GetDevicePath();

        if (devicePath.empty() && sharedState->config.GetInputFile().empty())
        {
            sharedState->isAutoDetect = true;
            LOG_INFO("Auto-detecting thermal camera...");
            devicePath = Camera::FindThermalCamera();
            if (devicePath.empty()) {
                // Don't exit yet, retry in loop
                LOG_WARN("No thermal camera found initially. Will retry.");
            } else {
                LOG_INFO(std::string("Found thermal camera at: ") + devicePath);
                sharedState->config.SetDevicePath(devicePath);
            }
        }
        else if (!devicePath.empty())
        {
            // User specified device
            sharedState->isAutoDetect = false;
        }

        // Setup signal handling
        std::signal(SIGINT, SignalHandler);
        std::signal(SIGTERM, SignalHandler);

        // 3. Initialize Thread Pool
        // Capture Thread, Processing Thread. Render runs on Main Thread.
        // We create pool of size (ThreadCount - 1) for workers.
        int workerCount = std::max(1, sharedState->config.GetThreadCount() - 1);
        ctpl::thread_pool_tls<ThreadContext> pool(workerCount, [](size_t id, std::shared_ptr<ThreadContext>& tls) {
            tls = std::make_shared<ThreadContext>();
            tls->threadId = static_cast<int>(id);
            tls->name = (id == 0) ? "Capture" : "Process"; 
            // Note: If more than 2 threads, others are generic workers or we need better assignment
        });

        // 4. Launch Processing Thread (Task 4.4)
        pool.push([sharedState](size_t id, [[maybe_unused]] ThreadContext& /*ctx*/) {
            std::cerr << "Processing thread started (ID: " << id << ")\n";
            Thermal::ThermalProcessor processor;
            
            while (sharedState->isRunning && sharedState->processingActive) {
                const uint8_t* sourceData = nullptr;
                bool isFrozen = sharedState->config.GetFreezeFrame();

                if (isFrozen) {
                    // If no frozen frame yet, capture one
                    if (!sharedState->config.GetFrozenFrameData()) {
                        // Wait for a fresh frame to freeze
                        sharedState->rawBuffer.WaitForNewFrame();
                        if (!sharedState->isRunning) break;
                        
                        const uint8_t* currentRaw = sharedState->rawBuffer.GetReadBuffer();
                        if (currentRaw) {
                            auto frozen = std::make_unique<uint8_t[]>(CameraConstants::FRAME_SIZE);
                            std::memcpy(frozen.get(), currentRaw, CameraConstants::FRAME_SIZE);
                            sharedState->config.SetFrozenFrameData(std::move(frozen));
                        }
                    }
                    sourceData = sharedState->config.GetFrozenFrameData();
                    
                    // Throttle re-processing of frozen frame to avoid busy loop
                    std::this_thread::sleep_for(std::chrono::milliseconds(33));
                } else {
                    // Clear frozen data if present (unfreeze)
                    if (sharedState->config.GetFrozenFrameData()) {
                        sharedState->config.SetFrozenFrameData(nullptr);
                    }
                    
                    // Normal processing
                    sharedState->rawBuffer.WaitForNewFrame();
                    if (!sharedState->isRunning) break;
                    sourceData = sharedState->rawBuffer.GetReadBuffer();
                }

                if (!sourceData) continue;

                // Update rotation from config
                processor.SetRotation(sharedState->config.GetRotation());

                // Get write buffer (Processed)
                Thermal::ThermalFrame& outputFrame = sharedState->GetProcessedWriteFrame();
                
                // Process
                // Note: processor.ProcessFrame might return false if frame invalid
                if (processor.ProcessFrame(sourceData, outputFrame)) {
                    // Swap to make available to renderer
                    sharedState->SwapProcessedFrames();
                }
            }
            std::cerr << "Processing thread stopped\n";
        });

        // 5. Launch Capture Thread (Task 4.3)
        pool.push([sharedState](size_t id, [[maybe_unused]] ThreadContext& ctx) {
            std::cerr << "Capture thread started (ID: " << id << ")\n";
            Camera camera;
            
            while (sharedState->isRunning) {
                try {
                    // 1. Connection Phase
                    if (!sharedState->cameraConnected) {
                        try {
                            std::string path = sharedState->config.GetDevicePath();
                            
                            // Re-detect if auto-detect mode and no path (or retry)
                            if (sharedState->isAutoDetect && path.empty()) {
                                std::cerr << "Scanning for camera...\r";
                                path = Camera::FindThermalCamera();
                                if (!path.empty()) {
                                    sharedState->config.SetDevicePath(path);
                                    std::cerr << "\nFound camera: " << path << "\n";
                                }
                            }
                            
                            // Check again
                            path = sharedState->config.GetDevicePath();
                            if (path.empty()) {
                                std::this_thread::sleep_for(std::chrono::seconds(1));
                                continue;
                            }

                            camera.Initialize(path);
                            std::cerr << "Camera initialized: " << camera.GetCameraName() << "\n";
                            
                            // Set default settings
                            camera.SetBrightness(50);
                            camera.SetContrast(50);
                            camera.SetGamma(300); // 3.0
                            
                            camera.StartStreaming();
                            sharedState->cameraConnected = true;
                        } catch (const std::exception& e) {
                            // Connection failed, wait and retry
                            if (sharedState->isRunning) {
                                std::cerr << "Connection failed: " << e.what() << " - retrying in 1s\n";
                                // If auto-detect, clear the path to force re-scan next loop
                                // This handles Location ID changes on macOS
                                if (sharedState->isAutoDetect) {
                                    sharedState->config.SetDevicePath("");
                                }
                                std::this_thread::sleep_for(std::chrono::seconds(1));
                            }
                            continue; 
                        }
                    }

                    // 2. Streaming Phase
                    uint8_t* writeBuffer = sharedState->rawBuffer.GetWriteBuffer();
                    size_t frameSize = sharedState->rawBuffer.GetFrameSize();
                    size_t bytesWritten = 0;
                    
                    // Blocks until new frame or timeout
                    camera.GetFrame(writeBuffer, frameSize, bytesWritten);

                    if (bytesWritten == frameSize) {
                        sharedState->rawBuffer.SwapBuffers();
                    } else if (bytesWritten == 0) {
                         // Timeout - just loop again
                    } else {
                         std::cerr << "Warning: Frame size mismatch (" << bytesWritten << " vs " << frameSize << ")\n";
                    }

                } catch (const std::exception& e) {
                    std::cerr << "Capture error: " << e.what() << "\n";
                    sharedState->cameraConnected = false;
                    try { camera.StopStreaming(); } catch(...) {}
                    
                    // Wait before retrying
                    if (sharedState->isRunning) {
                        // If auto-detect, clear the path to force re-scan next loop
                        if (sharedState->isAutoDetect) {
                            sharedState->config.SetDevicePath("");
                        }
                        std::this_thread::sleep_for(std::chrono::seconds(1));
                    }
                }
            }
            std::cerr << "Capture thread stopped\n";
        });

        // 6. Start Render Loop (Main Thread) (Task 4.5 & 4.7)
        LOG_INFO("Initializing Renderer...");
        Render::Renderer renderer(sharedState->config);
        if (!renderer.Initialize("Thermal Camera", CameraConstants::FRAME_WIDTH, CameraConstants::FRAME_HEIGHT)) {
            LOG_ERROR("Failed to initialize renderer");
            sharedState->Shutdown();
            return 1;
        }

        // FPS Calculation
        auto lastTime = std::chrono::high_resolution_clock::now();
        int frames = 0;
        
        while (sharedState->isRunning && renderer.IsRunning()) {
            // Wait for processed frame with timeout (to keep UI responsive)
            {
                std::unique_lock<std::mutex> lock(sharedState->processedMutex);
                if (sharedState->processedReady.wait_for(lock, std::chrono::milliseconds(100)) == std::cv_status::no_timeout) {
                    // We have a new frame
                    const Thermal::ThermalFrame* frame = &sharedState->processedBuffer[sharedState->processedReadIndex];
                    if (frame) {
                        renderer.RenderFrame(*frame);
                    }
                }
            }

            // Handle Events
            renderer.HandleEvents();
            
            // FPS
            frames++;
            auto now = std::chrono::high_resolution_clock::now(); // Keep high_res for FPS
            auto diff = std::chrono::duration_cast<std::chrono::seconds>(now - lastTime);
            if (diff.count() >= 1) {
                if (!cmd.exist("quiet")) {
                    std::cout << "FPS: " << frames << "\r" << std::flush;
                }
                frames = 0;
                lastTime = now;
            }

            // Test timeout check
            if (testTimeout > 0) {
                 auto runningTime = std::chrono::steady_clock::now() - startTime;
                 if (std::chrono::duration_cast<std::chrono::seconds>(runningTime).count() >= testTimeout) {
                     std::cerr << "Test timeout reached (" << testTimeout << "s). Exiting.\n";
                     break;
                 }
            }
            
            // Yield a bit if loop is too fast (though wait_for handles it)
        }

        std::cerr << "\nShutting down...\n";
        sharedState->Shutdown();
        
        // Wait for threads to finish
        // pool destructor will join threads
        
        renderer.Shutdown();
        
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Fatal Error: " << e.what() << "\n";
        return 1;
    }
}
