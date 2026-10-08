#include "dd_wrapper/include/sample_manager.hpp"
#include "pyroscope_ffi.h"
#include "sampler.hpp"
#include "span_links.hpp"

#include "echion/echion_sampler.h"
#include "echion/vm.h"

#include <cstddef>
#include <cstdint>
#include <mutex>

extern "C" void
pyroscope_stack_configure(double interval_s, uint32_t max_nframes, uint32_t max_threads)
{
    Datadog::SampleManager::set_max_nframes(max_nframes);
    auto& sampler = Datadog::Sampler::get();
    sampler.set_max_frames(max_nframes);
    sampler.set_max_threads_per_sample(max_threads);
    sampler.set_adaptive_sampling(false);
    sampler.set_interval(interval_s);
}

extern "C" bool
pyroscope_stack_is_safe_copy_failed()
{
#if defined PL_LINUX
    return failed_safe_copy;
#else
    return false;
#endif
}

extern "C" bool
pyroscope_stack_start()
{
    auto& sampler = Datadog::Sampler::get();
    {
        auto& echion = sampler.get_echion();
        const std::lock_guard<std::mutex> guard{ echion.thread_info_map_lock() };
        for (auto& [id, info] : echion.thread_info_map()) {
            (void)info->update_cpu_time();
        }
    }
    return sampler.start();
}

extern "C" void
pyroscope_stack_stop()
{
    auto& sampler = Datadog::Sampler::get();
    sampler.stop();
    sampler.get_echion().renderer().reset_string_cache(); // after the join, before stop_profilers clears the string table
}

extern "C" bool
pyroscope_stack_take_sampling_thread_error() noexcept
{
    return Datadog::Sampler::get().take_sampling_thread_error().has_value();
}

extern "C" void
pyroscope_stack_register_thread(uint64_t id, uint64_t native_id, const char* name)
{
    Datadog::Sampler::get().register_thread(id, native_id, name);
}

extern "C" void
pyroscope_stack_unregister_thread(uint64_t id)
{
    Datadog::Sampler::get().unregister_thread(id);
    Datadog::SpanLinks::get_instance().unlink_span(id);
}

extern "C" size_t
pyroscope_stack_thread_count()
{
    auto& echion = Datadog::Sampler::get().get_echion();
    const std::lock_guard<std::mutex> guard{ echion.thread_info_map_lock() };
    return echion.thread_info_map().size();
}
