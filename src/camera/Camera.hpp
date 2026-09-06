#ifndef CAMERA_HPP
#define CAMERA_HPP

#include "ICamera.hpp"

#ifdef __APPLE__
    #include "MacCameraImpl.hpp"
#else
    #include "UvcCameraImpl.hpp"
#endif

#ifdef __APPLE__
    typedef MacCameraImpl Camera;
#else
    typedef UvcCameraImpl Camera;
#endif

namespace CameraConstants
{
    const int FRAME_WIDTH = 256;
    const int FRAME_HEIGHT = 384;

    // Thermal sub-frame: sensor is natively 256x192 landscape. The full USB
    // frame is 256x384 because the raw temperatures are conveyed in the
    // second half of the YUYV frame.
    const int THERMAL_WIDTH = 256;
    const int THERMAL_HEIGHT = 192;
    const int FRAME_FPS = 25;
    const size_t FRAME_SIZE = FRAME_WIDTH * FRAME_HEIGHT * 2;
}

#endif
