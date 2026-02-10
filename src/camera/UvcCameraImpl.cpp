#include "UvcCameraImpl.hpp"

#include <stdexcept>
#include <iostream>

UvcCameraImpl::UvcCameraImpl()
    : _provider()
{
}

UvcCameraImpl::~UvcCameraImpl()
{
}

void UvcCameraImpl::Initialize(const std::string& devicePath)
{
    _provider.Initialize(devicePath);
}

void UvcCameraImpl::StartStreaming()
{
    _provider.StartStreaming();
}

void UvcCameraImpl::StopStreaming()
{
    _provider.StopStreaming();
}

void UvcCameraImpl::GetFrame(uint8_t* buffer, size_t bufferSize, size_t& bytesWritten)
{
    _provider.GetFrame(buffer, bufferSize, bytesWritten);
}


bool UvcCameraImpl::IsRunning() const
{
    return _provider.IsRunning();
}

std::string UvcCameraImpl::GetCameraName() const
{
    return _provider.GetCameraName();
}

uint16_t UvcCameraImpl::GetVendorId() const
{
    return _provider.GetVendorId();
}

uint16_t UvcCameraImpl::GetProductId() const
{
    return _provider.GetProductId();
}

bool UvcCameraImpl::SetBrightness(int value)
{
    return _provider.SetBrightness(value);
}

bool UvcCameraImpl::SetWhiteBalanceTemperature(int value)
{
    return _provider.SetWhiteBalanceTemperature(value);
}

bool UvcCameraImpl::SetBacklightCompensation(int value)
{
    return _provider.SetBacklightCompensation(value);
}

bool UvcCameraImpl::SetPowerLineFrequency(int value)
{
    return _provider.SetPowerLineFrequency(value);
}

bool UvcCameraImpl::SetSaturation(int value)
{
    return _provider.SetSaturation(value);
}

bool UvcCameraImpl::SetSharpness(int value)
{
    return _provider.SetSharpness(value);
}

bool UvcCameraImpl::SetContrast(int value)
{
    return _provider.SetContrast(value);
}

bool UvcCameraImpl::SetHue(int value)
{
    return _provider.SetHue(value);
}

bool UvcCameraImpl::SetGamma(int value)
{
    return _provider.SetGamma(value);
}

bool UvcCameraImpl::SetAutoWhiteBalanceTemperature(bool enable)
{
    return _provider.SetAutoWhiteBalanceTemperature(enable);
}

void UvcCameraImpl::ListCameras()
{
    UvcCameraProvider::ListCameras();
}

std::string UvcCameraImpl::FindThermalCamera()
{
    std::string result = UvcCameraProvider::FindThermalCamera();
    if (result.empty())
    {
        throw std::runtime_error("No thermal camera found (256x384 resolution)");
    }
    return result;
}
