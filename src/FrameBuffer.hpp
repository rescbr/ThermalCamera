#ifndef FRAMEBUFFER_HPP
#define FRAMEBUFFER_HPP

#include <array>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include "Error.hpp"

class FrameBuffer
{
public:
    FrameBuffer(size_t frameSize)
        : _frameSize(frameSize), _writeIndex(0), _readIndex(1)
    {
        try {
            _buffers[0] = std::make_unique<uint8_t[]>(frameSize);
            _buffers[1] = std::make_unique<uint8_t[]>(frameSize);
        } catch (const std::bad_alloc& e) {
            throw ERROR_CODE(Error::ErrorCode::MemoryError,
                           std::string("Failed to allocate frame buffer: ") + e.what());
        }
        if (!_buffers[0] || !_buffers[1]) {
            throw ERROR_CODE(Error::ErrorCode::MemoryError,
                           "Failed to allocate frame buffers");
        }
    }

    // Get the buffer currently designated for writing
    uint8_t* GetWriteBuffer()
    {
        std::lock_guard<std::mutex> lock(_mutex);
        return _buffers[_writeIndex].get();
    }

    // Get the buffer currently designated for reading
    uint8_t* GetReadBuffer()
    {
        std::lock_guard<std::mutex> lock(_mutex);
        return _buffers[_readIndex].get();
    }

    // Swap the read and write buffers
    void SwapBuffers()
    {
        std::lock_guard<std::mutex> lock(_mutex);
        int temp = _writeIndex;
        _writeIndex = _readIndex.load();
        _readIndex = temp;
        // Signal that a new frame is ready to be read
        _frameReady.notify_one();
    }

    // Wait for a new frame to be ready
    void WaitForNewFrame()
    {
        std::unique_lock<std::mutex> lock(_mutex);
        _frameReady.wait(lock);
    }
    
    // Wake up any waiting threads (e.g. for shutdown)
    void NotifyAll()
    {
        _frameReady.notify_all();
    }

    size_t GetFrameSize() const { return _frameSize; }

private:
    size_t _frameSize;
    std::array<std::unique_ptr<uint8_t[]>, 2> _buffers;
    std::atomic<int> _writeIndex;
    std::atomic<int> _readIndex;
    std::mutex _mutex;
    std::condition_variable _frameReady;
};

#endif // FRAMEBUFFER_HPP
