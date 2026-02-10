#include "MacIOKitUvcController.hpp"

#include <iostream>
#include <cstring>
#include <IOKit/IOCFPlugIn.h>

enum {
    UVC_SET_CUR = 0x01,
    UVC_GET_CUR = 0x81,
    UVC_GET_MIN = 0x82,
    UVC_GET_MAX = 0x83,
    UVC_GET_DEF = 0x87
};

enum {
    PU_BACKLIGHT_COMPENSATION = 0x01,
    PU_BRIGHTNESS = 0x02,
    PU_CONTRAST = 0x03,
    UVC_HUE = 0x06,
    UVC_SATURATION = 0x07,
    UVC_SHARPNESS = 0x08,
    UVC_GAMMA = 0x09,
    PU_WHITE_BALANCE_TEMPERATURE = 0x0A,
    PU_AUTO_WHITE_BALANCE_TEMPERATURE = 0x0B,
    PU_POWER_LINE_FREQUENCY = 0x05
};

MacIOKitUvcController::MacIOKitUvcController()
    : _usbDevice(MACH_PORT_NULL)
    , _deviceInterface(nullptr)
    , _controllerInterface(nullptr)
    , _videoInterfaceIndex(0)
    , _processingUnitId(0)
    , _initialized(false)
{
}

MacIOKitUvcController::~MacIOKitUvcController()
{
    if (_controllerInterface)
    {
        (*_controllerInterface)->Release(_controllerInterface);
        _controllerInterface = nullptr;
    }

    if (_deviceInterface)
    {
        (*_deviceInterface)->Release(_deviceInterface);
        _deviceInterface = nullptr;
    }

    if (_usbDevice != MACH_PORT_NULL)
    {
        IOObjectRelease(_usbDevice);
        _usbDevice = MACH_PORT_NULL;
    }
}

bool MacIOKitUvcController::Initialize(const std::string& deviceIdentifier)
{
    uint32_t locationId = 0;
    try
    {
        locationId = std::stoul(deviceIdentifier);
    }
    catch (...)
    {
        std::cerr << "[WARNING] " << __FILE__ << ":" << __LINE__ << " - Invalid device identifier format: " << deviceIdentifier << std::endl;
        return false;
    }

    if (!FindUsbDeviceAndInterfaces(locationId))
    {
        std::cerr << "[WARNING] " << __FILE__ << ":" << __LINE__ << " - UVC control initialization failed" << std::endl;
        std::cerr << "          Video streaming will continue without camera controls" << std::endl;
        std::cerr << "          This is expected if IOKit USB permissions are denied" << std::endl;
        return false;
    }

    _initialized = true;
    std::cerr << "[INFO] UVC controls initialized successfully" << std::endl;
    return true;
}

bool MacIOKitUvcController::IsInitialized() const
{
    return _initialized;
}

bool MacIOKitUvcController::FindUsbDeviceAndInterfaces(uint32_t locationId)
{
    CFMutableDictionaryRef matchingDict = IOServiceMatching(kIOUSBDeviceClassName);
    if (!matchingDict)
    {
        std::cerr << "[WARNING] " << __FILE__ << ":" << __LINE__ << " - Failed to create USB device matching dictionary" << std::endl;
        return false;
    }

    io_iterator_t iterator;
    kern_return_t kr = IOServiceGetMatchingServices(kIOMasterPortDefault, matchingDict, &iterator);
    if (kr != KERN_SUCCESS)
    {
        std::cerr << "[WARNING] " << __FILE__ << ":" << __LINE__ << " - Failed to get USB device services: " << kr << std::endl;
        return false;
    }

    io_service_t device;
    bool found = false;
    
    io_service_t fallbackDevice = MACH_PORT_NULL;
    IOUSBDeviceInterface320** fallbackInterface = nullptr;

    std::cerr << "Scanning USB devices for Location ID " << locationId << "...\n";

    while ((device = IOIteratorNext(iterator)))
    {
        io_name_t deviceName;
        IORegistryEntryGetName(device, deviceName);

        IOUSBDeviceInterface320** deviceInterface = nullptr;
        IOCFPlugInInterface** plugInInterface = nullptr;
        SInt32 score;

        kr = IOCreatePlugInInterfaceForService(device, kIOUSBDeviceUserClientTypeID,
                                              kIOCFPlugInInterfaceID, &plugInInterface, &score);
        if (kr != KERN_SUCCESS || !plugInInterface)
        {
            IOObjectRelease(device);
            continue;
        }

        HRESULT result = (*plugInInterface)->QueryInterface(plugInInterface,
                                                          CFUUIDGetUUIDBytes(kIOUSBDeviceInterfaceID320),
                                                          (LPVOID*)&deviceInterface);
        (*plugInInterface)->Release(plugInInterface);

        if (result || !deviceInterface)
        {
            IOObjectRelease(device);
            continue;
        }

        UInt32 deviceLocationId = 0;
        (*deviceInterface)->GetLocationID(deviceInterface, &deviceLocationId);
        
        UInt16 vendorId = 0, productId = 0;
        (*deviceInterface)->GetDeviceVendor(deviceInterface, &vendorId);
        (*deviceInterface)->GetDeviceProduct(deviceInterface, &productId);

        std::cerr << "  Found Device: " << deviceName 
                  << " (VID: 0x" << std::hex << vendorId 
                  << " PID: 0x" << productId 
                  << " Loc: 0x" << deviceLocationId << std::dec << ")\n";

        if (deviceLocationId == locationId)
        {
            std::cerr << "    -> MATCH FOUND!\n";
            _usbDevice = device;
            IOObjectRetain(_usbDevice);
            _deviceInterface = deviceInterface;
            found = true;
            
            // Release device (iterator reference)
            IOObjectRelease(device);
            break;
        }
        
        // Fallback: Check for Topdon TC001 / InfiRay P2 Pro (Realtek bridge)
        if (vendorId == 0x0bda && productId == 0x5840 && !fallbackDevice)
        {
             std::cerr << "    -> Candidate match found (VID/PID)\n";
             fallbackDevice = device;
             IOObjectRetain(fallbackDevice);
             fallbackInterface = deviceInterface;
             // Do NOT release deviceInterface here, we keep it
        }
        else
        {
             (*deviceInterface)->Release(deviceInterface);
        }

        IOObjectRelease(device);
    }

    if (!found && fallbackDevice)
    {
        std::cerr << "    -> Using candidate match\n";
        _usbDevice = fallbackDevice;
        _deviceInterface = fallbackInterface;
        found = true;
        fallbackDevice = MACH_PORT_NULL;
        fallbackInterface = nullptr;
    }
    
    // Cleanup fallback if unused (e.g. we found exact match later, though loop breaks on exact match so this is just safety)
    if (fallbackDevice) IOObjectRelease(fallbackDevice);
    if (fallbackInterface) (*fallbackInterface)->Release(fallbackInterface);

    IOObjectRelease(iterator);

    if (!found)
    {
        std::cerr << "[WARNING] " << __FILE__ << ":" << __LINE__ << " - USB device with location ID " << locationId << " not found" << std::endl;
        return false;
    }

    kr = (*_deviceInterface)->USBDeviceOpenSeize(_deviceInterface);
    if (kr != KERN_SUCCESS)
    {
        std::cerr << "[WARNING] " << __FILE__ << ":" << __LINE__ << " - Failed to open USB device with kr=" << kr << std::endl;
        return false;
    }

    IOUSBFindInterfaceRequest request;
    request.bInterfaceClass = kIOUSBFindInterfaceDontCare;
    request.bInterfaceSubClass = kIOUSBFindInterfaceDontCare;
    request.bInterfaceProtocol = kIOUSBFindInterfaceDontCare;
    request.bAlternateSetting = kIOUSBFindInterfaceDontCare;

    io_iterator_t interfaceIterator;
    kr = (*_deviceInterface)->CreateInterfaceIterator(_deviceInterface, &request, &interfaceIterator);
    if (kr != KERN_SUCCESS)
    {
        std::cerr << "[WARNING] " << __FILE__ << ":" << __LINE__ << " - Failed to create interface iterator" << std::endl;
        return false;
    }

    io_service_t interfaceService;
    bool foundControlInterface = false;

    while ((interfaceService = IOIteratorNext(interfaceIterator)))
    {
        IOCFPlugInInterface** plugInInterface = nullptr;
        IOUSBInterfaceInterface220** interfaceInterface = nullptr;
        SInt32 score;

        kr = IOCreatePlugInInterfaceForService(interfaceService,
                                              kIOUSBInterfaceUserClientTypeID,
                                              kIOCFPlugInInterfaceID,
                                              &plugInInterface, &score);
        if (kr != KERN_SUCCESS || !plugInInterface)
        {
            IOObjectRelease(interfaceService);
            continue;
        }

        HRESULT result = (*plugInInterface)->QueryInterface(plugInInterface,
                                                          CFUUIDGetUUIDBytes(kIOUSBInterfaceInterfaceID220),
                                                          (LPVOID*)&interfaceInterface);
        (*plugInInterface)->Release(plugInInterface);

        if (result || !interfaceInterface)
        {
            IOObjectRelease(interfaceService);
            continue;
        }

        UInt8 interfaceNumber = 0;
        UInt8 alternateSetting = 0;
        UInt8 interfaceClass = 0;
        UInt8 interfaceSubClass = 0;

        kr = (*interfaceInterface)->GetInterfaceNumber(interfaceInterface, &interfaceNumber);
        if (kr == KERN_SUCCESS)
        {
            kr = (*interfaceInterface)->GetAlternateSetting(interfaceInterface, &alternateSetting);
        }
        if (kr == KERN_SUCCESS)
        {
            kr = (*interfaceInterface)->GetInterfaceClass(interfaceInterface, &interfaceClass);
        }
        if (kr == KERN_SUCCESS)
        {
            kr = (*interfaceInterface)->GetInterfaceSubClass(interfaceInterface, &interfaceSubClass);
        }

        if (interfaceClass == 0x0E && interfaceSubClass == 0x02)
        {
            _controllerInterface = interfaceInterface;
            _videoInterfaceIndex = interfaceNumber;
            _processingUnitId = 2;

            // Don't open interface to avoid exclusive access error (kIOReturnExclusiveAccess)
            // when AVFoundation is using the device.
            // We will use DeviceRequest on the device interface instead.
            
            foundControlInterface = true;
            IOObjectRelease(interfaceService);
            break;
        }

        (*interfaceInterface)->Release(interfaceInterface);
        IOObjectRelease(interfaceService);
    }

    IOObjectRelease(interfaceIterator);

    if (!foundControlInterface)
    {
        std::cerr << "[WARNING] " << __FILE__ << ":" << __LINE__ << " - Video control interface not found" << std::endl;
        return false;
    }

    return true;
}

bool MacIOKitUvcController::SendControlRequest(int selector, int unitId, void* data, int length)
{
    if (!_deviceInterface)
    {
        return false;
    }

    IOUSBDevRequest controlRequest = {};
    controlRequest.bmRequestType = USBmakebmRequestType(kUSBOut, kUSBClass, kUSBInterface);
    controlRequest.bRequest = UVC_SET_CUR;
    controlRequest.wValue = (selector << 8);
    controlRequest.wIndex = (unitId << 8) | _videoInterfaceIndex;
    controlRequest.wLength = length;
    controlRequest.wLenDone = 0;
    controlRequest.pData = data;

    // Use DeviceRequest to bypass exclusive access check on interface
    IOReturn rc = (*_deviceInterface)->DeviceRequest(_deviceInterface, &controlRequest);

    if (rc != kIOReturnSuccess)
    {
        std::cerr << "[ERROR] " << __FILE__ << ":" << __LINE__ << " - UVC control failed with IOReturn: 0x"
                  << std::hex << rc << std::dec
                  << " (" << mach_error_string(rc) << ")" << std::endl;
        return false;
    }

    return true;
}

bool MacIOKitUvcController::SendControlRequestGet(int type, int selector, int unitId, void* data, int length)
{
    if (!_deviceInterface)
    {
        return false;
    }

    IOUSBDevRequest controlRequest = {};
    controlRequest.bmRequestType = USBmakebmRequestType(kUSBIn, kUSBClass, kUSBInterface);
    controlRequest.bRequest = type;
    controlRequest.wValue = (selector << 8);
    controlRequest.wIndex = (unitId << 8) | _videoInterfaceIndex;
    controlRequest.wLength = length;
    controlRequest.wLenDone = 0;
    controlRequest.pData = data;

    // Use DeviceRequest to bypass exclusive access check on interface
    IOReturn rc = (*_deviceInterface)->DeviceRequest(_deviceInterface, &controlRequest);

    if (rc != kIOReturnSuccess)
    {
        std::cerr << "[ERROR] " << __FILE__ << ":" << __LINE__ << " - UVC get control failed with IOReturn: 0x"
                  << std::hex << rc << std::dec
                  << " (" << mach_error_string(rc) << ")" << std::endl;
        return false;
    }

    return true;
}

bool MacIOKitUvcController::SetBrightness(int value)
{
    std::lock_guard<std::mutex> lock(_controlMutex);

    if (!_controllerInterface)
    {
        std::cerr << "[WARNING] " << __FILE__ << ":" << __LINE__ << " - Set brightness failed (IOKit not initialized)" << std::endl;
        return false;
    }

    uint16_t data = static_cast<uint16_t>(value);
    if (!SendControlRequest(PU_BRIGHTNESS, _processingUnitId, &data, sizeof(data)))
    {
        return false;
    }

    uint16_t readBack = 0;
    if (!SendControlRequestGet(UVC_GET_CUR, PU_BRIGHTNESS, _processingUnitId,
                            &readBack, sizeof(readBack)))
    {
        std::cerr << "[WARNING] " << __FILE__ << ":" << __LINE__ << " - Set brightness to " << value
                  << " succeeded, but read-back failed" << std::endl;
        return false;
    }

    if (readBack != data)
    {
        std::cerr << "[WARNING] " << __FILE__ << ":" << __LINE__ << " - Set brightness to " << value
                  << " but device reports " << readBack << std::endl;
        return false;
    }

    std::cerr << "[INFO] Set brightness to " << value << " (confirmed: " << readBack << ")" << std::endl;
    return true;
}

bool MacIOKitUvcController::SetWhiteBalanceTemperature(int value)
{
    std::lock_guard<std::mutex> lock(_controlMutex);

    if (!_controllerInterface)
    {
        std::cerr << "[WARNING] " << __FILE__ << ":" << __LINE__ << " - Set white balance temperature failed (IOKit not initialized)" << std::endl;
        return false;
    }

    uint16_t data = static_cast<uint16_t>(value);
    if (!SendControlRequest(PU_WHITE_BALANCE_TEMPERATURE, _processingUnitId, &data, sizeof(data)))
    {
        return false;
    }

    uint16_t readBack = 0;
    if (!SendControlRequestGet(UVC_GET_CUR, PU_WHITE_BALANCE_TEMPERATURE, _processingUnitId,
                            &readBack, sizeof(readBack)))
    {
        std::cerr << "[WARNING] " << __FILE__ << ":" << __LINE__ << " - Set white balance temperature to " << value
                  << " succeeded, but read-back failed" << std::endl;
        return false;
    }

    if (readBack != data)
    {
        std::cerr << "[WARNING] " << __FILE__ << ":" << __LINE__ << " - Set white balance temperature to " << value
                  << " but device reports " << readBack << std::endl;
        return false;
    }

    std::cerr << "[INFO] Set white balance temperature to " << value << " (confirmed: " << readBack << ")" << std::endl;
    return true;
}

bool MacIOKitUvcController::SetBacklightCompensation(int value)
{
    std::lock_guard<std::mutex> lock(_controlMutex);

    if (!_controllerInterface)
    {
        std::cerr << "[WARNING] " << __FILE__ << ":" << __LINE__ << " - Set backlight compensation failed (IOKit not initialized)" << std::endl;
        return false;
    }

    uint16_t data = static_cast<uint16_t>(value);
    if (!SendControlRequest(PU_BACKLIGHT_COMPENSATION, _processingUnitId, &data, sizeof(data)))
    {
        return false;
    }

    uint16_t readBack = 0;
    if (!SendControlRequestGet(UVC_GET_CUR, PU_BACKLIGHT_COMPENSATION, _processingUnitId,
                            &readBack, sizeof(readBack)))
    {
        std::cerr << "[WARNING] " << __FILE__ << ":" << __LINE__ << " - Set backlight compensation to " << value
                  << " succeeded, but read-back failed" << std::endl;
        return false;
    }

    if (readBack != data)
    {
        std::cerr << "[WARNING] " << __FILE__ << ":" << __LINE__ << " - Set backlight compensation to " << value
                  << " but device reports " << readBack << std::endl;
        return false;
    }

    std::cerr << "[INFO] Set backlight compensation to " << value << " (confirmed: " << readBack << ")" << std::endl;
    return true;
}

bool MacIOKitUvcController::SetPowerLineFrequency(int value)
{
    std::lock_guard<std::mutex> lock(_controlMutex);

    if (!_controllerInterface)
    {
        std::cerr << "[WARNING] " << __FILE__ << ":" << __LINE__ << " - Set power line frequency failed (IOKit not initialized)" << std::endl;
        return false;
    }

    uint16_t data = static_cast<uint16_t>(value);
    if (!SendControlRequest(PU_POWER_LINE_FREQUENCY, _processingUnitId, &data, sizeof(data)))
    {
        return false;
    }

    uint16_t readBack = 0;
    if (!SendControlRequestGet(UVC_GET_CUR, PU_POWER_LINE_FREQUENCY, _processingUnitId,
                            &readBack, sizeof(readBack)))
    {
        std::cerr << "[WARNING] " << __FILE__ << ":" << __LINE__ << " - Set power line frequency to " << value
                  << " succeeded, but read-back failed" << std::endl;
        return false;
    }

    if (readBack != data)
    {
        std::cerr << "[WARNING] " << __FILE__ << ":" << __LINE__ << " - Set power line frequency to " << value
                  << " but device reports " << readBack << std::endl;
        return false;
    }

    std::cerr << "[INFO] Set power line frequency to " << value << " (confirmed: " << readBack << ")" << std::endl;
    return true;
}

bool MacIOKitUvcController::SetSaturation(int value)
{
    std::lock_guard<std::mutex> lock(_controlMutex);

    if (!_controllerInterface)
    {
        std::cerr << "[WARNING] " << __FILE__ << ":" << __LINE__ << " - Set saturation failed (IOKit not initialized)" << std::endl;
        return false;
    }

    uint16_t data = static_cast<uint16_t>(value);
    if (!SendControlRequest(UVC_SATURATION, _processingUnitId, &data, sizeof(data)))
    {
        return false;
    }

    uint16_t readBack = 0;
    if (!SendControlRequestGet(UVC_GET_CUR, UVC_SATURATION, _processingUnitId,
                            &readBack, sizeof(readBack)))
    {
        std::cerr << "[WARNING] " << __FILE__ << ":" << __LINE__ << " - Set saturation to " << value
                  << " succeeded, but read-back failed" << std::endl;
        return false;
    }

    if (readBack != data)
    {
        std::cerr << "[WARNING] " << __FILE__ << ":" << __LINE__ << " - Set saturation to " << value
                  << " but device reports " << readBack << std::endl;
        return false;
    }

    std::cerr << "[INFO] Set saturation to " << value << " (confirmed: " << readBack << ")" << std::endl;
    return true;
}

bool MacIOKitUvcController::SetSharpness(int value)
{
    std::lock_guard<std::mutex> lock(_controlMutex);

    if (!_controllerInterface)
    {
        std::cerr << "[WARNING] " << __FILE__ << ":" << __LINE__ << " - Set sharpness failed (IOKit not initialized)" << std::endl;
        return false;
    }

    uint16_t data = static_cast<uint16_t>(value);
    if (!SendControlRequest(UVC_SHARPNESS, _processingUnitId, &data, sizeof(data)))
    {
        return false;
    }

    uint16_t readBack = 0;
    if (!SendControlRequestGet(UVC_GET_CUR, UVC_SHARPNESS, _processingUnitId,
                            &readBack, sizeof(readBack)))
    {
        std::cerr << "[WARNING] " << __FILE__ << ":" << __LINE__ << " - Set sharpness to " << value
                  << " succeeded, but read-back failed" << std::endl;
        return false;
    }

    if (readBack != data)
    {
        std::cerr << "[WARNING] " << __FILE__ << ":" << __LINE__ << " - Set sharpness to " << value
                  << " but device reports " << readBack << std::endl;
        return false;
    }

    std::cerr << "[INFO] Set sharpness to " << value << " (confirmed: " << readBack << ")" << std::endl;
    return true;
}

bool MacIOKitUvcController::SetContrast(int value)
{
    std::lock_guard<std::mutex> lock(_controlMutex);

    if (!_controllerInterface)
    {
        std::cerr << "[WARNING] " << __FILE__ << ":" << __LINE__ << " - Set contrast failed (IOKit not initialized)" << std::endl;
        return false;
    }

    uint16_t data = static_cast<uint16_t>(value);
    if (!SendControlRequest(PU_CONTRAST, _processingUnitId, &data, sizeof(data)))
    {
        return false;
    }

    uint16_t readBack = 0;
    if (!SendControlRequestGet(UVC_GET_CUR, PU_CONTRAST, _processingUnitId,
                            &readBack, sizeof(readBack)))
    {
        std::cerr << "[WARNING] " << __FILE__ << ":" << __LINE__ << " - Set contrast to " << value
                  << " succeeded, but read-back failed" << std::endl;
        return false;
    }

    if (readBack != data)
    {
        std::cerr << "[WARNING] " << __FILE__ << ":" << __LINE__ << " - Set contrast to " << value
                  << " but device reports " << readBack << std::endl;
        return false;
    }

    std::cerr << "[INFO] Set contrast to " << value << " (confirmed: " << readBack << ")" << std::endl;
    return true;
}

bool MacIOKitUvcController::SetHue(int value)
{
    std::lock_guard<std::mutex> lock(_controlMutex);

    if (!_controllerInterface)
    {
        std::cerr << "[WARNING] " << __FILE__ << ":" << __LINE__ << " - Set hue failed (IOKit not initialized)" << std::endl;
        return false;
    }

    uint16_t data = static_cast<int16_t>(value);
    if (!SendControlRequest(UVC_HUE, _processingUnitId, &data, sizeof(data)))
    {
        return false;
    }

    uint16_t readBack = 0;
    if (!SendControlRequestGet(UVC_GET_CUR, UVC_HUE, _processingUnitId,
                            &readBack, sizeof(readBack)))
    {
        std::cerr << "[WARNING] " << __FILE__ << ":" << __LINE__ << " - Set hue to " << value
                  << " succeeded, but read-back failed" << std::endl;
        return false;
    }

    if (readBack != data)
    {
        std::cerr << "[WARNING] " << __FILE__ << ":" << __LINE__ << " - Set hue to " << value
                  << " but device reports " << static_cast<int16_t>(readBack) << std::endl;
        return false;
    }

    std::cerr << "[INFO] Set hue to " << value << " (confirmed: " << static_cast<int16_t>(readBack) << ")" << std::endl;
    return true;
}

bool MacIOKitUvcController::SetGamma(int value)
{
    std::lock_guard<std::mutex> lock(_controlMutex);

    if (!_controllerInterface)
    {
        std::cerr << "[WARNING] " << __FILE__ << ":" << __LINE__ << " - Set gamma failed (IOKit not initialized)" << std::endl;
        return false;
    }

    uint16_t data = static_cast<uint16_t>(value);
    if (!SendControlRequest(UVC_GAMMA, _processingUnitId, &data, sizeof(data)))
    {
        return false;
    }

    uint16_t readBack = 0;
    if (!SendControlRequestGet(UVC_GET_CUR, UVC_GAMMA, _processingUnitId,
                            &readBack, sizeof(readBack)))
    {
        std::cerr << "[WARNING] " << __FILE__ << ":" << __LINE__ << " - Set gamma to " << value
                  << " succeeded, but read-back failed" << std::endl;
        return false;
    }

    if (readBack != data)
    {
        std::cerr << "[WARNING] " << __FILE__ << ":" << __LINE__ << " - Set gamma to " << value
                  << " but device reports " << readBack << std::endl;
        return false;
    }

    std::cerr << "[INFO] Set gamma to " << value << " (confirmed: " << readBack << ")" << std::endl;
    return true;
}

bool MacIOKitUvcController::SetAutoWhiteBalanceTemperature(bool enable)
{
    std::lock_guard<std::mutex> lock(_controlMutex);

    if (!_controllerInterface)
    {
        std::cerr << "[WARNING] " << __FILE__ << ":" << __LINE__ << " - Set auto white balance temperature failed (IOKit not initialized)" << std::endl;
        return false;
    }

    uint8_t data = enable ? 0x01 : 0x00;
    if (!SendControlRequest(PU_AUTO_WHITE_BALANCE_TEMPERATURE, _processingUnitId, &data, sizeof(data)))
    {
        return false;
    }

    uint8_t readBack = 0;
    if (!SendControlRequestGet(UVC_GET_CUR, PU_AUTO_WHITE_BALANCE_TEMPERATURE, _processingUnitId,
                            &readBack, sizeof(readBack)))
    {
        std::cerr << "[WARNING] " << __FILE__ << ":" << __LINE__ << " - Set auto white balance temperature to " << (enable ? "true" : "false")
                  << " succeeded, but read-back failed" << std::endl;
        return false;
    }

    bool readBackEnabled = (readBack == 0x01);
    if (readBackEnabled != enable)
    {
        std::cerr << "[WARNING] " << __FILE__ << ":" << __LINE__ << " - Set auto white balance temperature to " << (enable ? "true" : "false")
                  << " but device reports " << (readBackEnabled ? "true" : "false") << std::endl;
        return false;
    }

    std::cerr << "[INFO] Set auto white balance temperature to " << (enable ? "true" : "false")
              << " (confirmed: " << (readBackEnabled ? "true" : "false") << ")" << std::endl;
    return true;
}

bool MacIOKitUvcController::GetBrightness(int* value)
{
    std::lock_guard<std::mutex> lock(_controlMutex);

    if (!_controllerInterface)
    {
        return false;
    }

    uint16_t data = 0;
    if (!SendControlRequestGet(UVC_GET_CUR, PU_BRIGHTNESS, _processingUnitId,
                            &data, sizeof(data)))
    {
        return false;
    }

    *value = static_cast<int>(data);
    return true;
}

bool MacIOKitUvcController::GetWhiteBalanceTemperature(int* value)
{
    std::lock_guard<std::mutex> lock(_controlMutex);

    if (!_controllerInterface)
    {
        return false;
    }

    uint16_t data = 0;
    if (!SendControlRequestGet(UVC_GET_CUR, PU_WHITE_BALANCE_TEMPERATURE, _processingUnitId,
                            &data, sizeof(data)))
    {
        return false;
    }

    *value = static_cast<int>(data);
    return true;
}

bool MacIOKitUvcController::GetBacklightCompensation(int* value)
{
    std::lock_guard<std::mutex> lock(_controlMutex);

    if (!_controllerInterface)
    {
        return false;
    }

    uint16_t data = 0;
    if (!SendControlRequestGet(UVC_GET_CUR, PU_BACKLIGHT_COMPENSATION, _processingUnitId,
                            &data, sizeof(data)))
    {
        return false;
    }

    *value = static_cast<int>(data);
    return true;
}

bool MacIOKitUvcController::GetPowerLineFrequency(int* value)
{
    std::lock_guard<std::mutex> lock(_controlMutex);

    if (!_controllerInterface)
    {
        return false;
    }

    uint16_t data = 0;
    if (!SendControlRequestGet(UVC_GET_CUR, PU_POWER_LINE_FREQUENCY, _processingUnitId,
                            &data, sizeof(data)))
    {
        return false;
    }

    *value = static_cast<int>(data);
    return true;
}

bool MacIOKitUvcController::GetSaturation(int* value)
{
    std::lock_guard<std::mutex> lock(_controlMutex);

    if (!_controllerInterface)
    {
        return false;
    }

    uint16_t data = 0;
    if (!SendControlRequestGet(UVC_GET_CUR, UVC_SATURATION, _processingUnitId,
                            &data, sizeof(data)))
    {
        return false;
    }

    *value = static_cast<int>(data);
    return true;
}

bool MacIOKitUvcController::GetSharpness(int* value)
{
    std::lock_guard<std::mutex> lock(_controlMutex);

    if (!_controllerInterface)
    {
        return false;
    }

    uint16_t data = 0;
    if (!SendControlRequestGet(UVC_GET_CUR, UVC_SHARPNESS, _processingUnitId,
                            &data, sizeof(data)))
    {
        return false;
    }

    *value = static_cast<int>(data);
    return true;
}

bool MacIOKitUvcController::GetContrast(int* value)
{
    std::lock_guard<std::mutex> lock(_controlMutex);

    if (!_controllerInterface)
    {
        return false;
    }

    uint16_t data = 0;
    if (!SendControlRequestGet(UVC_GET_CUR, PU_CONTRAST, _processingUnitId,
                            &data, sizeof(data)))
    {
        return false;
    }

    *value = static_cast<int>(data);
    return true;
}

bool MacIOKitUvcController::GetHue(int* value)
{
    std::lock_guard<std::mutex> lock(_controlMutex);

    if (!_controllerInterface)
    {
        return false;
    }

    uint16_t data = 0;
    if (!SendControlRequestGet(UVC_GET_CUR, UVC_HUE, _processingUnitId,
                            &data, sizeof(data)))
    {
        return false;
    }

    *value = static_cast<int16_t>(data);
    return true;
}

bool MacIOKitUvcController::GetGamma(int* value)
{
    std::lock_guard<std::mutex> lock(_controlMutex);

    if (!_controllerInterface)
    {
        return false;
    }

    uint16_t data = 0;
    if (!SendControlRequestGet(UVC_GET_CUR, UVC_GAMMA, _processingUnitId,
                            &data, sizeof(data)))
    {
        return false;
    }

    *value = static_cast<int>(data);
    return true;
}

bool MacIOKitUvcController::GetAutoWhiteBalanceTemperature(int* value)
{
    std::lock_guard<std::mutex> lock(_controlMutex);

    if (!_controllerInterface)
    {
        return false;
    }

    uint8_t data = 0;
    if (!SendControlRequestGet(UVC_GET_CUR, PU_AUTO_WHITE_BALANCE_TEMPERATURE, _processingUnitId,
                            &data, sizeof(data)))
    {
        return false;
    }

    *value = (data == 0x01) ? 1 : 0;
    return true;
}
