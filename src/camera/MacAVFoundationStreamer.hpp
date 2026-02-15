#ifndef MAC_AVFOUNDATION_STREAMER_HPP
#define MAC_AVFOUNDATION_STREAMER_HPP

#import <Foundation/Foundation.h>
#import <AVFoundation/AVFoundation.h>
#import <CoreMediaIO/CMIOHardware.h>

#include <string>
#include <mutex>
#include <condition_variable>

@interface MacAVFoundationStreamer : NSObject <AVCaptureVideoDataOutputSampleBufferDelegate>
{
    AVCaptureSession* _captureSession;
    AVCaptureDeviceInput* _deviceInput;
    AVCaptureVideoDataOutput* _videoOutput;
    dispatch_queue_t _captureQueue;
    std::mutex _frameMutex;
    std::condition_variable _frameCV;
    bool _newFrameReceived;
    CMSampleBufferRef _latestBuffer;
    bool _isRunning;
    bool _isDisconnected;
}

+ (void)listCameras;
+ (std::string)findThermalCamera;

- (bool)initialize:(NSString*)deviceIdentifier;
- (bool)startStreaming;
- (void)stopStreaming;
- (CMSampleBufferRef)getLatestFrame;
- (CMSampleBufferRef)waitForNewFrame:(double)timeoutSeconds;
- (bool)isRunning;
- (bool)isDisconnected;
- (std::string)getCameraName;
- (uint16_t)getVendorId;
- (uint16_t)getProductId;

- (void)dealloc;

@end

#endif
