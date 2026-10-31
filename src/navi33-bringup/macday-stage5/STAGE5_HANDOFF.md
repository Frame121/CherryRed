# CherryRed Mac-day — Stage 5 Handoff (PSP survey + PSP firmware load)

วันที่: 2026-10-31 — แพ็กเกจนี้เตรียมไว้ให้รันบน FramePC (macOS 26.7.1 + RX 7600)
สิ่งที่เพิ่มใหม่ใน kext (ยังไม่เคยรันบนการ์ดจริง — ทุกขั้นตอน evidence-gated):

- **Stage 5a (อ่านอย่างเดียว)**: `navi33psp=1` — map BAR5 แล้วอ่าน PSP/SMU
  mailbox registers (มีอยู่แล้วในโค้ด, รอเก็บหลักฐานครั้งแรก)
- **Stage 5b (write ครั้งแรก)**: `navi33psp_load=1` — โหลด **KDB** อย่างเดียว
  ผ่าน bootloader mailbox (เล็กสุด, ปลอดภัยสุด)
- **Stage 5c (full chain)**: `navi33psp_chain=1` — KDB → SPL → SYS_DRV → SOS
  แล้วเช็ค sOS sign-of-life (`C2PMSG_81`) + อ่านเวอร์ชัน (`C2PMSG_58`)

รายละเอียดทางเทคนิคทั้งหมด: `src/navi33-bringup/PSP_LOAD_STAGE.md`

## ก่อนเริ่ม (สำคัญมาก)

1. **FramePC ใช้ RX 7600 เป็นจอหลัก (ไม่มี iGPU)** — Stage 5b/5c เขียนการ์ดจริง
   ถ้าเครื่องค้าง ให้เข้าเครื่องผ่าน SSH (Remote Login) หรือ Remote Desktop
   แล้วปิดเครื่อง/รีบูตด้วย EFI สำรอง
2. เตรียม USB EFI **ที่ไม่มี boot-arg เหล่านี้** ไว้กู้เครื่องเสมอ
3. ต้อง build kext ใหม่บน Mac ก่อน (`make` ใน `src/navi33-bringup` พร้อม
   MacKernelSDK + firmware blobs — blobs อยู่ใน `firmware/` แล้ว)
   หรือใช้ artifact จาก CI (`build-kext.yml`, macOS runner)

## ลำดับการรัน (รีบูตทีละโหมด ห้ามข้าม)

### รีบูตที่ 1 — พิสูจน์ Stage 3 ก่อน (kext ตัวเก่าที่ติดตั้งอยู่แล้ว)

```sh
sudo shutdown -r now          # boot-args มี navi33bringup=1 อยู่แล้ว
# กลับมาแล้ว:
sh stage3/stage3_verify_after_reboot.sh
```

- `ioreg -rn Navi33Bringup` ต้องเจอ node (12 ตัวชี้วัดของ Stage 4)
- ถ้ายังไม่เจอ: ดูหัวข้อ "ถ้า kext ยังไม่โหลด" ข้างล่าง

### รีบูตที่ 2 — Stage 5a (อ่านอย่างเดียว)

```sh
sudo sh stage5/stage5_apply.sh survey /path/to/Navi33Bringup.kext
sudo shutdown -r now
# กลับมาแล้ว:
sh stage5/stage5_verify_after_reboot.sh survey
```

ตรวจค่า `PspMailboxRegs` เทียบกับ Linux captures (ใน `RX7600-Linux-Evidence/`):
`MP0_C2PMSG_33/35` ควรมี bit31, `MP0_C2PMSG_81` ควรเป็น 0, `MP1_C2PMSG_90`
ควรเป็น 1 (SMU alive) — ถ้าตรงกับ Navi 32 ของ RedNavi แปลว่า mailbox map
ถูกต้องสำหรับ Navi 33

### รีบูตที่ 3 — Stage 5b (write ครั้งแรก: KDB)

```sh
sudo sh stage5/stage5_apply.sh kdb /path/to/Navi33Bringup.kext
sudo shutdown -r now
# กลับมาแล้ว:
sh stage5/stage5_verify_after_reboot.sh kdb
log show --last 10m --predicate 'eventMessage CONTAINS "PSPLOAD"'
```

PASS = log มี `step 1/1: KDB ... ready=1 errBits=0x00`, ioreg มี
`StepsAccepted=1`, เครื่องไม่ค้าง

### รีบูตที่ 4 — Stage 5c (full chain)

```sh
sudo sh stage5/stage5_apply.sh chain /path/to/Navi33Bringup.kext
sudo shutdown -r now
# กลับมาแล้ว:
sh stage5/stage5_verify_after_reboot.sh chain
```

PASS = `ChainComplete=true`, `SecureOsAlive=true`, `FirmwareVerified=true`
(ขาข้อ firmware ของ compute gate ของ Navi33Accelerator เปิดแล้ว)

## ถ้า kext ยังไม่โหลด (decision tree)

1. `nvram boot-args` ต้องเห็น `navi33bringup=1` — ถ้าไม่เห็น ให้ Reset NVRAM
   จาก OpenCore picker แล้ว set ใหม่ (stage5_apply.sh ใส่ NVRAM Delete ไว้แล้ว)
2. ดู OpenCore log (`Misc > Debug > Target` ต้องมี file+console): หาบรรทัด
   `Prelinked injection` — ถ้ามี `Invalid Parameter` แปลว่า OSBundleLibraries
   ไม่ครบ (build ผิดตัว) → build ใหม่บน Mac ตัวเดิม
3. ทดสอบแยกว่า kext เองโหลดได้ไหม (ไม่ผ่าน OpenCore):
   ```sh
   sudo kextutil -t /Volumes/.../Navi33Bringup.kext   # -t = test เท่านั้น
   ```
   ถ้า kextutil ผ่านแต่ OpenCore ไม่ inject → ปัญหาอยู่ที่ config.plist
   (Kernel/Add entry / folder path) ไม่ใช่ตัว kext

## Rollback

```sh
sudo sh stage5/stage5_apply.sh --rollback   # เอา kext + boot-args ออกหมด
sudo shutdown -r now
```

## สถานะ Metal (ตรงไปตรงมา)

Metal ยังไม่มี — ladder ขณะนี้อยู่ขั้น 3-4/9 (PCI/IP inventory → controlled
hardware init) ตาม `NAVI33_COMPUTE_FIRST_PLAN.md`:
C1 (firmware/PSP) เริ่มรันแล้ว → C2 (VRAM/GART/queue) → C3 (GFX11 compute IB
พิสูจน์ GPU เขียน buffer ได้) → C4 (user client) → C5 (RADV/Vulkan) → C6 (Metal)
ทุกขั้นต้องมีหลักฐาน PASS บนการ์ดจริงก่อนไปขั้นถัดไป
