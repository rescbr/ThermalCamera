#import "MacCameraImpl.hpp"
#import "MacCameraProvider.hpp"
#import <Foundation/Foundation.h>

#include <iostream>
#include <string>

void ObjCDeleter::operator()(void* ptr) const
{
    if (ptr)
    {
        CFRelease(ptr);
    }
}

MacCameraImpl::MacCameraImpl()
    : _provider((__bridge_retained void*)[[MacCameraProvider alloc] init], ObjCDeleter{})
{
}

MacCameraImpl::~MacCameraImpl()
{
}

void MacCameraImpl::Initialize(const std::string& devicePath)
{
    @try {
        if (![(__bridge MacCameraProvider*)_provider.get() initialize:devicePath])
        {
            std::string msg = "Failed to initialize camera: " + devicePath;
            throw std::runtime_error(msg);
        }
    }
    @catch (NSException *exception) {
        std::string msg = "Objective-C exception in Initialize: " + std::string([exception.reason UTF8String]);
        throw std::runtime_error(msg);
    }
}

void MacCameraImpl::StartStreaming()
{
    @try {
        if (![(__bridge MacCameraProvider*)_provider.get() startStreaming])
        {
            throw std::runtime_error("Failed to start streaming");
        }
    }
    @catch (NSException *exception) {
        std::string msg = "Objective-C exception in StartStreaming: " + std::string([exception.reason UTF8String]);
        throw std::runtime_error(msg);
    }
}

void MacCameraImpl::StopStreaming()
{
    @try {
        [(__bridge MacCameraProvider*)_provider.get() stopStreaming];
    }
    @catch (NSException *exception) {
        std::cerr << "[ERROR] Objective-C exception in StopStreaming: " << [exception.reason UTF8String] << std::endl;
    }
}

void MacCameraImpl::GetFrame(uint8_t* buffer, size_t bufferSize, size_t& bytesWritten)
{
    @try {
        [(__bridge MacCameraProvider*)_provider.get() getFrame:buffer size:bufferSize bytesWritten:&bytesWritten];
    }
    @catch (NSException *exception) {
        std::string msg = "Objective-C exception in GetFrame: " + std::string([exception.reason UTF8String]);
        throw std::runtime_error(msg);
    }
}


bool MacCameraImpl::IsRunning() const
{
    return [(__bridge MacCameraProvider*)_provider.get() isRunning];
}

std::string MacCameraImpl::GetCameraName() const
{
    return [(__bridge MacCameraProvider*)_provider.get() getCameraName];
}

uint16_t MacCameraImpl::GetVendorId() const
{
    return [(__bridge MacCameraProvider*)_provider.get() getVendorId];
}

uint16_t MacCameraImpl::GetProductId() const
{
    return [(__bridge MacCameraProvider*)_provider.get() getProductId];
}

bool MacCameraImpl::SetBrightness(int value)
{
    @try {
        return [(__bridge MacCameraProvider*)_provider.get() setBrightness:value];
    }
    @catch (NSException *exception) {
        std::cerr << "[ERROR] Objective-C exception in SetBrightness: " << [exception.reason UTF8String] << std::endl;
        return false;
    }
}

bool MacCameraImpl::SetWhiteBalanceTemperature(int value)
{
    @try {
        return [(__bridge MacCameraProvider*)_provider.get() setWhiteBalanceTemperature:value];
    }
    @catch (NSException *exception) {
        std::cerr << "[ERROR] Objective-C exception in SetWhiteBalanceTemperature: " << [exception.reason UTF8String] << std::endl;
        return false;
    }
}

bool MacCameraImpl::SetBacklightCompensation(int value)
{
    @try {
        return [(__bridge MacCameraProvider*)_provider.get() setBacklightCompensation:value];
    }
    @catch (NSException *exception) {
        std::cerr << "[ERROR] Objective-C exception in SetBacklightCompensation: " << [exception.reason UTF8String] << std::endl;
        return false;
    }
}

bool MacCameraImpl::SetPowerLineFrequency(int value)
{
    @try {
        return [(__bridge MacCameraProvider*)_provider.get() setPowerLineFrequency:value];
    }
    @catch (NSException *exception) {
        std::cerr << "[ERROR] Objective-C exception in SetPowerLineFrequency: " << [exception.reason UTF8String] << std::endl;
        return false;
    }
}

bool MacCameraImpl::SetSaturation(int value)
{
    @try {
        return [(__bridge MacCameraProvider*)_provider.get() setSaturation:value];
    }
    @catch (NSException *exception) {
        std::cerr << "[ERROR] Objective-C exception in SetSaturation: " << [exception.reason UTF8String] << std::endl;
        return false;
    }
}

bool MacCameraImpl::SetSharpness(int value)
{
    @try {
        return [(__bridge MacCameraProvider*)_provider.get() setSharpness:value];
    }
    @catch (NSException *exception) {
        std::cerr << "[ERROR] Objective-C exception in SetSharpness: " << [exception.reason UTF8String] << std::endl;
        return false;
    }
}

bool MacCameraImpl::SetContrast(int value)
{
    @try {
        return [(__bridge MacCameraProvider*)_provider.get() setContrast:value];
    }
    @catch (NSException *exception) {
        std::cerr << "[ERROR] Objective-C exception in SetContrast: " << [exception.reason UTF8String] << std::endl;
        return false;
    }
}

bool MacCameraImpl::SetHue(int value)
{
    @try {
        return [(__bridge MacCameraProvider*)_provider.get() setHue:value];
    }
    @catch (NSException *exception) {
        std::cerr << "[ERROR] Objective-C exception in SetHue: " << [exception.reason UTF8String] << std::endl;
        return false;
    }
}

bool MacCameraImpl::SetGamma(int value)
{
    @try {
        return [(__bridge MacCameraProvider*)_provider.get() setGamma:value];
    }
    @catch (NSException *exception) {
        std::cerr << "[ERROR] Objective-C exception in SetGamma: " << [exception.reason UTF8String] << std::endl;
        return false;
    }
}

bool MacCameraImpl::SetAutoWhiteBalanceTemperature(bool enable)
{
    @try {
        return [(__bridge MacCameraProvider*)_provider.get() setAutoWhiteBalanceTemperature:enable];
    }
    @catch (NSException *exception) {
        std::cerr << "[ERROR] Objective-C exception in SetAutoWhiteBalanceTemperature: " << [exception.reason UTF8String] << std::endl;
        return false;
    }
}

void MacCameraImpl::ListCameras()
{
    @try {
        [MacCameraProvider listCameras];
    }
    @catch (NSException *exception) {
        std::cerr << "[ERROR] Objective-C exception in ListCameras: " << [exception.reason UTF8String] << std::endl;
    }
}

std::string MacCameraImpl::FindThermalCamera()
{
    @try {
        std::string result = [MacCameraProvider findThermalCamera];
        if (result.empty())
        {
            throw std::runtime_error("No thermal camera found (256x384 resolution)");
        }
        return result;
    }
    @catch (NSException *exception) {
        std::string msg = "Objective-C exception in FindThermalCamera: " + std::string([exception.reason UTF8String]);
        throw std::runtime_error(msg);
    }
}
