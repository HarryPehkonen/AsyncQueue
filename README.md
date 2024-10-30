# AsyncQueue

A header-only C++ library implementing a thread-safe asynchronous queue with timeout support.

## Features
- Thread-safe operations
- Configurable capacity
- Timeout support for push/pop operations
- Move semantics support
- Extension support through virtual hooks
- Header-only implementation

## Integration

### Method 1: CMake FetchContent
```cmake
include(FetchContent)

FetchContent_Declare(
    async_queue
    GIT_REPOSITORY https://github.com/yourusername/async_queue.git
    GIT_TAG v0.1.0  # Specify a tag or commit hash
)

FetchContent_MakeAvailable(async_queue)

# Link against the library
target_link_libraries(your_target PRIVATE async_queue::async_queue)
```

### Method 2: Manual Installation
```bash
git clone https://github.com/yourusername/async_queue.git
cd async_queue
mkdir build && cd build
cmake ..
cmake --build .
sudo cmake --install .
```

Then in your project's CMakeLists.txt:
```cmake
find_package(async_queue REQUIRED)
target_link_libraries(your_target PRIVATE async_queue::async_queue)
```

## Usage Example
```cpp
#include <async_queue/async_queue.hpp>

async_queue::AsyncQueue<int> queue(100);  // Queue with capacity 100

// Producer
queue.push(42);
queue.try_push(43, std::chrono::milliseconds(100));

// Consumer
auto value = queue.pop();
if (value) {
    std::cout << "Received: " << *value << std::endl;
}
```

## Building Tests
```bash
mkdir build && cd build
cmake -DASYNC_QUEUE_BUILD_TESTS=ON ..
cmake --build .
ctest
```

## License
[Add your chosen license here]
