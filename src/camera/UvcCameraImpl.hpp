#ifndef UVC_CAMERA_IMPL_HPP
#define UVC_CAMERA_IMPL_HPP

#include "ICamera.hpp"
#include "UvcCameraProvider.hpp"
#include <memory>
#include <stdexcept>

class UvcCameraImpl : public ICamera
{
public:
    UvcCameraImpl();
    virtual ~UvcCameraImpl();

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
    UvcCameraProvider _provider;
};

#endif
