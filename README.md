# ESP-IDF C++23 Modern Practices Project

A collection of ESP-IDF examples demonstrating modern C++23 features, patterns, and best practices for embedded systems development.

## Project Overview

This project showcases the integration of **C++23** features with **ESP-IDF v5.5** for ESP32 development. The examples focus on clean, maintainable, and type-safe code while leveraging modern C++ idioms suitable for constrained embedded environments.

## Key C++23 Features Demonstrated

### Language Features
- **`std::span`** - Type-safe view over contiguous sequences (replacing pointer+size pairs)
- **Abbreviated Function Templates** (`auto` parameters) - Cleaner template syntax
- **`if` with Initializer** - Scope-limited variable declarations
- **Structured Bindings** - Decomposing tuple-like objects
- **Concepts & Constraints** (`template<SensorType>`) - Compile-time interface validation
- **Designated Initializers** (C++20) - Named struct member initialization

### Standard Library Features
- **`std::format`** - Type-safe, locale-aware formatting (ESP-IDF compatible)
- **`std::jthread`** - RAII thread wrapper with automatic join
- **`std::variant` + `std::visit`** - Type-safe sum types and visitation
- **`std::chrono` literals** (`2s`, `500ms`) - Readable time specifications

### ESP-IDF Integration
- **FreeRTOS task naming and core affinity**
- **ESP logging macros** (`ESP_LOGI`, `ESP_LOGW`, `ESP_LOGE`)
- **Memory-constrained optimizations** (static allocation, stack monitoring)

## Example Breakdown

### 1. `cpp_variant.cpp` - Finite State Machine
Implements a variant-based state machine demonstrating:
- `std::variant` for state representation
- `std::visit` with generic lambdas for state transitions
- `std::span` for sensor data processing
- Compile-time feature detection with `__has_include`
- `[[maybe_unused]]` attribute for intentional unused variables

### 2. `cpp_span_visit_concept.cpp` - Sensor Monitoring System
Shows advanced type-safe patterns:
- Concept-based sensor interfaces (`SensorType` concept)
- Variant-based state management with visitor pattern
- Buffer statistics using `std::span` views
- Thread-safe state machine with multiple managers
- Configuration helpers with `[[nodiscard]]`

### 3. `cpp_pthread.cpp` - Thread Management
Demonstrates modern threading practices:
- `std::jthread` with RAII lifecycle management
- Core affinity configuration using ESP-IDF pthread wrappers
- Immediately Invoked Lambda Expressions (IILE) for scope isolation
- Stack monitoring with `uxTaskGetStackHighWaterMark`

## Build Configuration

```cmake
# CMakeLists.txt snippet
idf_component_register(
    SRCS "cpp_variant.cpp"
    INCLUDE_DIRS ".")
target_compile_options(${COMPONENT_LIB} PRIVATE -std=gnu++23)
```

**Compiler Requirements**: GCC 13.2+ with `-std=gnu++23` flag

## Embedded-Specific Considerations

### Memory Management
- Prefer stack allocation and static storage where possible
- Use `std::array` instead of `std::vector` for fixed-size buffers
- `std::span` provides bounds-checked views without allocation
- Monitor stack usage with FreeRTOS utilities

### Type Safety
- `std::variant` eliminates runtime type errors
- Concepts provide compile-time interface checking
- `std::format` prevents format string vulnerabilities
- `[[nodiscard]]` ensures return values are used

### Thread Safety
- `std::jthread` ensures proper thread lifecycle
- Core pinning optimizes cache locality
- Task priorities follow FreeRTOS conventions
- Stack size tuning for memory-constrained environments

## Usage Patterns

### State Machine Implementation
```cpp
// Variant-based state representation
using State = std::variant<Idle, Running, Error>;

// Type-safe visitation
std::visit([&event]<typename S>(S& state) {
    handle(state, event);
}, current_state_);
```

### Sensor Abstraction
```cpp
template<typename T>
concept SensorType = requires(T t) {
    { t.read() } -> std::convertible_to<float>;
    { t.get_id() } -> std::convertible_to<int>;
};

// Constrained template
template<SensorType... Sensors>
void process_sensors(Sensors&&... sensors);
```

### Thread Configuration
```cpp
[[nodiscard]] esp_pthread_cfg_t create_config(
    std::string_view name, 
    int core_id, 
    size_t stack_size, 
    int prio) 
{
    auto cfg = esp_pthread_get_default_config();
    cfg.thread_name = name.data();
    cfg.pin_to_core = core_id;
    cfg.stack_size = stack_size;
    cfg.prio = prio;
    return cfg;
}
```

## Building and Flashing

```bash
# Set ESP-IDF environment
source $IDF_PATH/export.sh

# Configure and build
idf.py set-target esp32
idf.py build

# Flash and monitor
idf.py -p /dev/ttyUSB0 flash monitor
```

## Project Structure
```
main/
├── CMakeLists.txt          # Build configuration with C++23 flag
├── cpp_variant.cpp         # Variant-based state machine example
├── cpp_span_visit_concept.cpp  # Concepts and spans example
└── cpp_pthread.cpp         # Modern threading example
```

## Best Practices Demonstrated

1. **Compile-Time Safety**: Extensive use of concepts, variants, and spans
2. **Memory Efficiency**: Stack allocation, static buffers, and views
3. **Thread Safety**: RAII thread management and proper synchronization
4. **Code Clarity**: Abbreviated templates, structured bindings, and designated initializers
5. **Embedded Awareness**: Stack monitoring, core affinity, and logging integration

This project serves as a reference for modern C++ development in ESP-IDF environments, balancing advanced language features with the constraints of embedded systems.