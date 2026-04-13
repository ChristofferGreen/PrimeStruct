#pragma once

#if defined(__APPLE__) && (defined(__arm64__) || defined(__aarch64__))
#define PRIMESTRUCT_NATIVE_COLLECTIONS_ENABLED 1
#else
#define PRIMESTRUCT_NATIVE_COLLECTIONS_ENABLED 0
#endif
