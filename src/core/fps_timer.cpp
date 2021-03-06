#include "fps_timer.hpp"
#include <GLFW/glfw3.h>
#include <chrono>
#include <thread>

namespace core {
FPSTimer::DeltaTimer::DeltaTimer(FPSTimer &parent)
    : m_parent(parent), m_start_time(static_cast<float>(glfwGetTime())) {}

FPSTimer::DeltaTimer::~DeltaTimer() {
  m_parent.m_delta_time = static_cast<float>(glfwGetTime()) - m_start_time;
  m_parent._end_frame();
}

FPSTimer::FPSTimer(const size_t max_fps)
    : m_delta_time(0.0f), m_min_delta_time(1.0f / static_cast<float>(max_fps)) {
}

FPSTimer::DeltaTimer FPSTimer::begin_frame() {
  return FPSTimer::DeltaTimer(*this);
}

void FPSTimer::_end_frame() {
  const auto start_time{static_cast<float>(glfwGetTime())};
  if (m_delta_time < m_min_delta_time) {
    const auto sleep_time{m_min_delta_time - m_delta_time};
    // Sleep is unreliable on windows. TODO: Implement vertical sync
#ifdef _WIN32
    while (static_cast<float>(glfwGetTime()) < start_time + sleep_time)
      ;
#else
    std::this_thread::sleep_for(std::chrono::microseconds{
        static_cast<long int>(sleep_time * 1000000.0f)});
#endif
  }
  const auto bonus_time{static_cast<float>(glfwGetTime()) - start_time};
  m_delta_time += bonus_time;
}
} // namespace core
