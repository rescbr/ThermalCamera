#import "MacAVFoundationStreamer.hpp"

#import <CoreMedia/CoreMedia.h>
#import <CoreVideo/CoreVideo.h>

#include <iostream>
#include <mutex>

@implementation MacAVFoundationStreamer

+ (void)listCameras
{
    fprintf(stderr, "\nAvailable UVC Cameras (macOS):\n");
    fprintf(stderr, "=============================\n");

    AVCaptureDeviceDiscoverySession* discoverySession = [AVCaptureDeviceDiscoverySession
        discoverySessionWithDeviceTypes:@[AVCaptureDeviceTypeExternal]
        mediaType:AVMediaTypeVideo
        position:AVCaptureDevicePositionUnspecified];

    NSArray<AVCaptureDevice*>* devices = discoverySession.devices;
    int cameraIndex = 0;

    for (AVCaptureDevice* device in devices)
    {
        fprintf(stderr, "\nCamera %d:\n", cameraIndex);
        fprintf(stderr, "  Name: %s\n", [device.localizedName UTF8String]);

        if (device.uniqueID)
        {
            fprintf(stderr, "  Unique ID: %s\n", [device.uniqueID UTF8String]);
        }

        // KVC for vendorID/productID causes crash on some macOS versions/devices
        // Skipping direct VID/PID readout for listing

        for (AVCaptureDeviceFormat* format in device.formats)
        {
            CMVideoDimensions dimensions = CMVideoFormatDescriptionGetDimensions(format.formatDescription);
            fprintf(stderr, "  Format: %dx%d\n", dimensions.width, dimensions.height);
        }

        cameraIndex++;
    }

    fprintf(stderr, "\n=============================\n");
}

- (instancetype)init
{
    self = [super init];
    if (self)
    {
        _captureSession = nil;
        _deviceInput = nil;
        _videoOutput = nil;
        _captureQueue = dispatch_queue_create("com.thermalcamera.capture", DISPATCH_QUEUE_SERIAL);
        // _frameSemaphore replaced by _frameMutex
        // _frameAvailableSemaphore replaced by _frameCV
        _latestBuffer = nil;
        _newFrameReceived = false;
    if (_videoOutput)
    {
        [_videoOutput setSampleBufferDelegate:nil queue:NULL];
    }
    
    _isRunning = false;
    _isDisconnected = false;
    
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(deviceDisconnected:)
                                                 name:AVCaptureDeviceWasDisconnectedNotification
                                               object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(deviceDisconnected:)
                                                 name:AVCaptureSessionRuntimeErrorNotification
                                               object:nil];
    }
    return self;
}

- (void)dealloc
{
    [[NSNotificationCenter defaultCenter] removeObserver:self];
    [self stopStreaming];
}

- (bool)initialize:(NSString*)deviceIdentifier
{
    // Reset state for new connection
    _isDisconnected = false;
    _isRunning = false;
    
    {
        std::lock_guard<std::mutex> lock(_frameMutex);
        _newFrameReceived = false;
        if (_latestBuffer) {
            CFRelease(_latestBuffer);
            _latestBuffer = nil;
        }
    }
    
    AVCaptureDevice* device = nil;

    if ([deviceIdentifier length] == 0)
    {
        fprintf(stderr, "Finding thermal camera...\n");

        AVCaptureDeviceDiscoverySession* discoverySession = [AVCaptureDeviceDiscoverySession
            discoverySessionWithDeviceTypes:@[AVCaptureDeviceTypeExternal]
            mediaType:AVMediaTypeVideo
            position:AVCaptureDevicePositionUnspecified];

        NSArray<AVCaptureDevice*>* devices = discoverySession.devices;

        for (AVCaptureDevice* dev in devices)
        {
            for (AVCaptureDeviceFormat* format in dev.formats)
            {
                CMVideoDimensions dimensions = CMVideoFormatDescriptionGetDimensions(format.formatDescription);
                if (dimensions.width == 256 && dimensions.height == 384)
                {
                    device = dev;
                    break;
                }
            }
            if (device) break;
        }

        if (!device)
        {
            fprintf(stderr, "No thermal camera found (256x384)\n");
            return false;
        }

        fprintf(stderr, "Found thermal camera: %s\n", [device.localizedName UTF8String]);
    }
    else
    {
        device = [AVCaptureDevice deviceWithUniqueID:deviceIdentifier];
        if (!device)
        {
            fprintf(stderr, "Device not found: %s\n", [deviceIdentifier UTF8String]);
            return false;
        }
    }

    NSError* error = nil;

    @try {
        _deviceInput = [[AVCaptureDeviceInput alloc] initWithDevice:device error:&error];
    }
    @catch (NSException *exception) {
        std::cerr << "[ERROR] " << __FILE__ << ":" << __LINE__
                  << " - Exception creating device input: " << [exception.reason UTF8String] << std::endl;
        return false;
    }

    if (error)
    {
        std::cerr << "[ERROR] " << __FILE__ << ":" << __LINE__
                  << " - Failed to create device input: " << [error.localizedDescription UTF8String] << std::endl;
        return false;
    }

    @try {
        _captureSession = [[AVCaptureSession alloc] init];
        [_captureSession beginConfiguration];

        if ([_captureSession canAddInput:_deviceInput])
        {
            [_captureSession addInput:_deviceInput];
        }
        else
        {
            std::cerr << "[ERROR] " << __FILE__ << ":" << __LINE__
                      << " - Cannot add input to session" << std::endl;
            [_captureSession commitConfiguration];
            return false;
        }

        _videoOutput = [[AVCaptureVideoDataOutput alloc] init];
        _videoOutput.videoSettings = @{
            (NSString*)kCVPixelBufferPixelFormatTypeKey : @(kCVPixelFormatType_422YpCbCr8_yuvs)
        };
        _videoOutput.alwaysDiscardsLateVideoFrames = YES;

        [_videoOutput setSampleBufferDelegate:self queue:_captureQueue];

        if ([_captureSession canAddOutput:_videoOutput])
        {
            [_captureSession addOutput:_videoOutput];
        }
        else
        {
            std::cerr << "[ERROR] " << __FILE__ << ":" << __LINE__
                      << " - Cannot add output to session" << std::endl;
            [_captureSession commitConfiguration];
            return false;
        }

        for (AVCaptureConnection* connection in _videoOutput.connections)
        {
            if (connection.isVideoMirroringSupported)
            {
                connection.videoMirrored = NO;
            }
            
            // Try to disable via KVC on the connection (undocumented but reported to work for VCP)
            if (@available(macOS 14.0, *))
            {
                 @try {
                    // Check for "videoEffects" property on connection
                    if ([connection respondsToSelector:@selector(setVideoEffects:)]) {
                         [connection setValue:@[] forKey:@"videoEffects"];
                    }
                 } @catch (NSException *e) {
                     // Ignore KVC errors
                 }
            }
        }

        // Disable macOS 14.0+ Reaction Effects (Gestures) to save CPU
        if (@available(macOS 14.0, *))
        {
            if ([device respondsToSelector:@selector(setReactionEffectGesturesEnabled:)])
            {
                NSError* lockError = nil;
                if ([device lockForConfiguration:&lockError])
                {
                    // Log current state
                    BOOL current = NO;
                    @try { current = [[device valueForKey:@"reactionEffectGesturesEnabled"] boolValue]; } @catch(id e) {}
                    
                    if (current) std::cerr << "[INFO] Reaction Effects enabled before: YES" << std::endl;
                    
                    [device setValue:@NO forKey:@"reactionEffectGesturesEnabled"];
                    
                    // Verify
                    BOOL after = NO;
                    @try { after = [[device valueForKey:@"reactionEffectGesturesEnabled"] boolValue]; } @catch(id e) {}
                    
                    if (current && !after) {
                        std::cerr << "[INFO] Successfully disabled macOS Reaction Effects" << std::endl;
                    } else if (current && after) {
                         std::cerr << "[WARNING] Failed to disable macOS Reaction Effects (value didn't change)" << std::endl;
                    }
                    
                    [device unlockForConfiguration];
                }
                else
                {
                    std::cerr << "[WARNING] Failed to lock device to disable Reaction Effects: "
                              << [lockError.localizedDescription UTF8String] << std::endl;
                }
            }
        }
        
        [_captureSession commitConfiguration];
    }
    @catch (NSException *exception) {
        std::cerr << "[ERROR] " << __FILE__ << ":" << __LINE__
                  << " - Exception configuring session: " << [exception.reason UTF8String] << std::endl;
        return false;
    }

    return true;
}

- (bool)startStreaming
{
    if (_isRunning)
    {
        return true;
    }

    @try {
        [_captureSession startRunning];
        _isRunning = true;
        std::cerr << "[INFO] Started streaming successfully" << std::endl;

        // Re-apply Reaction Effects disable after startRunning
        if (@available(macOS 14.0, *))
        {
            AVCaptureDevice* device = _deviceInput.device;
            if (device && [device respondsToSelector:@selector(setReactionEffectGesturesEnabled:)])
            {
                 NSError* lockError = nil;
                 if ([device lockForConfiguration:&lockError])
                 {
                     BOOL current = [[device valueForKey:@"reactionEffectGesturesEnabled"] boolValue];
                     if (current)
                     {
                         [device setValue:@NO forKey:@"reactionEffectGesturesEnabled"];
                         std::cerr << "[INFO] Re-disabled macOS Reaction Effects after start" << std::endl;
                     }
                     [device unlockForConfiguration];
                 }
            }
        }
    }
    @catch (NSException *exception) {
        std::cerr << "[ERROR] " << __FILE__ << ":" << __LINE__
                  << " - Exception starting session: " << [exception.reason UTF8String] << std::endl;
        return false;
    }

    return true;
}

- (void)stopStreaming
{
    if (!_isRunning)
    {
        return;
    }

    _isRunning = false;

    @try {
        if ([_captureSession isRunning])
        {
            [_captureSession stopRunning];
        }
    }
    @catch (NSException *exception) {
        std::cerr << "[ERROR] " << __FILE__ << ":" << __LINE__
                  << " - Exception stopping session: " << [exception.reason UTF8String] << std::endl;
    }

    {
        std::lock_guard<std::mutex> lock(_frameMutex);
        if (_latestBuffer)
        {
            CFRelease(_latestBuffer);
            _latestBuffer = nil;
        }
    }

    // Wake up any waiters
    _frameCV.notify_all();

    fprintf(stderr, "Stopped streaming\n");
}

- (CMSampleBufferRef)getLatestFrame
{
    std::lock_guard<std::mutex> lock(_frameMutex);
    CMSampleBufferRef buffer = _latestBuffer;

    if (buffer)
    {
        CFRetain(buffer);
    }

    return buffer;
}

- (bool)isRunning
{
    return _isRunning;
}

- (std::string)getCameraName
{
    if (_deviceInput && _deviceInput.device)
    {
        return std::string([_deviceInput.device.localizedName UTF8String]);
    }
    return "Unknown";
}

- (uint16_t)getVendorId
{
    // VID/PID not easily accessible via public AVFoundation API without KVC risk
    return 0;
}

- (uint16_t)getProductId
{
    // VID/PID not easily accessible via public AVFoundation API without KVC risk
    return 0;
}

- (void)captureOutput:(AVCaptureOutput*)output
    didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer
    fromConnection:(AVCaptureConnection*)connection
{
    std::lock_guard<std::mutex> lock(_frameMutex);

    if (_latestBuffer)
    {
        CFRelease(_latestBuffer);
    }

    _latestBuffer = sampleBuffer;
    CFRetain(sampleBuffer);

    _newFrameReceived = true;
    _frameCV.notify_one();
}

- (void)deviceDisconnected:(NSNotification *)notification
{
    if ([notification.name isEqualToString:AVCaptureDeviceWasDisconnectedNotification]) {
        AVCaptureDevice *device = (AVCaptureDevice*)notification.object;
        if (device == _deviceInput.device) {
             std::cerr << "[WARNING] Camera disconnected!" << std::endl;
             {
                 std::lock_guard<std::mutex> lock(_frameMutex);
                 _isDisconnected = true;
                 _isRunning = false;
             }
             _frameCV.notify_all();
        }
    } else if ([notification.name isEqualToString:AVCaptureSessionRuntimeErrorNotification]) {
        AVCaptureSession *session = (AVCaptureSession*)notification.object;
        if (session == _captureSession) {
            NSError *error = notification.userInfo[AVCaptureSessionErrorKey];
            const char* errStr = error ? [error.localizedDescription UTF8String] : "Unknown error";
            std::cerr << "[ERROR] Capture session runtime error: " << errStr << std::endl;
            {
                std::lock_guard<std::mutex> lock(_frameMutex);
                _isDisconnected = true;
                _isRunning = false;
            }
            _frameCV.notify_all();
        }
    }
}

- (CMSampleBufferRef)waitForNewFrame:(double)timeoutSeconds
{
    std::unique_lock<std::mutex> lock(_frameMutex);

    if (_isDisconnected) return nil;

    // Wait for new frame
    if (_frameCV.wait_for(lock, std::chrono::duration<double>(timeoutSeconds), [self]{ return _newFrameReceived || !_isRunning || _isDisconnected; }))
    {
        if (_isDisconnected || !_isRunning) return nil;
        _newFrameReceived = false;
        CMSampleBufferRef buffer = _latestBuffer;
        if (buffer) CFRetain(buffer);
        return buffer;
    }
    
    return nil;
}

- (bool)isDisconnected
{
    return _isDisconnected;
}

+ (std::string)findThermalCamera
{
    AVCaptureDeviceDiscoverySession* discoverySession = [AVCaptureDeviceDiscoverySession
        discoverySessionWithDeviceTypes:@[AVCaptureDeviceTypeExternal]
        mediaType:AVMediaTypeVideo
        position:AVCaptureDevicePositionUnspecified];

    NSArray<AVCaptureDevice*>* devices = discoverySession.devices;

    for (AVCaptureDevice* device in devices)
    {
        for (AVCaptureDeviceFormat* format in device.formats)
        {
            CMVideoDimensions dimensions = CMVideoFormatDescriptionGetDimensions(format.formatDescription);
            if (dimensions.width == 256 && dimensions.height == 384)
            {
                if (device.uniqueID)
                {
                    return std::string([device.uniqueID UTF8String]);
                }
            }
        }
    }

    return "";
}

@end
