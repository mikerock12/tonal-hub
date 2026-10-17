#pragma once
#include <time.h>
/* C11 timespec_get is introduced in Android API 29. Keep API 28 compatibility. */
#if defined(__ANDROID__) && __ANDROID_API__ < 29
#ifndef TIME_UTC
#define TIME_UTC 1
#endif
static inline int tonal_timespec_get(struct timespec* ts, int base) {
    return base == TIME_UTC && clock_gettime(CLOCK_REALTIME, ts) == 0 ? TIME_UTC : 0;
}
#define timespec_get tonal_timespec_get
#endif
