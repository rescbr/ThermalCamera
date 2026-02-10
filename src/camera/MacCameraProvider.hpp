#ifndef MAC_CAMERA_PROVIDER_HPP
#define MAC_CAMERA_PROVIDER_HPP

#import <Foundation/Foundation.h>

#include <string>
#include <cstdint>
#include <mutex>

#import "MacAVFoundationStreamer.hpp"

class MacIOKitUvcController;

@interface MacCameraProvider : NSObject
{
    MacAVFoundationStreamer* _streamer;
    std::unique_ptr<MacIOKitUvcController> _controller;
    bool _controlsAvailable;
    std::string _deviceIdentifier;
    std::mutex _frameMutex;
    std::mutex _controlMutex;
}

- (bool)initialize:(const std::string&)devicePath;
- (bool)startStreaming;
- (void)stopStreaming;
- (void)getFrame:(uint8_t*)buffer size:(size_t)bufferSize bytesWritten:(size_t*)bytesWritten;
- (bool)isRunning;
- (std::string)getCameraName;
- (uint16_t)getVendorId;
- (uint16_t)getProductId;

+ (void)listCameras;
+ (std::string)findThermalCamera;

- (bool)setBrightness:(int)value;
- (bool)setWhiteBalanceTemperature:(int)value;
- (bool)setBacklightCompensation:(int)value;
- (bool)setPowerLineFrequency:(int)value;
- (bool)setSaturation:(int)value;
- (bool)setSharpness:(int)value;
- (bool)setContrast:(int)value;
- (bool)setHue:(int)value;
- (bool)setGamma:(int)value;
- (bool)setAutoWhiteBalanceTemperature:(bool)enable;

- (bool)getBrightness:(int*)value;
- (bool)getWhiteBalanceTemperature:(int*)value;
- (bool)getBacklightCompensation:(int*)value;
- (bool)getPowerLineFrequency:(int*)value;
- (bool)getSaturation:(int*)value;
- (bool)getSharpness:(int*)value;
- (bool)getContrast:(int*)value;
- (bool)getHue:(int*)value;
- (bool)getGamma:(int*)value;
- (bool)getAutoWhiteBalanceTemperature:(int*)value;

- (void)dealloc;

@end

#endif
