#ifndef UVC_CAMERA_PROVIDER_HPP
#define UVC_CAMERA_PROVIDER_HPP

#include <cstdint>
#include <string>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>
#include <stdexcept>
#include <libuvc/libuvc.h>

class UvcCameraProvider
{
public:
    UvcCameraProvider();
    ~UvcCameraProvider();

    void Initialize(const std::string& devicePath);
    void StartStreaming();
    void StopStreaming();
    void GetFrame(uint8_t* buffer, size_t bufferSize, size_t& bytesWritten);
    bool IsRunning() const;
    static void ListCameras();
    static std::string FindThermalCamera();
    
    std::string GetCameraName() const;
    uint16_t GetVendorId() const;
    uint16_t GetProductId() const;

    bool SetBrightness(int value);
    bool SetWhiteBalanceTemperature(int value);
    bool SetBacklightCompensation(int value);
    bool SetPowerLineFrequency(int value);
    bool SetSaturation(int value);
    bool SetSharpness(int value);
    bool SetContrast(int value);
    bool SetHue(int value);
    bool SetGamma(int value);
    bool SetAutoWhiteBalanceTemperature(bool enable);

    int GetBrightness(int* value);
    int GetWhiteBalanceTemperature(int* value);
    int GetBacklightCompensation(int* value);
    int GetPowerLineFrequency(int* value);
    int GetSaturation(int* value);
    int GetSharpness(int* value);
    int GetContrast(int* value);
    int GetHue(int* value);
    int GetGamma(int* value);
    int GetAutoWhiteBalanceTemperature(int* value);

private:
    uvc_context_t* _context;
    uvc_device_t* _device;
    uvc_device_handle_t* _deviceHandle;
    uvc_stream_handle_t* _streamHandle;
    uvc_stream_ctrl_t _streamCtrl;
    
    std::string _cameraName;
    uint16_t _vendorId;
    uint16_t _productId;
    
    std::unique_ptr<uint8_t[]> _frameBuffer;
    size_t _frameSize;
    uint64_t _frameSequence;
    uint64_t _lastSequenceSeen;
    std::mutex _frameMutex;
    std::condition_variable _frameCV;
    
    std::mutex _controlMutex;
    
    std::atomic<bool> _isRunning;
};

#endif
