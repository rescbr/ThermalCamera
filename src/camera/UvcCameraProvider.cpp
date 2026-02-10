#include "UvcCameraProvider.hpp"

#include <iostream>
#include <cstring>
#include <stdexcept>

static const int FRAME_WIDTH = 256;
static const int FRAME_HEIGHT = 384;
static const int FRAME_FPS = 25;

UvcCameraProvider::UvcCameraProvider()
    : _context(nullptr)
    , _device(nullptr)
    , _deviceHandle(nullptr)
    , _streamHandle(nullptr)
    , _cameraName("")
    , _vendorId(0)
    , _productId(0)
    , _frameBuffer(nullptr)
    , _frameSize(0)
    , _isRunning(false)
{
}

UvcCameraProvider::~UvcCameraProvider()
{
    StopStreaming();

    if (_deviceHandle)
    {
        uvc_close(_deviceHandle);
        _deviceHandle = nullptr;
    }

    if (_device)
    {
        uvc_unref_device(_device);
        _device = nullptr;
    }

    if (_context)
    {
        uvc_exit(_context);
        _context = nullptr;
    }
}

void UvcCameraProvider::Initialize(const std::string& devicePath)
{
    uvc_error_t res = uvc_init(&_context, nullptr);
    if (res < 0)
    {
        std::string msg = "Failed to initialize UVC context: " + std::string(uvc_strerror(res));
        throw std::runtime_error(msg);
    }

    res = uvc_find_device(_context, &_device, 0, 0, devicePath.c_str());
    if (res < 0)
    {
        std::string msg = "Failed to find UVC device " + devicePath + ": " + uvc_strerror(res);
        throw std::runtime_error(msg);
    }

    res = uvc_open(_device, &_deviceHandle);
    if (res < 0)
    {
        std::string msg = "Failed to open UVC device: " + std::string(uvc_strerror(res));
        throw std::runtime_error(msg);
    }

    uvc_device_descriptor_t* desc;
    res = uvc_get_device_descriptor(_device, &desc);
    if (res == 0)
    {
        _cameraName = desc->product ? desc->product : "Unknown";
        _vendorId = desc->idVendor;
        _productId = desc->idProduct;
        uvc_free_device_descriptor(desc);
    }

    uvc_print_diag(_deviceHandle, stderr);

    res = uvc_get_stream_ctrl_format_size(
        _deviceHandle,
        &_streamCtrl,
        UVC_FRAME_FORMAT_YUYV,
        FRAME_WIDTH,
        FRAME_HEIGHT,
        FRAME_FPS
    );

    if (res < 0)
    {
        std::string msg = "Failed to negotiate stream format: " + std::string(uvc_strerror(res));
        throw std::runtime_error(msg);
    }
}

void UvcCameraProvider::StartStreaming()
{
    if (_isRunning)
    {
        return;
    }

    uvc_error_t res = uvc_start_streaming(
        _deviceHandle,
        &_streamCtrl,
        [](uvc_frame_t* frame, void* userPtr) {
            UvcCameraProvider* camera = static_cast<UvcCameraProvider*>(userPtr);

            std::lock_guard<std::mutex> lock(camera->_frameMutex);

            if (!camera->_frameBuffer || camera->_frameSize != frame->data_bytes)
            {
                camera->_frameBuffer.reset(new uint8_t[frame->data_bytes]);
                camera->_frameSize = frame->data_bytes;
            }

            std::memcpy(camera->_frameBuffer.get(), frame->data, frame->data_bytes);
        },
        this,
        0
    );

    if (res < 0)
    {
        std::string msg = "Failed to start streaming: " + std::string(uvc_strerror(res));
        throw std::runtime_error(msg);
    }

    _isRunning = true;
    std::cerr << "Started streaming successfully\n";
}

void UvcCameraProvider::StopStreaming()
{
    if (!_isRunning)
    {
        return;
    }

    _isRunning = false;

    if (_streamHandle)
    {
        uvc_stop_streaming(_deviceHandle);
        _streamHandle = nullptr;
    }

    std::cerr << "Stopped streaming\n";
}

void UvcCameraProvider::GetFrame(uint8_t* buffer, size_t bufferSize, size_t& bytesWritten)
{
    std::lock_guard<std::mutex> lock(_frameMutex);

    if (!_frameBuffer || _frameSize == 0)
    {
        throw std::runtime_error("No frame buffer available");
    }

    if (bufferSize < _frameSize)
    {
        throw std::runtime_error(
            "Buffer too small: need " + std::to_string(_frameSize) +
            ", got " + std::to_string(bufferSize));
    }

    std::memcpy(buffer, _frameBuffer.get(), _frameSize);
    bytesWritten = _frameSize;
}


bool UvcCameraProvider::IsRunning() const
{
    return _isRunning;
}

std::string UvcCameraProvider::GetCameraName() const
{
    return _cameraName;
}

uint16_t UvcCameraProvider::GetVendorId() const
{
    return _vendorId;
}

uint16_t UvcCameraProvider::GetProductId() const
{
    return _productId;
}

bool UvcCameraProvider::SetBrightness(int value)
{
    std::lock_guard<std::mutex> lock(_controlMutex);

    if (!_deviceHandle)
    {
        std::cerr << "Warning: Set brightness failed\n";
        std::cerr << "         Camera not initialized\n";
        return false;
    }

    int result = uvc_set_brightness(_deviceHandle, value);
    if (result < 0)
    {
        std::cerr << "Warning: Set brightness failed\n";
        std::cerr << "         libuvc error: " << uvc_strerror(result) << "\n";
        return false;
    }

    int readBack = 0;
    result = uvc_get_brightness(_deviceHandle, &readBack);
    if (result < 0)
    {
        std::cerr << "Warning: Set brightness to " << value
                  << " succeeded, but read-back failed\n";
        std::cerr << "         libuvc error: " << uvc_strerror(result) << "\n";
        return false;
    }

    if (readBack != value)
    {
        std::cerr << "Warning: Set brightness to " << value
                  << " but device reports " << readBack << "\n";
        return false;
    }

    std::cerr << "Set brightness to " << value << " (confirmed: " << readBack << ")\n";
    return true;
}

bool UvcCameraProvider::SetWhiteBalanceTemperature(int value)
{
    std::lock_guard<std::mutex> lock(_controlMutex);

    if (!_deviceHandle)
    {
        std::cerr << "Warning: Set white balance temperature failed\n";
        std::cerr << "         Camera not initialized\n";
        return false;
    }

    int result = uvc_set_white_balance_temperature(_deviceHandle, value);
    if (result < 0)
    {
        std::cerr << "Warning: Set white balance temperature failed\n";
        std::cerr << "         libuvc error: " << uvc_strerror(result) << "\n";
        return false;
    }

    int readBack = 0;
    result = uvc_get_white_balance_temperature(_deviceHandle, &readBack);
    if (result < 0)
    {
        std::cerr << "Warning: Set white balance temperature to " << value
                  << " succeeded, but read-back failed\n";
        std::cerr << "         libuvc error: " << uvc_strerror(result) << "\n";
        return false;
    }

    if (readBack != value)
    {
        std::cerr << "Warning: Set white balance temperature to " << value
                  << " but device reports " << readBack << "\n";
        return false;
    }

    std::cerr << "Set white balance temperature to " << value << " (confirmed: " << readBack << ")\n";
    return true;
}

bool UvcCameraProvider::SetBacklightCompensation(int value)
{
    std::lock_guard<std::mutex> lock(_controlMutex);

    if (!_deviceHandle)
    {
        std::cerr << "Warning: Set backlight compensation failed\n";
        std::cerr << "         Camera not initialized\n";
        return false;
    }

    int result = uvc_set_backlight_compensation(_deviceHandle, value);
    if (result < 0)
    {
        std::cerr << "Warning: Set backlight compensation failed\n";
        std::cerr << "         libuvc error: " << uvc_strerror(result) << "\n";
        return false;
    }

    int readBack = 0;
    result = uvc_get_backlight_compensation(_deviceHandle, &readBack);
    if (result < 0)
    {
        std::cerr << "Warning: Set backlight compensation to " << value
                  << " succeeded, but read-back failed\n";
        std::cerr << "         libuvc error: " << uvc_strerror(result) << "\n";
        return false;
    }

    if (readBack != value)
    {
        std::cerr << "Warning: Set backlight compensation to " << value
                  << " but device reports " << readBack << "\n";
        return false;
    }

    std::cerr << "Set backlight compensation to " << value << " (confirmed: " << readBack << ")\n";
    return true;
}

bool UvcCameraProvider::SetPowerLineFrequency(int value)
{
    std::lock_guard<std::mutex> lock(_controlMutex);

    if (!_deviceHandle)
    {
        std::cerr << "Warning: Set power line frequency failed\n";
        std::cerr << "         Camera not initialized\n";
        return false;
    }

    int result = uvc_set_power_line_frequency(_deviceHandle, value);
    if (result < 0)
    {
        std::cerr << "Warning: Set power line frequency failed\n";
        std::cerr << "         libuvc error: " << uvc_strerror(result) << "\n";
        return false;
    }

    int readBack = 0;
    result = uvc_get_power_line_frequency(_deviceHandle, &readBack);
    if (result < 0)
    {
        std::cerr << "Warning: Set power line frequency to " << value
                  << " succeeded, but read-back failed\n";
        std::cerr << "         libuvc error: " << uvc_strerror(result) << "\n";
        return false;
    }

    if (readBack != value)
    {
        std::cerr << "Warning: Set power line frequency to " << value
                  << " but device reports " << readBack << "\n";
        return false;
    }

    std::cerr << "Set power line frequency to " << value << " (confirmed: " << readBack << ")\n";
    return true;
}

bool UvcCameraProvider::SetSaturation(int value)
{
    std::lock_guard<std::mutex> lock(_controlMutex);

    if (!_deviceHandle)
    {
        std::cerr << "Warning: Set saturation failed\n";
        std::cerr << "         Camera not initialized\n";
        return false;
    }

    int result = uvc_set_saturation(_deviceHandle, value);
    if (result < 0)
    {
        std::cerr << "Warning: Set saturation failed\n";
        std::cerr << "         libuvc error: " << uvc_strerror(result) << "\n";
        return false;
    }

    int readBack = 0;
    result = uvc_get_saturation(_deviceHandle, &readBack);
    if (result < 0)
    {
        std::cerr << "Warning: Set saturation to " << value
                  << " succeeded, but read-back failed\n";
        std::cerr << "         libuvc error: " << uvc_strerror(result) << "\n";
        return false;
    }

    if (readBack != value)
    {
        std::cerr << "Warning: Set saturation to " << value
                  << " but device reports " << readBack << "\n";
        return false;
    }

    std::cerr << "Set saturation to " << value << " (confirmed: " << readBack << ")\n";
    return true;
}

bool UvcCameraProvider::SetSharpness(int value)
{
    std::lock_guard<std::mutex> lock(_controlMutex);

    if (!_deviceHandle)
    {
        std::cerr << "Warning: Set sharpness failed\n";
        std::cerr << "         Camera not initialized\n";
        return false;
    }

    int result = uvc_set_sharpness(_deviceHandle, value);
    if (result < 0)
    {
        std::cerr << "Warning: Set sharpness failed\n";
        std::cerr << "         libuvc error: " << uvc_strerror(result) << "\n";
        return false;
    }

    int readBack = 0;
    result = uvc_get_sharpness(_deviceHandle, &readBack);
    if (result < 0)
    {
        std::cerr << "Warning: Set sharpness to " << value
                  << " succeeded, but read-back failed\n";
        std::cerr << "         libuvc error: " << uvc_strerror(result) << "\n";
        return false;
    }

    if (readBack != value)
    {
        std::cerr << "Warning: Set sharpness to " << value
                  << " but device reports " << readBack << "\n";
        return false;
    }

    std::cerr << "Set sharpness to " << value << " (confirmed: " << readBack << ")\n";
    return true;
}

bool UvcCameraProvider::SetContrast(int value)
{
    std::lock_guard<std::mutex> lock(_controlMutex);

    if (!_deviceHandle)
    {
        std::cerr << "Warning: Set contrast failed\n";
        std::cerr << "         Camera not initialized\n";
        return false;
    }

    int result = uvc_set_contrast(_deviceHandle, value);
    if (result < 0)
    {
        std::cerr << "Warning: Set contrast failed\n";
        std::cerr << "         libuvc error: " << uvc_strerror(result) << "\n";
        return false;
    }

    int readBack = 0;
    result = uvc_get_contrast(_deviceHandle, &readBack);
    if (result < 0)
    {
        std::cerr << "Warning: Set contrast to " << value
                  << " succeeded, but read-back failed\n";
        std::cerr << "         libuvc error: " << uvc_strerror(result) << "\n";
        return false;
    }

    if (readBack != value)
    {
        std::cerr << "Warning: Set contrast to " << value
                  << " but device reports " << readBack << "\n";
        return false;
    }

    std::cerr << "Set contrast to " << value << " (confirmed: " << readBack << ")\n";
    return true;
}

bool UvcCameraProvider::SetHue(int value)
{
    std::lock_guard<std::mutex> lock(_controlMutex);

    if (!_deviceHandle)
    {
        std::cerr << "Warning: Set hue failed\n";
        std::cerr << "         Camera not initialized\n";
        return false;
    }

    int result = uvc_set_hue(_deviceHandle, value);
    if (result < 0)
    {
        std::cerr << "Warning: Set hue failed\n";
        std::cerr << "         libuvc error: " << uvc_strerror(result) << "\n";
        return false;
    }

    int readBack = 0;
    result = uvc_get_hue(_deviceHandle, &readBack);
    if (result < 0)
    {
        std::cerr << "Warning: Set hue to " << value
                  << " succeeded, but read-back failed\n";
        std::cerr << "         libuvc error: " << uvc_strerror(result) << "\n";
        return false;
    }

    if (readBack != value)
    {
        std::cerr << "Warning: Set hue to " << value
                  << " but device reports " << readBack << "\n";
        return false;
    }

    std::cerr << "Set hue to " << value << " (confirmed: " << readBack << ")\n";
    return true;
}

bool UvcCameraProvider::SetGamma(int value)
{
    std::lock_guard<std::mutex> lock(_controlMutex);

    if (!_deviceHandle)
    {
        std::cerr << "Warning: Set gamma failed\n";
        std::cerr << "         Camera not initialized\n";
        return false;
    }

    int result = uvc_set_gamma(_deviceHandle, value);
    if (result < 0)
    {
        std::cerr << "Warning: Set gamma failed\n";
        std::cerr << "         libuvc error: " << uvc_strerror(result) << "\n";
        return false;
    }

    int readBack = 0;
    result = uvc_get_gamma(_deviceHandle, &readBack);
    if (result < 0)
    {
        std::cerr << "Warning: Set gamma to " << value
                  << " succeeded, but read-back failed\n";
        std::cerr << "         libuvc error: " << uvc_strerror(result) << "\n";
        return false;
    }

    if (readBack != value)
    {
        std::cerr << "Warning: Set gamma to " << value
                  << " but device reports " << readBack << "\n";
        return false;
    }

    std::cerr << "Set gamma to " << value << " (confirmed: " << readBack << ")\n";
    return true;
}

bool UvcCameraProvider::SetAutoWhiteBalanceTemperature(bool enable)
{
    std::lock_guard<std::mutex> lock(_controlMutex);

    if (!_deviceHandle)
    {
        std::cerr << "Warning: Set auto white balance temperature failed\n";
        std::cerr << "         Camera not initialized\n";
        return false;
    }

    int result = uvc_set_ae_mode(_deviceHandle, enable ? 8 : 1);
    if (result < 0)
    {
        std::cerr << "Warning: Set auto white balance temperature failed\n";
        std::cerr << "         libuvc error: " << uvc_strerror(result) << "\n";
        return false;
    }

    int readBack = 0;
    result = uvc_get_ae_mode(_deviceHandle, &readBack);
    if (result < 0)
    {
        std::cerr << "Warning: Set auto white balance temperature to " << (enable ? "true" : "false")
                  << " succeeded, but read-back failed\n";
        std::cerr << "         libuvc error: " << uvc_strerror(result) << "\n";
        return false;
    }

    bool readBackEnabled = (readBack == 8);
    if (readBackEnabled != enable)
    {
        std::cerr << "Warning: Set auto white balance temperature to " << (enable ? "true" : "false")
                  << " but device reports " << (readBackEnabled ? "true" : "false") << "\n";
        return false;
    }

    std::cerr << "Set auto white balance temperature to " << (enable ? "true" : "false")
              << " (confirmed: " << (readBackEnabled ? "true" : "false") << ")\n";
    return true;
}

int UvcCameraProvider::GetBrightness(int* value)
{
    std::lock_guard<std::mutex> lock(_controlMutex);

    if (!_deviceHandle)
    {
        return -1;
    }

    return uvc_get_brightness(_deviceHandle, value);
}

int UvcCameraProvider::GetWhiteBalanceTemperature(int* value)
{
    std::lock_guard<std::mutex> lock(_controlMutex);

    if (!_deviceHandle)
    {
        return -1;
    }

    return uvc_get_white_balance_temperature(_deviceHandle, value);
}

int UvcCameraProvider::GetBacklightCompensation(int* value)
{
    std::lock_guard<std::mutex> lock(_controlMutex);

    if (!_deviceHandle)
    {
        return -1;
    }

    return uvc_get_backlight_compensation(_deviceHandle, value);
}

int UvcCameraProvider::GetPowerLineFrequency(int* value)
{
    std::lock_guard<std::mutex> lock(_controlMutex);

    if (!_deviceHandle)
    {
        return -1;
    }

    return uvc_get_power_line_frequency(_deviceHandle, value);
}

int UvcCameraProvider::GetSaturation(int* value)
{
    std::lock_guard<std::mutex> lock(_controlMutex);

    if (!_deviceHandle)
    {
        return -1;
    }

    return uvc_get_saturation(_deviceHandle, value);
}

int UvcCameraProvider::GetSharpness(int* value)
{
    std::lock_guard<std::mutex> lock(_controlMutex);

    if (!_deviceHandle)
    {
        return -1;
    }

    return uvc_get_sharpness(_deviceHandle, value);
}

int UvcCameraProvider::GetContrast(int* value)
{
    std::lock_guard<std::mutex> lock(_controlMutex);

    if (!_deviceHandle)
    {
        return -1;
    }

    return uvc_get_contrast(_deviceHandle, value);
}

int UvcCameraProvider::GetHue(int* value)
{
    std::lock_guard<std::mutex> lock(_controlMutex);

    if (!_deviceHandle)
    {
        return -1;
    }

    return uvc_get_hue(_deviceHandle, value);
}

int UvcCameraProvider::GetGamma(int* value)
{
    std::lock_guard<std::mutex> lock(_controlMutex);

    if (!_deviceHandle)
    {
        return -1;
    }

    return uvc_get_gamma(_deviceHandle, value);
}

int UvcCameraProvider::GetAutoWhiteBalanceTemperature(int* value)
{
    std::lock_guard<std::mutex> lock(_controlMutex);

    if (!_deviceHandle)
    {
        return -1;
    }

    int mode;
    int result = uvc_get_ae_mode(_deviceHandle, &mode);
    if (result == 0)
    {
        *value = (mode == 8) ? 1 : 0;
    }

    return result;
}

void UvcCameraProvider::ListCameras()
{
    uvc_context_t* context;
    uvc_error_t res = uvc_init(&context, nullptr);

    if (res < 0)
    {
        std::cerr << "Failed to initialize UVC context: " << uvc_strerror(res) << "\n";
        return;
    }

    uvc_device_t** devices;
    uvc_device_descriptor_t* desc;

    res = uvc_get_device_list(context, &devices);
    if (res < 0)
    {
        std::cerr << "Failed to get device list: " << uvc_strerror(res) << "\n";
        uvc_exit(context);
        return;
    }

    std::cerr << "\nAvailable UVC Cameras:\n";
    std::cerr << "=====================\n";

    for (int i = 0; devices[i] != nullptr; ++i)
    {
        uvc_device_t* device = devices[i];

        res = uvc_get_device_descriptor(device, &desc);
        if (res < 0)
        {
            continue;
        }

        std::cerr << "\nCamera " << i << ":\n";
        std::cerr << "  Vendor ID: 0x" << std::hex << desc->idVendor << std::dec;
        std::cerr << " (" << (desc->manufacturer ? desc->manufacturer : "Unknown") << ")\n";
        std::cerr << "  Product ID: 0x" << std::hex << desc->idProduct << std::dec;
        std::cerr << " (" << (desc->product ? desc->product : "Unknown") << ")\n";
        std::cerr << "  Serial: " << (desc->serialNumber ? desc->serialNumber : "N/A") << "\n";

        uvc_free_device_descriptor(desc);

        uvc_device_handle_t* handle;
        res = uvc_open(device, &handle);
        if (res == 0)
        {
            int formatCount = 0;

            std::cerr << "  Supported Formats:\n";

            const uvc_format_desc_t* formatDesc = uvc_get_format_descs(handle);
            if (formatDesc == nullptr)
            {
                std::cerr << "    No format descriptors available\n";
            }

            while (formatDesc != nullptr)
            {
                std::cerr << "    - Format " << static_cast<int>(formatDesc->bDescriptorSubtype)
                          << " (" << formatDesc->bFormatIndex << ")\n";

                const uvc_frame_desc_t* frameDesc = formatDesc->frame_descs;
                if (frameDesc == nullptr)
                {
                    std::cerr << "      No frame descriptors\n";
                }

                while (frameDesc != nullptr)
                {
                    std::cerr << "      " << frameDesc->wWidth << "x" << frameDesc->wHeight
                              << " @ " << frameDesc->dwDefaultFrameInterval << " FPS\n";
                    frameDesc = frameDesc->next;
                    formatCount++;
                }

                formatDesc = formatDesc->next;
            }

            if (formatCount == 0)
            {
                std::cerr << "    No frame formats found\n";
            }

            uvc_close(handle);
        }
        else
        {
            std::cerr << "  Failed to open device: " << uvc_strerror(res) << "\n";
        }
    }

    uvc_free_device_list(devices, 1);
    uvc_exit(context);
    std::cerr << "\n=====================\n";
}

std::string UvcCameraProvider::FindThermalCamera()
{
    uvc_context_t* context;
    uvc_error_t res = uvc_init(&context, nullptr);

    if (res < 0)
    {
        return "";
    }

    uvc_device_t** devices;
    res = uvc_get_device_list(context, &devices);
    if (res < 0)
    {
        uvc_exit(context);
        return "";
    }

    std::string bestDevicePath;

    for (int i = 0; devices[i] != nullptr; ++i)
    {
        uvc_device_t* device = devices[i];

        uvc_device_handle_t* handle;
        res = uvc_open(device, &handle);
        if (res != 0)
        {
            continue;
        }

        const uvc_format_desc_t* formatDesc = uvc_get_format_descs(handle);
        while (formatDesc != nullptr)
        {
            const uvc_frame_desc_t* frameDesc = formatDesc->frame_descs;
            while (frameDesc != nullptr)
            {
                if (frameDesc->wWidth == 256 && frameDesc->wHeight == 384)
                {
                    uvc_device_handle_t* devHandle = nullptr;
                    uvc_open(device, &devHandle);

                    uint8_t bus[256];
                    if (uvc_get_bus_number(device) >= 0)
                    {
                        snprintf(reinterpret_cast<char*>(bus), sizeof(bus), "%u", uvc_get_bus_number(device));
                    }

                    uint8_t address[256];
                    if (uvc_get_device_address(device) >= 0)
                    {
                        snprintf(reinterpret_cast<char*>(address), sizeof(address), "%u", uvc_get_device_address(device));
                    }

                    if (devHandle)
                    {
                        uvc_close(devHandle);
                    }

                    bestDevicePath = std::string(reinterpret_cast<char*>(bus)) + ":" + std::string(reinterpret_cast<char*>(address));

                    uvc_close(handle);
                    uvc_free_device_list(devices, 1);
                    uvc_exit(context);
                    return bestDevicePath;
                }

                frameDesc = frameDesc->next;
            }

            formatDesc = formatDesc->next;
        }

        uvc_close(handle);
    }

    uvc_free_device_list(devices, 1);
    uvc_exit(context);

    return bestDevicePath;
}
