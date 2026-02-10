#ifndef MAC_CAMERA_IMPL_HPP
#define MAC_CAMERA_IMPL_HPP

#include "ICamera.hpp"
#include "ObjCPtr.hpp"
#include <memory>
#include <string>

class MacCameraImpl : public ICamera
{
public:
    MacCameraImpl();
    virtual ~MacCameraImpl();

    virtual void Initialize(const std::string& devicePath) override;
    virtual void StartStreaming() override;
    virtual void StopStreaming() override;
    virtual void GetFrame(uint8_t* buffer, size_t bufferSize, size_t& bytesWritten) override;
    virtual bool IsRunning() const override;
    virtual std::string GetCameraName() const override;
    virtual uint16_t GetVendorId() const override;
    virtual uint16_t GetProductId() const override;

    virtual bool SetBrightness(int value) override;
    virtual bool SetWhiteBalanceTemperature(int value) override;
    virtual bool SetBacklightCompensation(int value) override;
    virtual bool SetPowerLineFrequency(int value) override;
    virtual bool SetSaturation(int value) override;
    virtual bool SetSharpness(int value) override;
    virtual bool SetContrast(int value) override;
    virtual bool SetHue(int value) override;
    virtual bool SetGamma(int value) override;
    virtual bool SetAutoWhiteBalanceTemperature(bool enable) override;

    static void ListCameras();
    static std::string FindThermalCamera();

private:
    std::unique_ptr<void, ObjCDeleter> _provider;
};

#endif
