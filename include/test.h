/**********************************************************************
  Copyright(c) 2011-2015 Intel Corporation All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in
      the documentation and/or other materials provided with the
      distribution.
    * Neither the name of Intel Corporation nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**********************************************************************/

#ifndef _TEST_H
#define _TEST_H

/**
 *  @file  test.h
 *  @brief Test helper include for common perf and test macros
 *
 *  This is a helper file to enable short and simple tests. Not intended for use
 *  in library functions or production. Includes helper routines for alignment,
 *  benchmark timing, and filesize.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdint.h>

#ifdef _MSC_VER
#define inline __inline
#endif

/* Make os-independent alignment attribute, alloc and free. */
#if defined __unix__ || defined __APPLE__
#define DECLARE_ALIGNED(decl, alignval) decl __attribute__((aligned(alignval)))
#define __forceinline                   static inline
#define aligned_free(x)                 free(x)
#else
#ifdef __MINGW32__
#define DECLARE_ALIGNED(decl, alignval) decl __attribute__((aligned(alignval)))
#define posix_memalign(p, algn, len)                                                               \
        (NULL == (*((char **) (p)) = (void *) _aligned_malloc(len, algn)))
#define aligned_free(x) _aligned_free(x)
#else
#define DECLARE_ALIGNED(decl, alignval) __declspec(align(alignval)) decl
#define posix_memalign(p, algn, len)                                                               \
        (NULL == (*((char **) (p)) = (void *) _aligned_malloc(len, algn)))
#define aligned_free(x) _aligned_free(x)
#endif
#endif

#ifdef DEBUG
#define DEBUG_PRINT(x) printf x
#else
#define DEBUG_PRINT(x)                                                                             \
        do {                                                                                       \
        } while (0)
#endif

/* Decide whether to use benchmark time as an approximation or a minimum. Fewer
 * calls to the timer are required for the approximation case.*/
#define BENCHMARK_MIN_TIME    0
#define BENCHMARK_APPROX_TIME 1
#ifndef BENCHMARK_TYPE
#define BENCHMARK_TYPE BENCHMARK_MIN_TIME
#endif

#ifdef USE_RDTSC
/* The use of rtdsc is nuanced. On many processors it corresponds to a
 * standardized clock source. To obtain a meaningful result it may be
 * necessary to fix the CPU clock to match the rtdsc tick rate.
 */
#include <inttypes.h>
#include <x86intrin.h>
#define USE_CYCLES
#else
#include <time.h>
#define USE_SECONDS
#endif

#ifdef USE_RDTSC
#ifndef BENCHMARK_TIME
#define BENCHMARK_TIME 6
#endif
#define GHZ            1000000000
#define UNIT_SCALE     (GHZ)
#define CALIBRATE_TIME (UNIT_SCALE / 2)
static inline long long
get_time(void)
{
        unsigned int dummy;
        return __rdtscp(&dummy);
}

static inline long long
get_res(void)
{
        return 1;
}
#else
#ifndef BENCHMARK_TIME
#define BENCHMARK_TIME 3
#endif
#ifdef _MSC_VER
#include <windows.h>
#define UNIT_SCALE     get_res()
#define CALIBRATE_TIME (UNIT_SCALE / 4)
static inline long long
get_time(void)
{
        LARGE_INTEGER ret;
        QueryPerformanceCounter(&ret);
        return ret.QuadPart;
}

static inline long long
get_res(void)
{
        LARGE_INTEGER ret;
        QueryPerformanceFrequency(&ret);
        return ret.QuadPart;
}
#else
#define NANO_SCALE     1000000000
#define UNIT_SCALE     NANO_SCALE
#define CALIBRATE_TIME (UNIT_SCALE / 4)
#ifdef __FreeBSD__
#define CLOCK_ID CLOCK_MONOTONIC_PRECISE
#else
#define CLOCK_ID CLOCK_MONOTONIC
#endif

static inline long long
get_time(void)
{
        struct timespec time;
        long long nano_total;
        clock_gettime(CLOCK_ID, &time);
        nano_total = time.tv_sec;
        nano_total *= NANO_SCALE;
        nano_total += time.tv_nsec;
        return nano_total;
}

static inline long long
get_res(void)
{
        struct timespec time;
        long long nano_total;
        clock_getres(CLOCK_ID, &time);
        nano_total = time.tv_sec;
        nano_total *= NANO_SCALE;
        nano_total += time.tv_nsec;
        return nano_total;
}
#endif
#endif
struct perf {
        long long start;
        long long stop;
        long long run_total;
        long long iterations;
};

static inline void
perf_init(struct perf *p)
{
        p->start = 0;
        p->stop = 0;
        p->run_total = 0;
        p->iterations = 0;
}

static inline void
perf_continue(struct perf *p)
{
        p->start = get_time();
}

static inline void
perf_pause(struct perf *p)
{
        p->stop = get_time();
        p->run_total = p->run_total + p->stop - p->start;
        p->start = p->stop;
}

static inline void
perf_start(struct perf *p)
{
        perf_init(p);
        perf_continue(p);
}

static inline void
perf_stop(struct perf *p)
{
        perf_pause(p);
}

static inline double
get_time_elapsed(struct perf *p)
{
        return 1.0 * p->run_total / UNIT_SCALE;
}

static inline long long
get_base_elapsed(struct perf *p)
{
        return p->run_total;
}

static inline unsigned long long
estimate_perf_iterations(struct perf *p, unsigned long long runs, unsigned long long total)
{
        total = total * runs;
        if (get_base_elapsed(p) > 0)
                return (total + get_base_elapsed(p) - 1) / get_base_elapsed(p);
        else
                return (total + get_res() - 1) / get_res();
}

#define CALIBRATE(PERF, FUNC_CALL)                                                                 \
        do {                                                                                       \
                unsigned long long _i, _iter = 1;                                                  \
                perf_start((PERF));                                                                \
                (FUNC_CALL);                                                                       \
                perf_pause((PERF));                                                                \
                                                                                                   \
                while (get_base_elapsed((PERF)) < CALIBRATE_TIME) {                                \
                        _iter = estimate_perf_iterations((PERF), _iter, 2 * CALIBRATE_TIME);       \
                        perf_start((PERF));                                                        \
                        for (_i = 0; _i < _iter; _i++) {                                           \
                                (FUNC_CALL);                                                       \
                        }                                                                          \
                        perf_stop((PERF));                                                         \
                }                                                                                  \
                ((PERF))->iterations = _iter;                                                      \
        } while (0)

#define PERFORMANCE_TEST(PERF, RUN_TIME, FUNC_CALL)                                                \
        do {                                                                                       \
                unsigned long long _i, _iter = ((PERF))->iterations;                               \
                unsigned long long _run_total = (RUN_TIME);                                        \
                _run_total *= UNIT_SCALE;                                                          \
                _iter = estimate_perf_iterations((PERF), _iter, _run_total);                       \
                ((PERF))->iterations = 0;                                                          \
                perf_start((PERF));                                                                \
                for (_i = 0; _i < _iter; _i++) {                                                   \
                        (FUNC_CALL);                                                               \
                }                                                                                  \
                perf_pause((PERF));                                                                \
                ((PERF))->iterations += _iter;                                                     \
                                                                                                   \
                if (get_base_elapsed((PERF)) < _run_total &&                                       \
                    BENCHMARK_TYPE == BENCHMARK_MIN_TIME) {                                        \
                        _iter = estimate_perf_iterations((PERF), _iter,                            \
                                                         _run_total - get_base_elapsed((PERF)) +   \
                                                                 (UNIT_SCALE / 16));               \
                        perf_continue((PERF));                                                     \
                        for (_i = 0; _i < _iter; _i++) {                                           \
                                (FUNC_CALL);                                                       \
                        }                                                                          \
                        perf_pause((PERF));                                                        \
                        ((PERF))->iterations += _iter;                                             \
                }                                                                                  \
        } while (0)

#define BENCHMARK(PERF, RUN_TIME, FUNC_CALL)                                                       \
        do {                                                                                       \
                if ((RUN_TIME) > 0) {                                                              \
                        CALIBRATE((PERF), (FUNC_CALL));                                            \
                        PERFORMANCE_TEST((PERF), (RUN_TIME), (FUNC_CALL));                         \
                                                                                                   \
                } else {                                                                           \
                        ((PERF))->iterations = 1;                                                  \
                        perf_start((PERF));                                                        \
                        (FUNC_CALL);                                                               \
                        perf_stop((PERF));                                                         \
                }                                                                                  \
        } while (0)

#define CALIBRATE_COLD(PERF, SETUP_CODE, FUNC_CALL)                                                \
        do {                                                                                       \
                unsigned long long _i, _iter = 1;                                                  \
                perf_start((PERF));                                                                \
                (SETUP_CODE);                                                                      \
                (FUNC_CALL);                                                                       \
                perf_pause((PERF));                                                                \
                                                                                                   \
                while (get_base_elapsed((PERF)) < CALIBRATE_TIME) {                                \
                        _iter = estimate_perf_iterations((PERF), _iter, 2 * CALIBRATE_TIME);       \
                        perf_start((PERF));                                                        \
                        for (_i = 0; _i < _iter; _i++) {                                           \
                                (SETUP_CODE);                                                      \
                                (FUNC_CALL);                                                       \
                        }                                                                          \
                        perf_stop((PERF));                                                         \
                }                                                                                  \
                ((PERF))->iterations = _iter;                                                      \
        } while (0)

#define PERFORMANCE_TEST_COLD(PERF, RUN_TIME, SETUP_CODE, FUNC_CALL)                               \
        do {                                                                                       \
                unsigned long long _i, _iter = ((PERF))->iterations;                               \
                unsigned long long _run_total = (RUN_TIME);                                        \
                _run_total *= UNIT_SCALE;                                                          \
                _iter = estimate_perf_iterations((PERF), _iter, _run_total);                       \
                ((PERF))->iterations = 0;                                                          \
                perf_start((PERF));                                                                \
                for (_i = 0; _i < _iter; _i++) {                                                   \
                        (SETUP_CODE);                                                              \
                        (FUNC_CALL);                                                               \
                }                                                                                  \
                perf_pause((PERF));                                                                \
                ((PERF))->iterations += _iter;                                                     \
                                                                                                   \
                if (get_base_elapsed((PERF)) < _run_total &&                                       \
                    BENCHMARK_TYPE == BENCHMARK_MIN_TIME) {                                        \
                        _iter = estimate_perf_iterations((PERF), _iter,                            \
                                                         _run_total - get_base_elapsed((PERF)) +   \
                                                                 (UNIT_SCALE / 16));               \
                        perf_continue((PERF));                                                     \
                        for (_i = 0; _i < _iter; _i++) {                                           \
                                (SETUP_CODE);                                                      \
                                (FUNC_CALL);                                                       \
                        }                                                                          \
                        perf_pause((PERF));                                                        \
                        ((PERF))->iterations += _iter;                                             \
                }                                                                                  \
        } while (0)

#define BENCHMARK_COLD(PERF, RUN_TIME, SETUP_CODE, FUNC_CALL)                                      \
        do {                                                                                       \
                if ((RUN_TIME) > 0) {                                                              \
                        CALIBRATE_COLD((PERF), (SETUP_CODE), (FUNC_CALL));                         \
                        PERFORMANCE_TEST_COLD((PERF), (RUN_TIME), (SETUP_CODE), (FUNC_CALL));      \
                                                                                                   \
                } else {                                                                           \
                        ((PERF))->iterations = 1;                                                  \
                        perf_start((PERF));                                                        \
                        (SETUP_CODE);                                                              \
                        (FUNC_CALL);                                                               \
                        perf_stop((PERF));                                                         \
                }                                                                                  \
        } while (0)

#ifdef USE_CYCLES
static inline void
perf_print(struct perf p, long long unit_count)
{
        long long total_units = p.iterations * unit_count;

        printf("runtime = %10lld ticks", get_base_elapsed(&p));
        if (total_units != 0) {
                printf(", bandwidth %lld MB in %.4f GC = %.2f ticks/byte", total_units / (1000000),
                       get_time_elapsed(&p), get_base_elapsed(&p) / (double) total_units);
        }
        printf("\n");
}
#else
static inline void
perf_print(struct perf p, double unit_count)
{
        long long total_units = p.iterations * unit_count;
        long long usecs = (long long) (get_time_elapsed(&p) * 1000000);

        printf("runtime = %10lld usecs", usecs);
        if (total_units != 0) {
                printf(", bandwidth %lld MB in %.4f sec = %.2f MB/s", total_units / (1000000),
                       get_time_elapsed(&p),
                       ((double) total_units) / (1000000 * get_time_elapsed(&p)));
        }
        printf("\n");
}
#endif

static inline uint64_t
get_filesize(FILE *fp)
{
        uint64_t file_size;
        fpos_t pos, pos_curr;

        fgetpos(fp, &pos_curr); /* Save current position */
#if defined(_WIN32) || defined(_WIN64)
        _fseeki64(fp, 0, SEEK_END);
#else
        fseeko(fp, 0, SEEK_END);
#endif
        fgetpos(fp, &pos);
        file_size = *(uint64_t *) &pos;
        fsetpos(fp, &pos_curr); /* Restore position */

        return file_size;
}

#ifdef __cplusplus
}
#endif

#endif // _TEST_H
