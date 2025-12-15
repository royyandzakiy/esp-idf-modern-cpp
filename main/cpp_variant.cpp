// cpp_variant.cpp
#include <array>
#include <variant>
#include <span>
#include <string_view>
#include <concepts>
#include <thread>
#include <chrono>
#include <format>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <esp_pthread.h>

//------------------------------------------------------------
// Feature test macros / __has_include
//------------------------------------------------------------
#if __has_include(<span>)
    #define HAS_STD_SPAN 1
#else
    #error "std::span required"
#endif

//------------------------------------------------------------
// Common utilities
//------------------------------------------------------------
using namespace std::chrono_literals;

static constexpr const char* TAG = "FSM";

// Abbreviated function template + concepts
template <std::integral T>
constexpr T clamp_value(T v, T lo, T hi)
{
    if constexpr (std::is_signed_v<T>) {
        return (v < lo) ? lo : (v > hi) ? hi : v;
    } else {
        return (v > hi) ? hi : v;
    }
}

//------------------------------------------------------------
// Events
//------------------------------------------------------------
struct EvInit {};
struct EvTick {};
struct EvError { int code; };

//------------------------------------------------------------
// States
//------------------------------------------------------------
struct Idle {
    [[maybe_unused]] uint32_t counter = 0;
};

struct Running {
    std::span<const int> samples;   // span feature
};

struct Error {
    int code;
};

// Variant-based FSM state
using State = std::variant<Idle, Running, Error>;

//------------------------------------------------------------
// State Machine
//------------------------------------------------------------
class StateMachine {
public:
    explicit StateMachine(std::span<const int> sensor_data)
        : sensor_data_{sensor_data}
    {}

    void dispatch(auto&& event)
    {
        std::visit(
            [this, &event]<typename S>(S& state) {
                handle(state, event);
            },
            state_
        );
    }

private:
    //--------------------------------------------------------
    // Helpers
    //--------------------------------------------------------
    static auto minmax_samples(std::span<const int> s)
    {
        int min = s.front();
        int max = s.front();

        // range-for with init
        for (bool first = true; const int v : s) {
            if (first) {
                first = false;
                continue;
            }
            min = std::min(min, v);
            max = std::max(max, v);
        }
        return std::pair{ min, max }; // structured bindings target
    }

    //--------------------------------------------------------
    // Handlers (overload set)
    //--------------------------------------------------------
    void handle(Idle& s, const EvInit&)
    {
        ESP_LOGI(TAG, "Transition: Idle -> Running");
        state_ = Running{ sensor_data_ };
    }

    void handle(Running& s, const EvTick&)
    {
        // if with initializer + structured bindings
        if (const auto [min, max] = minmax_samples(s.samples); max > 90) {
            ESP_LOGW(TAG, "Sensor overload detected");
            state_ = Error{ max };
        } else {
            ESP_LOGI(TAG, "Running: min=%d max=%d", min, max);
        }
    }

    void handle(Error& e, const EvTick&)
    {
        ESP_LOGE(TAG, "Error state, code=%d", e.code);
    }

    template <typename S, typename E>
    void handle(S&, const E&)
    {
        // default: ignore
    }

private:
    State state_{ Idle{} };
    std::span<const int> sensor_data_;
};

//------------------------------------------------------------
// Thread entry
//------------------------------------------------------------
extern "C" void app_main()
{
    // Sensor buffer (static lifetime, no allocation)
    static constexpr std::array<int, 8> sensor_samples{
        10, 20, 30, 40, 55, 60, 70, 95
    };

    StateMachine fsm{ std::span{ sensor_samples } };

    fsm.dispatch(EvInit{});

    while (true) {
        fsm.dispatch(EvTick{});
        std::this_thread::sleep_for(2s);
    }
}
