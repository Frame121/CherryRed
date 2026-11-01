# Static AMDGPU firmware dependency correlation

- Source C/H files scanned: 2756
- Unique firmware path references: 14

| Firmware path | `MODULE_FIRMWARE` declaration | Local payload candidate | Source references |
|---|---|---|---|
| `amdgpu/dcn_3_2_1_dmcub.bin` | no / macro reference only | dcn_3_2_1_dmcub.bin.zst | display/amdgpu_dm/amdgpu_dm.c |
| `amdgpu/gc_11_0_2_imu.bin` | yes | gc_11_0_2_imu.bin.zst | amdgpu/imu_v11_0.c |
| `amdgpu/gc_11_0_2_me.bin` | yes | gc_11_0_2_me.bin.zst | amdgpu/gfx_v11_0.c |
| `amdgpu/gc_11_0_2_mec.bin` | yes | gc_11_0_2_mec.bin.zst | amdgpu/gfx_v11_0.c |
| `amdgpu/gc_11_0_2_mes.bin` | yes | gc_11_0_2_mes.bin.zst | amdgpu/mes_v11_0.c |
| `amdgpu/gc_11_0_2_mes1.bin` | yes | gc_11_0_2_mes1.bin.zst | amdgpu/mes_v11_0.c |
| `amdgpu/gc_11_0_2_mes_2.bin` | yes | gc_11_0_2_mes_2.bin.zst | amdgpu/mes_v11_0.c |
| `amdgpu/gc_11_0_2_pfp.bin` | yes | gc_11_0_2_pfp.bin.zst | amdgpu/gfx_v11_0.c |
| `amdgpu/gc_11_0_2_rlc.bin` | yes | gc_11_0_2_rlc.bin.zst | amdgpu/gfx_v11_0.c |
| `amdgpu/psp_13_0_7_sos.bin` | yes | psp_13_0_7_sos.bin.zst | amdgpu/psp_v13_0.c |
| `amdgpu/psp_13_0_7_ta.bin` | yes | psp_13_0_7_ta.bin.zst | amdgpu/psp_v13_0.c |
| `amdgpu/sdma_6_0_2.bin` | yes | sdma_6_0_2.bin.zst | amdgpu/sdma_v6_0.c |
| `amdgpu/smu_13_0_7.bin` | yes | smu_13_0_7.bin.zst | pm/swsmu/smu13/smu_v13_0.c |
| `amdgpu/vcn_4_0_4.bin` | no / macro reference only | vcn_4_0_4.bin.zst | amdgpu/amdgpu_vcn.c |

## Interpretation limits

This report correlates static source references with file names found locally. It does not prove the installed kernel binary was built from this source tree, a file was requested or loaded at runtime, the blob is compatible with macOS, or redistribution is permitted. Compressed `.zst` / `.xz` matches are filename matches only.
