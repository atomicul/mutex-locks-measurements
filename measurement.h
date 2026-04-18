// Define _GNU_SOURCE at the very top to enable RUSAGE_THREAD on Linux
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>

// --- Platform-Specific Headers ---
#if defined(__APPLE__)
    #include <mach/mach_init.h>
    #include <mach/thread_act.h>
    #include <mach/thread_info.h>
#elif defined(__linux__)
    #include <sys/resource.h>
#else
    #error "Platform not supported for thread-specific CPU timing."
#endif
// ---------------------------------

typedef struct {
    int64_t wall_micros;
    int64_t user_micros;
    int64_t sys_micros;
} ThreadBench;

static inline void capture_bench_snapshot(ThreadBench* b) {
    struct timespec ts;
    // CLOCK_MONOTONIC is standard POSIX and works on both modern macOS and Linux
    assert(clock_gettime(CLOCK_MONOTONIC, &ts) == 0);
    b->wall_micros = (int64_t)ts.tv_sec * 1000000LL + (int64_t)ts.tv_nsec / 1000LL;

#if defined(__APPLE__)
    // macOS: Use Mach thread info
    mach_msg_type_number_t count = THREAD_BASIC_INFO_COUNT;
    thread_basic_info_data_t info;
    kern_return_t kr = thread_info(mach_thread_self(), THREAD_BASIC_INFO, (thread_info_t)&info, &count);
    assert(kr == KERN_SUCCESS);

    b->user_micros = (int64_t)info.user_time.seconds * 1000000LL + (int64_t)info.user_time.microseconds;
    b->sys_micros  = (int64_t)info.system_time.seconds * 1000000LL + (int64_t)info.system_time.microseconds;

#elif defined(__linux__)
    // Linux: Use getrusage with RUSAGE_THREAD
    struct rusage ru;
    assert(getrusage(RUSAGE_THREAD, &ru) == 0);

    b->user_micros = (int64_t)ru.ru_utime.tv_sec * 1000000LL + (int64_t)ru.ru_utime.tv_usec;
    b->sys_micros  = (int64_t)ru.ru_stime.tv_sec * 1000000LL + (int64_t)ru.ru_stime.tv_usec;
#endif
}

static inline void bench_begin(ThreadBench* b) {
    capture_bench_snapshot(b);
}

static inline void bench_end(ThreadBench* b) {
    ThreadBench current;
    capture_bench_snapshot(&current);

    b->wall_micros = current.wall_micros - b->wall_micros;
    b->user_micros = current.user_micros - b->user_micros;
    b->sys_micros  = current.sys_micros - b->sys_micros;
}

static inline void bench_print(const ThreadBench* b) {
    int64_t total_cpu_micros = b->user_micros + b->sys_micros;

    double scheduled_pct = 0.0;
    if (b->wall_micros > 0) {
        scheduled_pct = ((double)total_cpu_micros / b->wall_micros) * 100.0;
    }

    double user_pct = 0.0;
    double sys_pct = 0.0;
    if (total_cpu_micros > 0) {
        user_pct = ((double)b->user_micros / total_cpu_micros) * 100.0;
        sys_pct  = ((double)b->sys_micros / total_cpu_micros) * 100.0;
    }

    printf("--- Thread Benchmark Results ---\n");
    printf("Time (Absolute):\n");
    printf("  Wall time:    %10" PRId64 " us (%.3f s)\n", b->wall_micros, b->wall_micros / 1e6);
    printf("  Total CPU:    %10" PRId64 " us (%.3f s)\n", total_cpu_micros, total_cpu_micros / 1e6);
    printf("    User code:  %10" PRId64 " us\n", b->user_micros);
    printf("    Kernel sys: %10" PRId64 " us\n", b->sys_micros);
    
    printf("\nTime (Relative):\n");
    printf("  Scheduled:    %6.2f%% (CPU time / Wall time)\n", scheduled_pct);
    if (total_cpu_micros > 0) {
        printf("    User space: %6.2f%% of scheduled time\n", user_pct);
        printf("    Kernel:     %6.2f%% of scheduled time\n", sys_pct);
    } else {
        printf("    User space:    N/A (No CPU time recorded)\n");
        printf("    Kernel:        N/A (No CPU time recorded)\n");
    }
    printf("--------------------------------\n");
}
