#pragma once

#include <stdlib.h>
#include <string.h>

#ifndef OPUS_CLEAR
#define OPUS_CLEAR(dst, n) (memset((dst), 0, (n) * sizeof(*(dst))))
#endif

#ifndef opus_alloc
#define opus_alloc malloc
#endif

#ifndef opus_free
#define opus_free free
#endif
