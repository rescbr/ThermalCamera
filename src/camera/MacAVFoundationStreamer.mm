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
        _frameSemaphore = dispatch_semaphore_create(1);
        _latestBuffer = nil;
    if (_videoOutput)
    {
        [_videoOutput setSampleBufferDelegate:nil queue:NULL];
    }
    
    _isRunning = false;
    }
    return self;
}

- (void)dealloc
{
    [self stopStreaming];
}

- (bool)initialize:(NSString*)deviceIdentifier
{
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
            (NSString*)kCVPixelBufferPixelFormatTypeKey : @(kCVPixelFormatType_422YpCbCr8)
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

    dispatch_semaphore_wait(_frameSemaphore, DISPATCH_TIME_FOREVER);

    if (_latestBuffer)
    {
        CFRelease(_latestBuffer);
        _latestBuffer = nil;
    }

    dispatch_semaphore_signal(_frameSemaphore);

    fprintf(stderr, "Stopped streaming\n");
}

- (CMSampleBufferRef)getLatestFrame
{
    dispatch_semaphore_wait(_frameSemaphore, DISPATCH_TIME_FOREVER);
    CMSampleBufferRef buffer = _latestBuffer;

    if (buffer)
    {
        CFRetain(buffer);
    }

    dispatch_semaphore_signal(_frameSemaphore);
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
    dispatch_semaphore_wait(_frameSemaphore, DISPATCH_TIME_FOREVER);

    if (_latestBuffer)
    {
        CFRelease(_latestBuffer);
    }

    _latestBuffer = sampleBuffer;
    CFRetain(sampleBuffer);

    dispatch_semaphore_signal(_frameSemaphore);
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
