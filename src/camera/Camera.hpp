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
    const int FRAME_FPS = 25;
    const size_t FRAME_SIZE = FRAME_WIDTH * FRAME_HEIGHT * 2;
}

#endif
