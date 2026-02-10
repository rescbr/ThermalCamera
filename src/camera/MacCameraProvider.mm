#import "MacCameraProvider.hpp"
#include "../Profile.hpp"

#import "MacIOKitUvcController.hpp"

#import <CoreMedia/CoreMedia.h>
#import <CoreVideo/CoreVideo.h>

#include <iostream>
#include <cstring>

@implementation MacCameraProvider

- (instancetype)init
{
    self = [super init];
    if (self)
    {
        _streamer = [[MacAVFoundationStreamer alloc] init];
        _controller = std::make_unique<MacIOKitUvcController>();
        _controlsAvailable = false;
        _deviceIdentifier = "";
    }
    return self;
}

- (void)dealloc
{
    [self stopStreaming];

    // _controller is a unique_ptr, so it will be deleted automatically
    // when the C++ members are destructed by the runtime.
    
    _streamer = nil;
}

+ (void)listCameras
{
    [MacAVFoundationStreamer listCameras];
}

+ (std::string)findThermalCamera
{
    return [MacAVFoundationStreamer findThermalCamera];
}

- (bool)initialize:(const std::string&)devicePath
{
    _deviceIdentifier = devicePath;

    NSString *nsId = [NSString stringWithUTF8String:devicePath.c_str()];
    if (![_streamer initialize:nsId])
    {
        std::cerr << "[ERROR] " << __FILE__ << ":" << __LINE__ << " - Failed to initialize AVFoundation streamer for device: " << devicePath << std::endl;
        return false;
    }

    uint32_t locationId = 0;
    try
    {
        // Try to parse as number (base 0 handles 0x prefix for hex)
        unsigned long val = std::stoul(devicePath, nullptr, 0);
        locationId = static_cast<uint32_t>(val);
    }
    catch (...)
    {
        // If parsing fails, locationId stays 0
    }

    // Try to initialize controller with whatever ID we have (or 0 to trigger search/logging)
    if (_controller->Initialize(std::to_string(locationId)))
    {
        _controlsAvailable = true;
        std::cerr << "[INFO] UVC controls initialized successfully" << std::endl;
    }
    else
    {
        _controlsAvailable = false;
        std::cerr << "[WARNING] " << __FILE__ << ":" << __LINE__ << " - UVC controls unavailable (IOKit initialization failed)" << std::endl;
        std::cerr << "          Video streaming continues normally. Device: " << devicePath << std::endl;
    }

    return true;
}

- (bool)startStreaming
{
    return [_streamer startStreaming];
}

- (void)stopStreaming
{
    [_streamer stopStreaming];
}

- (void)getFrame:(uint8_t*)buffer size:(size_t)bufferSize bytesWritten:(size_t*)bytesWritten
{
    PROFILE_SCOPE("MacCameraProvider::getFrame");
    std::lock_guard<std::mutex> lock(_frameMutex);

    CMSampleBufferRef sampleBuffer = [_streamer getLatestFrame];
    if (!sampleBuffer)
    {
        [NSException raise:@"NoFrameException" format:@"No sample buffer available"];
    }

    CVImageBufferRef imageBuffer = CMSampleBufferGetImageBuffer(sampleBuffer);
    if (!imageBuffer)
    {
        CFRelease(sampleBuffer);
        [NSException raise:@"BufferException" format:@"Failed to get image buffer from sample buffer"];
    }

    CVReturn lockResult;
    {
        // PROFILE_SCOPE("MacCameraProvider::Lock");
        lockResult = CVPixelBufferLockBaseAddress(imageBuffer, kCVPixelBufferLock_ReadOnly);
    }
    
    if (lockResult != kCVReturnSuccess)
    {
        CFRelease(sampleBuffer);
        [NSException raise:@"BufferException" format:@"Failed to lock pixel buffer: %d", lockResult];
    }

    size_t width = CVPixelBufferGetWidthOfPlane(imageBuffer, 0);
    size_t height = CVPixelBufferGetHeightOfPlane(imageBuffer, 0);
    size_t requiredSize = width * height * 2;

    if (bufferSize < requiredSize)
    {
        CVPixelBufferUnlockBaseAddress(imageBuffer, kCVPixelBufferLock_ReadOnly);
        CFRelease(sampleBuffer);
        [NSException raise:@"BufferException" format:@"Buffer too small: need %zu, got %zu", requiredSize, bufferSize];
    }

    uint8_t* baseAddress = (uint8_t*)CVPixelBufferGetBaseAddressOfPlane(imageBuffer, 0);
    
    {
        PROFILE_SCOPE("MacCameraProvider::Memcpy");
        std::memcpy(buffer, baseAddress, requiredSize);
    }
    *bytesWritten = requiredSize;

    CVPixelBufferUnlockBaseAddress(imageBuffer, kCVPixelBufferLock_ReadOnly);
    CFRelease(sampleBuffer);
}



- (bool)isRunning
{
    return [_streamer isRunning];
}

- (std::string)getCameraName
{
    return [_streamer getCameraName];
}

- (uint16_t)getVendorId
{
    return [_streamer getVendorId];
}

- (uint16_t)getProductId
{
    return [_streamer getProductId];
}

- (bool)setBrightness:(int)value
{
    std::lock_guard<std::mutex> lock(_controlMutex);

    if (!_controlsAvailable)
    {
        std::cerr << "[WARNING] " << __FILE__ << ":" << __LINE__ << " - UVC controls not available (IOKit failed). Device: " << _deviceIdentifier << std::endl;
        return false;
    }

    if (!_controller->SetBrightness(value))
    {
        std::cerr << "[WARNING] " << __FILE__ << ":" << __LINE__ << " - Failed to set brightness to " << value << std::endl;
        return false;
    }

    return true;
}

- (bool)setWhiteBalanceTemperature:(int)value
{
    std::lock_guard<std::mutex> lock(_controlMutex);

    if (!_controlsAvailable)
    {
        std::cerr << "[WARNING] " << __FILE__ << ":" << __LINE__ << " - UVC controls not available (IOKit failed). Device: " << _deviceIdentifier << std::endl;
        return false;
    }

    if (!_controller->SetWhiteBalanceTemperature(value))
    {
        std::cerr << "[WARNING] " << __FILE__ << ":" << __LINE__ << " - Failed to set white balance temperature to " << value << std::endl;
        return false;
    }

    return true;
}

- (bool)setBacklightCompensation:(int)value
{
    std::lock_guard<std::mutex> lock(_controlMutex);

    if (!_controlsAvailable)
    {
        std::cerr << "[WARNING] " << __FILE__ << ":" << __LINE__ << " - UVC controls not available (IOKit failed). Device: " << _deviceIdentifier << std::endl;
        return false;
    }

    if (!_controller->SetBacklightCompensation(value))
    {
        std::cerr << "[WARNING] " << __FILE__ << ":" << __LINE__ << " - Failed to set backlight compensation to " << value << std::endl;
        return false;
    }

    return true;
}

- (bool)setPowerLineFrequency:(int)value
{
    std::lock_guard<std::mutex> lock(_controlMutex);

    if (!_controlsAvailable)
    {
        std::cerr << "[WARNING] " << __FILE__ << ":" << __LINE__ << " - UVC controls not available (IOKit failed). Device: " << _deviceIdentifier << std::endl;
        return false;
    }

    if (!_controller->SetPowerLineFrequency(value))
    {
        std::cerr << "[WARNING] " << __FILE__ << ":" << __LINE__ << " - Failed to set power line frequency to " << value << std::endl;
        return false;
    }

    return true;
}

- (bool)setSaturation:(int)value
{
    std::lock_guard<std::mutex> lock(_controlMutex);

    if (!_controlsAvailable)
    {
        std::cerr << "[WARNING] " << __FILE__ << ":" << __LINE__ << " - UVC controls not available (IOKit failed). Device: " << _deviceIdentifier << std::endl;
        return false;
    }

    if (!_controller->SetSaturation(value))
    {
        std::cerr << "[WARNING] " << __FILE__ << ":" << __LINE__ << " - Failed to set saturation to " << value << std::endl;
        return false;
    }

    return true;
}

- (bool)setSharpness:(int)value
{
    std::lock_guard<std::mutex> lock(_controlMutex);

    if (!_controlsAvailable)
    {
        std::cerr << "[WARNING] " << __FILE__ << ":" << __LINE__ << " - UVC controls not available (IOKit failed). Device: " << _deviceIdentifier << std::endl;
        return false;
    }

    if (!_controller->SetSharpness(value))
    {
        std::cerr << "[WARNING] " << __FILE__ << ":" << __LINE__ << " - Failed to set sharpness to " << value << std::endl;
        return false;
    }

    return true;
}

- (bool)setContrast:(int)value
{
    std::lock_guard<std::mutex> lock(_controlMutex);

    if (!_controlsAvailable)
    {
        std::cerr << "[WARNING] " << __FILE__ << ":" << __LINE__ << " - UVC controls not available (IOKit failed). Device: " << _deviceIdentifier << std::endl;
        return false;
    }

    if (!_controller->SetContrast(value))
    {
        std::cerr << "[WARNING] " << __FILE__ << ":" << __LINE__ << " - Failed to set contrast to " << value << std::endl;
        return false;
    }

    return true;
}

- (bool)setHue:(int)value
{
    std::lock_guard<std::mutex> lock(_controlMutex);

    if (!_controlsAvailable)
    {
        std::cerr << "[WARNING] " << __FILE__ << ":" << __LINE__ << " - UVC controls not available (IOKit failed). Device: " << _deviceIdentifier << std::endl;
        return false;
    }

    if (!_controller->SetHue(value))
    {
        std::cerr << "[WARNING] " << __FILE__ << ":" << __LINE__ << " - Failed to set hue to " << value << std::endl;
        return false;
    }

    return true;
}

- (bool)setGamma:(int)value
{
    std::lock_guard<std::mutex> lock(_controlMutex);

    if (!_controlsAvailable)
    {
        std::cerr << "[WARNING] " << __FILE__ << ":" << __LINE__ << " - UVC controls not available (IOKit failed). Device: " << _deviceIdentifier << std::endl;
        return false;
    }

    if (!_controller->SetGamma(value))
    {
        std::cerr << "[WARNING] " << __FILE__ << ":" << __LINE__ << " - Failed to set gamma to " << value << std::endl;
        return false;
    }

    return true;
}

- (bool)setAutoWhiteBalanceTemperature:(bool)enable
{
    std::lock_guard<std::mutex> lock(_controlMutex);

    if (!_controlsAvailable)
    {
        std::cerr << "[WARNING] " << __FILE__ << ":" << __LINE__ << " - UVC controls not available (IOKit failed). Device: " << _deviceIdentifier << std::endl;
        return false;
    }

    if (!_controller->SetAutoWhiteBalanceTemperature(enable))
    {
        std::cerr << "[WARNING] " << __FILE__ << ":" << __LINE__ << " - Failed to set auto white balance to " << (enable ? "true" : "false") << std::endl;
        return false;
    }

    return true;
}

- (bool)getBrightness:(int*)value
{
    std::lock_guard<std::mutex> lock(_controlMutex);

    if (!_controlsAvailable)
    {
        return false;
    }

    return _controller->GetBrightness(value);
}

- (bool)getWhiteBalanceTemperature:(int*)value
{
    std::lock_guard<std::mutex> lock(_controlMutex);

    if (!_controlsAvailable)
    {
        return false;
    }

    return _controller->GetWhiteBalanceTemperature(value);
}

- (bool)getBacklightCompensation:(int*)value
{
    std::lock_guard<std::mutex> lock(_controlMutex);

    if (!_controlsAvailable)
    {
        return false;
    }

    return _controller->GetBacklightCompensation(value);
}

- (bool)getPowerLineFrequency:(int*)value
{
    std::lock_guard<std::mutex> lock(_controlMutex);

    if (!_controlsAvailable)
    {
        return false;
    }

    return _controller->GetPowerLineFrequency(value);
}

- (bool)getSaturation:(int*)value
{
    std::lock_guard<std::mutex> lock(_controlMutex);

    if (!_controlsAvailable)
    {
        return false;
    }

    return _controller->GetSaturation(value);
}

- (bool)getSharpness:(int*)value
{
    std::lock_guard<std::mutex> lock(_controlMutex);

    if (!_controlsAvailable)
    {
        return false;
    }

    return _controller->GetSharpness(value);
}

- (bool)getContrast:(int*)value
{
    std::lock_guard<std::mutex> lock(_controlMutex);

    if (!_controlsAvailable)
    {
        return false;
    }

    return _controller->GetContrast(value);
}

- (bool)getHue:(int*)value
{
    std::lock_guard<std::mutex> lock(_controlMutex);

    if (!_controlsAvailable)
    {
        return false;
    }

    return _controller->GetHue(value);
}

- (bool)getGamma:(int*)value
{
    std::lock_guard<std::mutex> lock(_controlMutex);

    if (!_controlsAvailable)
    {
        return false;
    }

    return _controller->GetGamma(value);
}

- (bool)getAutoWhiteBalanceTemperature:(int*)value
{
    std::lock_guard<std::mutex> lock(_controlMutex);

    if (!_controlsAvailable)
    {
        return false;
    }

    return _controller->GetAutoWhiteBalanceTemperature(value);
}

@end
