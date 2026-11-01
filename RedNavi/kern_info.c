#include <mach/mach_types.h>
#include <libkern/libkern.h>

extern kern_return_t _start(kmod_info_t *ki, void *data);
extern kern_return_t _stop(kmod_info_t *ki, void *data);

#ifdef RN_BUILD_NAVI33
__attribute__((visibility("default"))) KMOD_EXPLICIT_DECL(org.rednavi.RedNavi.Navi33, "0.0.1", _start, _stop)
#else
__attribute__((visibility("default"))) KMOD_EXPLICIT_DECL(org.rednavi.RedNavi, "0.0.1", _start, _stop)
#endif
__private_extern__ kmod_start_func_t *_realmain = 0;
__private_extern__ kmod_stop_func_t *_antimain = 0;
__private_extern__ int _kext_apple_cc = __APPLE_CC__;
