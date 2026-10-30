#include <metal_stdlib>
using namespace metal;

// Minimal end-to-end Metal compute probe. This is a generic Metal kernel;
// it does not imply RX 7600/Navi 33 driver support.
kernel void cherryred_vector_add(
    device const float* lhs       [[buffer(0)]],
    device const float* rhs       [[buffer(1)]],
    device float*       output    [[buffer(2)]],
    constant uint&      elementCount [[buffer(3)]],
    uint index [[thread_position_in_grid]])
{
    if (index >= elementCount) return;
    output[index] = lhs[index] + rhs[index];
}
