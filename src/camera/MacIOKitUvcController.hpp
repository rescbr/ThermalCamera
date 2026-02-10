#ifndef MAC_IOKIT_UVC_CONTROLLER_HPP
#define MAC_IOKIT_UVC_CONTROLLER_HPP

#include <string>
#include <mutex>
#include <cstdint>

#ifdef __APPLE__
    #include <IOKit/IOKitLib.h>
    #include <IOKit/usb/IOUSBLib.h>
    #include <mach/mach_error.h>
#endif

class MacIOKitUvcController
{
public:
    MacIOKitUvcController();
    ~MacIOKitUvcController();

    bool Initialize(const std::string& deviceIdentifier);
    bool IsInitialized() const;

    bool SetBrightness(int value);
    bool SetWhiteBalanceTemperature(int value);
    bool SetBacklightCompensation(int value);
    bool SetPowerLineFrequency(int value);
    bool SetSaturation(int value);
    bool SetSharpness(int value);
    bool SetContrast(int value);
    bool SetHue(int value);
    bool SetGamma(int value);
    bool SetAutoWhiteBalanceTemperature(bool enable);

    bool GetBrightness(int* value);
    bool GetWhiteBalanceTemperature(int* value);
    bool GetBacklightCompensation(int* value);
    bool GetPowerLineFrequency(int* value);
    bool GetSaturation(int* value);
    bool GetSharpness(int* value);
    bool GetContrast(int* value);
    bool GetHue(int* value);
    bool GetGamma(int* value);
    bool GetAutoWhiteBalanceTemperature(int* value);

private:
    bool SendControlRequest(int selector, int unitId, void* data, int length);
    bool SendControlRequestGet(int type, int selector, int unitId, void* data, int length);
    bool FindUsbDeviceAndInterfaces(uint32_t locationId);

    io_service_t _usbDevice;
    IOUSBDeviceInterface320** _deviceInterface;
    IOUSBInterfaceInterface220** _controllerInterface;
    uint8_t _videoInterfaceIndex;
    uint8_t _processingUnitId;
    std::mutex _controlMutex;
    bool _initialized;
};

#endif
