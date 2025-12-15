// cpp_pthread.cpp
#include <iostream>
#include <thread>
#include <chrono>
#include <string>
#include <format>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <esp_pthread.h>

// --- C++23 Goodies ---
using namespace std::chrono_literals;
constexpr auto sleep_duration = 5s;

// Helper to reliably log formatted string using ESP_LOGI
auto print_thread_info(const char *task_name, const std::string_view extra = "") -> void
{
    // C++23: Uses std::format for cleaner string creation than stringstream
    const std::string log_message = std::format(
        "{}{}Core id: {}, prio: {}, min free stack: {} bytes.",
        extra,
        !extra.empty() ? " " : "", // Add space if extra is present
        xPortGetCoreID(),
        uxTaskPriorityGet(nullptr),
        uxTaskGetStackHighWaterMark(nullptr)
    );
    ESP_LOGI(task_name, "%s", log_message.c_str());
}

// --- Thread Functions ---

auto thread_func_inherited() -> void
{
    const char* const name = pcTaskGetName(nullptr);
    while (true) {
        print_thread_info(name, "INHERITING thread (same params/name as parent).");
        std::this_thread::sleep_for(sleep_duration);
    }
}

auto spawn_another_thread() -> void
{
    const char* const name = pcTaskGetName(nullptr);
    
    // C++11/14/17: Still using std::jthread, detached for embedded infinite loop
    std::jthread inherits(thread_func_inherited);
    inherits.detach();

    while (true) {
        print_thread_info(name);
        std::this_thread::sleep_for(sleep_duration);
    }
}

auto thread_func_any_core() -> void
{
    const char* const name = pcTaskGetName(nullptr);
    while (true) {
        print_thread_info(name, "ANY_CORE thread (default config).");
        std::this_thread::sleep_for(sleep_duration);
    }
}

auto thread_func() -> void
{
    const char* const name = pcTaskGetName(nullptr);
    while (true) {
        print_thread_info(name);
        std::this_thread::sleep_for(sleep_duration);
    }
}

// --- Configuration Helper ---

// C++23: [[nodiscard]] attribute ensures the return value (the configuration) is used
[[nodiscard]] esp_pthread_cfg_t create_config(const char *name, const int core_id, const size_t stack_size, const int prio, const bool inherit = false)
{
    auto cfg = esp_pthread_get_default_config();
    // Designated initializers are nice, but not possible when initializing a struct from a function call like get_default_config()
    cfg.thread_name = name;
    cfg.pin_to_core = core_id;
    cfg.stack_size = stack_size;
    cfg.prio = prio;
    cfg.inherit_cfg = inherit;
    return cfg;
}

extern "C" void app_main(void)
{
    // 1. Any Core Thread
    // Use an immediately invoked lambda to set config and create thread cleanly
    []() {
        const auto cfg = esp_pthread_get_default_config();
        esp_pthread_set_cfg(&cfg);
        std::jthread any_core(thread_func_any_core);
        any_core.detach();
    }(); // IILE (Immediately Invoked Lambda Expression) C++11+

    // 2. Core 0 Thread with Inheritance
    []() {
        const auto cfg = create_config(
            "Thread 1",
            0,
            3 * 1024,
            5,
            true // Inherit config
        );
        esp_pthread_set_cfg(&cfg);
        std::jthread thread_1(spawn_another_thread);
        thread_1.detach();
    }();

    // 3. Core 1 Thread
    []() {
        const auto cfg = create_config(
            "Thread 2",
            1,
            3 * 1024,
            5
        );
        esp_pthread_set_cfg(&cfg);
        std::jthread thread_2(thread_func);
        thread_2.detach();
    }();
    
    // 4. Main Task Loop
    const char* const main_task_name = pcTaskGetName(nullptr);
    while (true) {
        print_thread_info(main_task_name, "MAIN_TASK is running.");
        std::this_thread::sleep_for(sleep_duration);
    }
}