#include <mach/mach_types.h>
#include <libkern/OSKextLib.h>

extern kern_return_t _start(kmod_info_t* ki, void* data);
extern kern_return_t _stop(kmod_info_t* ki, void* data);

static kern_return_t n33_start(kmod_info_t* ki, void* data) {
    (void)ki;
    (void)data;
    return KERN_SUCCESS;
}

static kern_return_t n33_stop(kmod_info_t* ki, void* data) {
    (void)ki;
    (void)data;
    return KERN_SUCCESS;
}

KMOD_EXPLICIT_DECL(com.cherryred.Navi33Probe, "0.1.0", _start, _stop)
__private_extern__ kmod_start_func_t* _realmain = n33_start;
__private_extern__ kmod_stop_func_t* _antimain = n33_stop;
