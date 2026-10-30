// Minimal kmod glue for the read-only Navi 33 PCI survey prototype.
#include <mach/mach_types.h>
#include <libkern/OSKextLib.h>

extern kern_return_t _start(kmod_info_t *ki, void *data);
extern kern_return_t _stop(kmod_info_t *ki, void *data);

static kern_return_t n33_kmod_start(kmod_info_t *ki, void *data)
{
    (void)ki;
    (void)data;
    return KERN_SUCCESS;
}

static kern_return_t n33_kmod_stop(kmod_info_t *ki, void *data)
{
    (void)ki;
    (void)data;
    return KERN_SUCCESS;
}

KMOD_EXPLICIT_DECL(com.navi33.survey, "0.0.1", _start, _stop)

__private_extern__ kmod_start_func_t *_realmain = n33_kmod_start;
__private_extern__ kmod_stop_func_t *_antimain = n33_kmod_stop;
