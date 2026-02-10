# C++ Engineering Guidelines & Code Style

## 1. Project Structure & Build System (Meson)

This project uses **Meson** as the primary build system.

### Directory Layout
*   **`/` (Root):** Contains the primary `meson.build`. All build definitions should ideally reside here or be included from here.
*   **`src/`:** Contains all implementation files (`.cpp`, `.c`) and private headers.
*   **`inc/`:** Contains shared or public header files (`.hpp`, `.h`).
*   **`subprojects/`:** Contains external dependencies managed via Meson Wraps or CMake subprojects (e.g., `ffts`, `glm`, `cminpack`).
*   **`build/` or `builddir/`:** The standard locations for build artifacts. **Never** commit these directories.

### Meson Configuration (`meson.build`)
*   **Setup:** Initialize the build directory using `meson setup builddir`.
*   **Compilation:** Run builds using `meson compile -C builddir`.
*   **Sources:** You may use explicit file lists or **globs** (if the project policy allows) to define source sets.
*   **Dependencies:**
    *   Use `subproject()` for dependencies.
    *   Use the `cmake` module for dependencies that only provide CMake build files.
    ```python
    cmake = import('cmake')
    lib_proj = cmake.subproject('libname', cmake_options: ['-DBUILD_SHARED_LIBS=ON'])
    lib_dep = lib_proj.dependency('lib_target')
    ```

## 2. C++ Code Style

### Formatting & Layout
*   **Indentation:** **4 spaces**. Do not use tabs.
*   **Brace Style (Allman):** Open and close braces on their own lines for all control structures, classes, and namespaces.
    ```cpp
    void ProcessData()
    {
        if (isValid)
        {
            // logic
        }
    }
    ```
*   **Brace Style (Lambdas):** Use **K&R** (attached brace) for lambdas to visually distinguish them from control blocks.
    ```cpp
    auto callback = [](int value) {
        return value * 2;
    };
    ```

### Naming Conventions
*   **Namespaces:** `PascalCase` (e.g., `CoreLogic`).
*   **Classes & Functions:** `PascalCase` (e.g., `DataProcessor`, `RunAnalysis`).
*   **Member Variables:** `_camelCase` (prefixed with a single underscore).
    ```cpp
    class Processor {
        size_t _bufferSize;
        std::vector<double> _dataPoints;
    };
    ```
*   **Local Variables:** `camelCase` is preferred.
    *   *Exception:* `snake_case` is permitted for mathematical variables or matrix indices (e.g., `sigma_sq`, `row_idx`, `x_val`) to match mathematical notation.

## 3. Concurrency Pattern (CTPL with TLS)

For parallel tasks requiring thread-local resources (e.g., database connections, large buffers, computation contexts), use the `ctpl_stl_tls.h` pattern to avoid locking.

**Implementation Pattern:**
1.  **Define TLS Struct:** Create a struct holding the specific data required *per thread*.
2.  **Initialize Pool:** Use the constructor that accepts a builder lambda to initialize the TLS data.
3.  **Execute:** Tasks receive a reference to their specific TLS instance.

```cpp
#include "ctpl/ctpl_stl_tls.h"

// 1. Define Thread Local Storage Data
struct WorkerContext
{
    std::vector<double> _scratchBuffer;
    HeavyObject _calculator;
    
    WorkerContext() : _scratchBuffer(1024) {}
};

class TaskManager
{
    std::unique_ptr<ctpl::thread_pool_tls<WorkerContext>> _pool;

public:
    void Initialize(size_t threadCount)
    {
        // 2. Initialize Pool with Builder Lambda
        _pool.reset(new ctpl::thread_pool_tls<WorkerContext>(
            threadCount,
            [](size_t id, std::shared_ptr<WorkerContext>& tls) {
                // Initialize the TLS object for this specific thread
                tls.reset(new WorkerContext());
            }
        ));
    }

    void RunBatch()
    {
        // 3. Push Task (accepts thread ID and TLS reference)
        _pool->push([](size_t id, WorkerContext& tls) {
            // Access generic TLS data without locking
            tls._calculator.Compute(tls._scratchBuffer);
        });
    }
};
```

## 4. CLI Argument Parsing

Use the `cmdline` library with a fluent interface pattern in `main()`.

*   **Configuration:** Define all flags at the top of `main`.
*   **Validation:** Check for errors (`!cmd.parse()`) and help flags (`cmd.exist("help")`) immediately.
*   **Defaults:** Always provide sensible defaults, utilizing `std::thread::hardware_concurrency()` for threading.

```cpp
#include "cmdline/cmdline.h"

int main(int argc, char **argv)
{
    cmdline::parser cmd;
    cmd.add<size_t>("threads", 't', "thread count", false, std::thread::hardware_concurrency());
    cmd.add<std::string>("config", 'c', "config file", true, "");

    if (!cmd.parse(argc, argv) || cmd.exist("help"))
    {
        std::cerr << cmd.error_full() << cmd.usage();
        return 0;
    }
    
    // Application logic...
}
```

## 5. Input/Output & Logging

*   **Stream Separation:**
    *   **`std::cerr`**: Use exclusively for logging, debug information, status updates, and error messages.
    *   **`std::cout`**: Use exclusively for the "actual" output of the program (e.g., CSV data, final results) that is intended to be piped to other tools.
*   **File Parsing:**
    *   Prefer `std::ifstream` with the stream extraction operator (`>>`) for simple token-based parsing.
    *   Always validate file open status and data counts immediately after loading.

## 6. Modern C++ Practices

*   **Standard Version:** Target **C++17** or higher.
*   **Memory Management:**
    *   **Strictly avoid raw pointers** for ownership.
    *   Use `std::unique_ptr` for exclusive ownership.
    *   Use `std::shared_ptr` only when ownership is shared (e.g., with thread pools).
*   **Type Inference:** Use `auto` for iterators, lambda returns, and complex template types.
*   **Math:** When performing complex math (curve fitting, signal processing), prefer well-tested libraries or wrappers (like `LsqFit`, `ffts`) over raw implementation.
