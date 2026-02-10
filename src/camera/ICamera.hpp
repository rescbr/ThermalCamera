#ifndef ICAMERA_HPP
#define ICAMERA_HPP

#include <cstdint>
#include <string>
#include <stdexcept>

class ICamera
{
public:
    virtual ~ICamera() = default;

    virtual void Initialize(const std::string& devicePath) = 0;
    virtual void StartStreaming() = 0;
    virtual void StopStreaming() = 0;

    // Get the latest frame into the provided buffer
    // buffer: Caller-allocated buffer to write frame data into
    // bufferSize: Size of the buffer in bytes
    // bytesWritten: Output - number of bytes written
    // Throws std::runtime_error if no frame is available or buffer is too small
    virtual void GetFrame(uint8_t* buffer, size_t bufferSize, size_t& bytesWritten) = 0;

    virtual bool IsRunning() const = 0;
    virtual std::string GetCameraName() const = 0;
    virtual uint16_t GetVendorId() const = 0;
    virtual uint16_t GetProductId() const = 0;

    virtual bool SetBrightness(int value) = 0;
    virtual bool SetWhiteBalanceTemperature(int value) = 0;
    virtual bool SetBacklightCompensation(int value) = 0;
    virtual bool SetPowerLineFrequency(int value) = 0;
    virtual bool SetSaturation(int value) = 0;
    virtual bool SetSharpness(int value) = 0;
    virtual bool SetContrast(int value) = 0;
    virtual bool SetHue(int value) = 0;
    virtual bool SetGamma(int value) = 0;
    virtual bool SetAutoWhiteBalanceTemperature(bool enable) = 0;

    static void ListCameras();
    static std::string FindThermalCamera();
};

#endif
