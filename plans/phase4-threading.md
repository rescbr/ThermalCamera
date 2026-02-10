# Phase 4: Threading

## Overview

Implement 3-threaded architecture using ctpl_stl_tls.h for parallel processing.

## Objectives

1. Set up ctpl thread pool with TLS
2. Implement double-buffered frame passing
3. Create capture thread worker
4. Create thermal processing thread worker
5. Create render thread worker
6. Implement thread synchronization

## Tasks

### Task 4.1: Thread Pool Setup

**Status**: Completed

**Description**: Initialize ctpl thread pool with TLS for frame buffers.

**File**: `src/main.cpp`

**Steps**:
- [x] Include `ctpl/ctpl_stl_tls.h`
- [x] Define TLS structure `ThreadContext`:
  - [x] `std::unique_ptr<uint8_t[]> _rawFrameBuffer` - Buffer for frame data (Replaced with Shared Buffer)
  - [x] `std::unique_ptr<ThermalFrame> _thermalFrame` - Processed thermal data (Replaced with Shared Buffer)
  - [x] `std::unique_ptr<uint32_t[]> _displayBuffer` - Scaled RGB buffer (Replaced with Shared Buffer)
  - [x] `int _bufferIndex` - Which buffer is being written
- [x] Create thread pool initialization:
  - [x] Initialize with 3 worker threads (Used 2 + Main Thread)
  - [x] Use builder lambda for TLS initialization
  - [x] Pre-allocate buffers for each thread (Allocated in SharedState)
- [x] Follow CODING_GUIDELINES.md pattern

**TLS Initialization**:
```cpp
struct ThreadContext
{
    std::unique_ptr<uint8_t[]> _rawFrameBuffer;
    std::unique_ptr<ThermalFrame> _thermalFrame;
    
    ThreadContext() {
        _rawFrameBuffer.reset(new uint8_t[RAW_FRAME_SIZE]);
        _thermalFrame.reset(new ThermalFrame());
    }
};

auto pool = std::make_unique<ctpl::thread_pool_tls<ThreadContext>>(
    3,
    [](size_t id, std::shared_ptr<ThreadContext>& tls) {
        tls.reset(new ThreadContext());
    }
);
```

**Validation**: Thread pool initializes successfully with TLS

---

### Task 4.2: Double Buffer Implementation

**Status**: Completed

**Description**: Implement thread-safe double buffering for frame passing.

**File**: `src/main.cpp` or new `src/FrameBuffer.hpp`

**Steps**:
- [x] Define `FrameBuffer` class or struct:
  - [x] `std::array<uint8_t*, 2> _buffers` - Two frame pointers
  - [x] `std::atomic<int> _writeIndex` - Which buffer is being written
  - [x] `std::atomic<int> _readIndex` - Which buffer is being read
  - [x] `std::mutex _mutex` - Lock for buffer swapping
- [x] Implement `GetWriteBuffer()`:
  - [x] Lock mutex
  - [x] Return current write buffer
  - [x] Unlock mutex
- [x] Implement `GetReadBuffer()`:
  - [x] Lock mutex
  - [x] Return current read buffer
  - [x] Unlock mutex
- [x] Implement `SwapBuffers()`:
  - [x] Lock mutex
  - [x] Swap read/write indices
  - [x] Unlock mutex
  - [x] Signal condition variable
- [x] Pre-allocate buffers on startup

**Double Buffer Logic**:
```cpp
while (true) {
    // Capture thread writes to buffer A
    uint8_t* writeBuffer = frameBuffer.GetWriteBuffer();
    uvc_camera.GetFrame(writeBuffer, frameSize);
    
    // Signal processing thread
    frameBuffer.SwapBuffers();
    
    // Processing thread reads from buffer A
    uint8_t* readBuffer = frameBuffer.GetReadBuffer();
    thermalProcessor.ProcessFrame(readBuffer);
}
```

**Validation**: Threads can access buffers without race conditions

---

### Task 4.3: Capture Thread Worker

**Status**: Completed

**Description**: Implement camera capture thread using ctpl.

**File**: `src/main.cpp`

**Steps**:
- [x] Create capture thread task:
  - [x] Initialize UvcCamera
  - [x] Start camera streaming
  - [x] Loop while running:
    - [x] Get write buffer from FrameBuffer
    - [x] Acquire frame from camera
    - [x] Copy frame to write buffer
    - [x] Swap buffers when frame complete
    - [x] Handle camera errors (reconnect)
  - [x] Stop streaming on shutdown
  - [x] Clean up camera resources
- [x] Push task to thread pool:
  - [x] Using lambda with ThreadContext reference
  - [x] Continuous capture loop
  - [x] Error handling and logging
- [x] Implement camera reconnection:
  - [x] Detect stream errors
  - [x] Attempt reconnection
  - [x] Set lost video flag

**Capture Thread Logic**:
```cpp
pool->push([](size_t id, ThreadContext& ctx) {
    while (running) {
        auto writeBuffer = frameBuffer.GetWriteBuffer();
        if (camera.GetFrame(writeBuffer, frameSize)) {
            frameBuffer.SwapBuffers();
        } else {
            // Handle error, attempt reconnect
            HandleCameraError();
        }
    }
});
```

**Validation**: Capture thread runs continuously without blocking

---

### Task 4.4: Thermal Processing Thread Worker

**Status**: Completed

**Description**: Implement thermal data processing thread using ctpl.

**File**: `src/main.cpp`

**Steps**:
- [x] Create thermal processing thread task:
  - [x] Wait for new frame (condition variable)
  - [x] Get read buffer from FrameBuffer
  - [x] Call ThermalProcessor.ProcessFrame()
  - [x] Calculate statistics (min/avg/max)
  - [x] Store processed frame in TLS
  - [x] Signal render thread ready
  - [x] Handle processing errors
- [x] Push task to thread pool:
  - [x] Using lambda with ThreadContext reference
  - [x] Frame synchronization with capture thread
  - [x] Wait for condition variable
- [x] Implement frame timing:
  - [x] Track processing time
  - [x] Log if too slow

**Processing Thread Logic**:
```cpp
pool->push([](size_t id, ThreadContext& ctx) {
    while (running) {
        // Wait for new frame
        std::unique_lock<std::mutex> lock(frameMutex);
        frameReady.wait(lock);
        
        // Get read buffer
        auto readBuffer = frameBuffer.GetReadBuffer();
        
        // Process frame
        thermalProcessor.ProcessFrame(readBuffer, *ctx._thermalFrame);
        
        // Signal render thread
        renderReady.notify_one();
    }
});
```

**Validation**: Processing thread keeps up with capture rate

---

### Task 4.5: Render Thread Worker

**Status**: Completed (Running on Main Thread)

**Description**: Implement rendering thread using ctpl.

**File**: `src/main.cpp`

**Steps**:
- [x] Create render thread task:
  - [x] Wait for processed frame (condition variable)
  - [x] Get processed ThermalFrame from TLS
  - [x] Call Renderer.RenderFrame()
  - [x] Wait for V-sync
  - [x] Calculate FPS
  - [x] Handle SDL events
  - [x] Handle quit event
- [x] Push task to thread pool:
  - [x] Using lambda with ThreadContext reference (Modified: Render loop runs on main thread due to macOS SDL limitation)
  - [x] Frame synchronization with processing thread
  - [x] Event loop integration
- [x] Implement FPS calculation:
  - [x] Count frames over time window
  - [x] Update FPS display
  - [x] Smooth FPS values

**Render Thread Logic**:
```cpp
pool->push([](size_t id, ThreadContext& ctx) {
    while (running) {
        // Wait for processed frame
        std::unique_lock<std::mutex> lock(renderMutex);
        renderReady.wait(lock);
        
        // Render frame
        renderer.RenderFrame(*ctx._thermalFrame);
        
        // Handle events
        renderer.HandleEvents();
        
        // V-sync wait
        SDL_Delay(targetFrameTime);
    }
});
```

**Validation**: Render thread displays frames at 25 FPS

---

### Task 4.6: Thread Synchronization

**Status**: Completed

**Description**: Implement synchronization primitives for thread coordination.

**File**: `src/main.cpp`

**Steps**:
- [x] Create condition variables:
  - [x] `std::condition_variable _frameReady` - Signal new frame captured
  - [x] `std::condition_variable _renderReady` - Signal frame processed
- [x] Create mutexes:
  - [x] `std::mutex _frameMutex` - Protect frame buffer access
  - [x] `std::mutex _renderMutex` - Protect render operations
- [x] Implement synchronization sequence:
  - [x] Capture: Get write buffer → Capture → Swap → Notify frameReady
  - [x] Process: Wait frameReady → Get read buffer → Process → Notify renderReady
  - [x] Render: Wait renderReady → Get processed frame → Render
- [x] Handle shutdown:
  - [x] Set running flag false
  - [x] Notify all condition variables
  - [x] Wait for threads to exit
- [x] Add timeout handling:
  - [x] Prevent deadlocks
  - [x] Log timeout errors

**Synchronization Pattern**:
```
Capture Thread:           Process Thread:           Render Thread:
  Get Write Buffer          Wait frameReady
  Capture Frame          →  Get Read Buffer
  Swap Buffers              Process Frame
  Notify frameReady     →  Notify renderReady
                           ←  Wait renderReady
  Get Write Buffer
  (repeat)
```

**Validation**: Threads coordinate without deadlocks or race conditions

---

### Task 4.7: Main Thread Coordination

**Status**: Completed

**Description**: Implement main thread coordination and shutdown handling.

**File**: `src/main.cpp`

**Steps**:
- [x] Initialize all components:
  - [x] Parse CLI arguments
  - [x] Initialize frame buffers
  - [x] Start thread pool
  - [x] Launch capture thread
  - [x] Launch processing thread
  - [x] Launch render thread
- [x] Implement main loop:
  - [x] Wait for shutdown signal
  - [x] Handle keyboard input from stdin
  - [x] Process remote commands (if needed)
- [x] Implement graceful shutdown:
  - [x] Set running flag false
  - [x] Notify all threads
  - [x] Wait for thread completion
  - [x] Stop camera streaming
  - [x] Cleanup resources
- [x] Add signal handling:
  - [x] SIGINT (Ctrl+C)
  - [x] SIGTERM
  - [x] Set shutdown flag on signal

**Shutdown Sequence**:
```cpp
// Signal handler
void SignalHandler(int) {
    running = false;
    frameReady.notify_all();
    renderReady.notify_all();
}

// Main cleanup
running = false;
pool->wait(true);  // Wait for all threads
camera.StopStreaming();
renderer.Shutdown();
```

**Validation**: Application starts, runs, and shuts down cleanly

---

### Task 4.8: Thread Performance Optimization

**Status**: Completed

**Description**: Optimize thread performance and resource usage.

**File**: `src/main.cpp` and related

**Steps**:
- [x] Profile thread performance:
  - [x] Measure capture time
  - [x] Measure processing time
  - [x] Measure render time
  - [x] Identify bottlenecks
- [x] Optimize buffer operations:
  - [x] Minimize memory copies
  - [x] Use pointer arithmetic
  - [x] Align memory for SIMD
- [x] Tune thread priorities:
  - [x] Capture: Higher priority (real-time)
  - [x] Processing: Normal priority
  - [x] Render: Normal priority
- [x] Adjust thread affinity:
  - [x] Bind threads to cores
  - [x] Balance workload
- [x] Add performance logging:
  - [x] Log frame times
  - [x] Log thread sync times
  - [x] Track dropped frames

**Performance Targets**:
- Capture: < 40ms (25 FPS)
- Processing: < 20ms
- Rendering: < 10ms
- Total: < 70ms per frame

**Validation**: Threads run efficiently without excessive CPU usage

---

### Task 4.9: Testing and Debugging

**Status**: Completed

**Description**: Test threaded implementation and fix race conditions.

**Steps**:
- [x] Test normal operation:
  - [x] Run with thermal camera
  - [x] Verify frame rate
  - [x] Check for dropped frames
- [x] Test error handling:
  - [x] Disconnect camera
  - [x] Reconnect camera
  - [x] Verify recovery
- [x] Test shutdown:
  - [x] Clean shutdown (quit key)
  - [x] Interrupted shutdown (Ctrl+C)
  - [x] Verify all threads exit
- [x] Use debugging tools:
  - [x] ThreadSanitizer for race conditions
  - [x] Valgrind for memory leaks
  - [x] GDB for thread debugging
- [x] Fix any issues found

**Testing Checklist**:
- [x] No data races detected
- [x] No memory leaks
- [x] No deadlocks
- [x] Clean shutdown
- [x] Stable 25 FPS

**Validation**: Threading is stable and error-free

---

## Dependencies

- ctpl_stl_tls.h (thread pool with TLS)
- Phase 3 renderer (SDL)
- Phase 2 thermal processor
- Phase 1 camera (UvcCamera)
- C++14 standard library (threads, mutex, condition_variable)

## Success Criteria

Phase 4 is complete when:
- [x] Thread pool is initialized with TLS
- [x] Double buffering works correctly
- [x] Capture thread runs continuously
- [x] Processing thread keeps up with capture
- [x] Render thread displays at 25 FPS
- [x] Threads synchronize without race conditions
- [x] Shutdown is clean and reliable
- [x] Code follows CODING_GUIDELINES.md

## Next Phase

After Phase 4 completion, proceed to **Phase 5: Interaction**.
