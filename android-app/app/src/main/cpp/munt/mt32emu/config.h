/* Manually configured for Android NDK static build */
#ifndef MT32EMU_CONFIG_H
#define MT32EMU_CONFIG_H

#define MT32EMU_VERSION       "2.7.1"
#define MT32EMU_VERSION_MAJOR  2
#define MT32EMU_VERSION_MINOR  7
#define MT32EMU_VERSION_PATCH  1

/* 0 = C++ API only (static lib) */
#define MT32EMU_EXPORTS_TYPE   0

/* Static build: MT32EMU_SHARED is NOT defined */

/* No runtime version tagging for static build */
#define MT32EMU_WITH_VERSION_TAGGING 0
#undef  MT32EMU_RUNTIME_VERSION_CHECK

#endif /* MT32EMU_CONFIG_H */
