// cpp_span_visit_concept.cpp
#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <array>
#include <span>
#include <variant>
#include <string>
#include <format>
#include <optional>
#include <concepts>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <esp_pthread.h>

// --- C++23 Feature Test Macros ---
#ifdef __has_include
#  if __has_include(<version>)
#    include <version>
#  endif
#  if __has_include(<ranges>)
#    include <ranges>
#    define HAS_RANGES 1
#  endif
#endif

using namespace std::chrono_literals;
constexpr auto STATE_UPDATE_INTERVAL = 2s;
constexpr auto LOG_INTERVAL = 5s;

// --- Concepts & Constraints ---
template<typename T>
concept SensorType = requires(T t) {
    { t.read() } -> std::convertible_to<float>;
    { t.get_id() } -> std::convertible_to<int>;
};

template<typename T>
concept StateType = requires {
    requires std::is_enum_v<T> || std::is_class_v<T>;
};

// --- State Variant Definition ---
enum class StateId { IDLE, MONITORING, ALERT, CALIBRATING };
struct IdleState {};
struct MonitoringState { 
    float average_value; 
    int sample_count;
};
struct AlertState {
    std::string_view message;
    float threshold;
};
struct CalibratingState {
    float reference_value;
    int calibration_step;
};

using StateVariant = std::variant<
    IdleState, 
    MonitoringState, 
    AlertState, 
    CalibratingState
>;

// --- Sensor Concepts Implementation ---
class TemperatureSensor {
public:
    auto read() const -> float { return 23.5f + (rand() % 100) * 0.01f; }
    auto get_id() const -> int { return 1; }
};

class HumiditySensor {
public:
    auto read() const -> float { return 45.0f + (rand() % 100) * 0.02f; }
    auto get_id() const -> int { return 2; }
};

class PressureSensor {
public:
    auto read() const -> float { return 1013.25f + (rand() % 100) * 0.05f; }
    auto get_id() const -> int { return 3; }
};

// --- State Machine with Variants & Visit ---
class StateMachine {
private:
    StateVariant current_state_{IdleState{}};
    StateId current_id_{StateId::IDLE};
    std::array<float, 10> sensor_buffer_{};
    size_t buffer_index_{0};

public:
    // --- Public accessor for buffer index ---
    [[nodiscard]] auto get_buffer_index() const -> size_t {
        return buffer_index_;
    }

    // --- Abbreviated Function Templates (C++20) ---
    auto transition_to(StateVariant new_state) -> void {
        current_state_ = new_state;
        current_id_ = static_cast<StateId>(current_state_.index());
    }

    // --- Using std::visit with variants ---
    auto get_state_info() const -> std::string {
        return std::visit([]<typename T>(const T& state) -> std::string {
            if constexpr (std::is_same_v<T, IdleState>) {
                return "Idle - Waiting for commands";
            } else if constexpr (std::is_same_v<T, MonitoringState>) {
                return std::format("Monitoring - Avg: {:.2f}, Samples: {}",
                    state.average_value, state.sample_count);
            } else if constexpr (std::is_same_v<T, AlertState>) {
                return std::format("ALERT: {} (Threshold: {:.1f})",
                    state.message, state.threshold);
            } else if constexpr (std::is_same_v<T, CalibratingState>) {
                return std::format("Calibrating - Ref: {:.2f}, Step: {}",
                    state.reference_value, state.calibration_step);
            }
        }, current_state_);
    }

    // --- Process sensors using span ---
    template<SensorType... Sensors>
    auto process_sensors(Sensors&&... sensors) -> void {
        // Create span of sensor readings
        std::array<float, sizeof...(sensors)> readings{sensors.read()...};
        std::span<const float> readings_span{readings};
        
        // Update buffer with first reading
        if (!readings_span.empty()) {
            sensor_buffer_[buffer_index_ % sensor_buffer_.size()] = readings_span[0];
            buffer_index_++;
        }

        // State transition logic
        std::visit([this, readings_span](auto& state) {
            using T = std::decay_t<decltype(state)>;
            
            if constexpr (std::is_same_v<T, IdleState>) {
                if (!readings_span.empty() && readings_span[0] > 20.0f) {
                    transition_to(MonitoringState{readings_span[0], 1});
                }
            } else if constexpr (std::is_same_v<T, MonitoringState>) {
                state.sample_count++;
                
                // Calculate average using span
                float sum = 0;
                for (auto val : sensor_buffer_ | std::views::take(buffer_index_)) {
                    sum += val;
                }
                state.average_value = sum / std::min(buffer_index_, sensor_buffer_.size());
                
                if (state.average_value > 30.0f) {
                    transition_to(AlertState{"Temperature High", 30.0f});
                }
            } else if constexpr (std::is_same_v<T, AlertState>) {
                if (!readings_span.empty() && readings_span[0] < 25.0f) {
                    transition_to(CalibratingState{22.5f, 1});
                }
            } else if constexpr (std::is_same_v<T, CalibratingState>) {
                state.calibration_step++;
                if (state.calibration_step > 5) {
                    transition_to(IdleState{});
                }
            }
        }, current_state_);
    }

    // --- Get buffer statistics using span ---
    auto get_buffer_stats() const -> std::tuple<float, float> {
        // Range-for with init (C++20)
        for (std::span<const float> data{sensor_buffer_.data(), 
             std::min(buffer_index_, sensor_buffer_.size())}; 
             auto val : data) {
            [[maybe_unused]] auto _ = val; // Example of [[maybe_unused]]
        }
        
        if (buffer_index_ == 0) return {0.0f, 0.0f};
        
        std::span<const float> active_buffer{
            sensor_buffer_.data(), 
            std::min(buffer_index_, sensor_buffer_.size())
        };
        
        float min_val = std::numeric_limits<float>::max();
        float max_val = std::numeric_limits<float>::lowest();
        
        for (auto val : active_buffer) {
            min_val = std::min(min_val, val);
            max_val = std::max(max_val, val);
        }
        
        return {min_val, max_val};
    }

    auto get_current_state_id() const -> StateId { return current_id_; }
};

// --- Thread-safe State Machine Manager ---
class StateMachineManager {
private:
    StateMachine state_machine_;
    TemperatureSensor temp_sensor_;
    HumiditySensor humidity_sensor_;
    PressureSensor pressure_sensor_;
    
public:
    auto update() -> void {
        // Process all sensors
        state_machine_.process_sensors(temp_sensor_, humidity_sensor_, pressure_sensor_);
        
        // Get buffer stats using structured binding
        auto [min_val, max_val] = state_machine_.get_buffer_stats();
        
        // Log state with buffer info
        ESP_LOGI("StateMachine", 
            "State: %s | Buffer: %zu samples | Range: [%.1f, %.1f]",
            state_machine_.get_state_info().c_str(),
            std::min(state_machine_.get_buffer_index(), 
                    static_cast<size_t>(10)),
            min_val, max_val);
    }
    
    [[nodiscard]] auto get_state_id() const -> StateId {
        return state_machine_.get_current_state_id();
    }
};

// --- Thread Functions with C++23 Features ---
auto state_monitor_thread([[maybe_unused]] int thread_id) -> void {
    StateMachineManager manager;
    const char* task_name = pcTaskGetName(nullptr);
    
    while (true) {
        // if with initializer
        if (auto state = manager.get_state_id(); state == StateId::ALERT) {
            ESP_LOGW(task_name, "Thread %d: CRITICAL ALERT STATE", thread_id);
        }
        
        manager.update();
        std::this_thread::sleep_for(STATE_UPDATE_INTERVAL);
    }
}

auto sensor_processor_thread() -> void {
    const char* task_name = pcTaskGetName(nullptr);
    std::vector<StateMachineManager> managers(3);
    
    // Range-based for with init - using the manager
    for (size_t i = 0; auto& manager : managers) {
        ESP_LOGI(task_name, "Initialized manager %zu", i++);
        // Actually use the manager to avoid unused variable warning
        manager.update();
    }
    
    while (true) {
        // Process each manager
        for (auto& manager : managers) {
            manager.update();
        }
        
        std::this_thread::sleep_for(1s);
    }
}

// --- Configuration Helper ---
[[nodiscard]] esp_pthread_cfg_t create_config(
    std::string_view name, 
    int core_id, 
    size_t stack_size, 
    int prio, 
    bool inherit = false) 
{
    auto cfg = esp_pthread_get_default_config();
    cfg.thread_name = name.data();
    cfg.pin_to_core = core_id;
    cfg.stack_size = stack_size;
    cfg.prio = prio;
    cfg.inherit_cfg = inherit;
    return cfg;
}

// --- Main Application ---
extern "C" void app_main(void) {
    ESP_LOGI("main", "Starting C++23 State Machine Example");
    
    // Thread 1: State Monitor on Core 0
    []() {
        auto cfg = create_config("StateMon", 0, 4096, 5);
        esp_pthread_set_cfg(&cfg);
        std::jthread monitor([]() { state_monitor_thread(1); });
        monitor.detach();
    }();
    
    // Thread 2: Sensor Processor on Core 1
    []() {
        auto cfg = create_config("SensorProc", 1, 4096, 6);
        esp_pthread_set_cfg(&cfg);
        std::jthread processor(sensor_processor_thread);
        processor.detach();
    }();
    
    // Thread 3: Another State Monitor on Any Core
    []() {
        auto cfg = esp_pthread_get_default_config();
        esp_pthread_set_cfg(&cfg);
        std::jthread monitor([]() { state_monitor_thread(2); });
        monitor.detach();
    }();
    
    // Main loop
    const char* main_task_name = pcTaskGetName(nullptr);
    int cycle = 0;
    
    while (true) {
        ESP_LOGI(main_task_name, 
            "Main task cycle %d | Min free stack: %d bytes",
            ++cycle,
            uxTaskGetStackHighWaterMark(nullptr));
        
        std::this_thread::sleep_for(LOG_INTERVAL);
    }
}