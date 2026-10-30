# CherryRed Mac-day — Stage 3 Handoff (2026-10-31)

สถานะ: ทุกอย่างที่ทำได้โดยไม่ต้องใช้รหัสผ่าน sudo เสร็จสมบูรณ์แล้ว
Stage 3 (โหลด kext ผ่าน OpenCore) ต้องใช้ sudo 1 ครั้ง — เตรียมของไว้พร้อมรัน

## ไฟล์ที่เตรียมไว้ (ใน ~/Developer/cherryred-macday-20261030/stage3/)

- `Navi33Bringup.kext` — build สำเร็จบนเครื่องนี้ (x86_64, min macOS 11.0,
  ad-hoc signed, codesign --verify ผ่าน, firmware 14 blobs ฝังใน binary แล้ว)
  - UUID: 6F34DC52-AE44-3656-8562-23B8D9C1CFEF
  - fail-closed: ไม่มี boot-arg `navi33bringup=1` = kext ปฏิเสธ start เอง
  - อ่านอย่างเดียว: PCI config reads เท่านั้น ไม่มี MMIO/write/firmware load
- `stage3_apply.sh` — สคริปต์ติดตั้ง (สำรอง ESP 2 ที่ → คัดลอก kext → แก้
  config.plist เพิ่ม kext + boot-arg → lint ตรวจ → unmount)
- `stage3_verify_after_reboot.sh` — หลังรีบูต รัน (ไม่ต้อง sudo): จับ ioreg
  Navi33Bringup, log, รัน Stage 4 comparison กับ Linux evidence, รีเช็ค Metal
- `0001-*.patch`, `0002-*.patch` — แพตช์ 2 จุดที่ต้องแก้เพื่อให้ build ผ่าน
  (source เดิมไม่เคยถูก compile จริง — ดูรายละเอียดใน WORKLOG)
- `evidence/` — userspace PCI capture + Stage 4 pre-comparison

## ขั้นตอนที่ต้องใช้ sudo (คำสั่งเดียว + รีบูต)

```sh
cd ~/Developer/cherryred-macday-20261030/stage3
sudo sh stage3_apply.sh        # สำรอง ESP อัตโนมัติ → ติดตั้ง → แก้ config
sudo shutdown -r now           # รีบูต (Remote Desktop จะหลุดชั่วคราว)
```

หลังรีบูตกลับมา:

```sh
cd ~/Developer/cherryred-macday-20261030/stage3
sh stage3_verify_after_reboot.sh   # ไม่ต้อง sudo — สร้าง evidence + ตาราง PASS/FAIL
```

## ความเสี่ยงและ rollback

- kext นี้ fail-closed ทั้งดีไซน์: ถ้า OpenCore inject ไม่สำเร็จ → boot ต่อ
  ปกติโดยไม่มี kext (เหลือแค่ log line ใน OpenCore log); ถ้า start ได้ →
  ทำแค่ config-space reads แล้ว publish properties ใต้ node `Navi33Bringup`
- boot-arg เดิม (`-v keepsyms=1 debug=0x100 -radvesa`) ถูกคงไว้ + ต่อท้าย
  display ยังวิ่งแบบ IONDRVFramebuffer เหมือนเดิม ไม่มีอะไรแตะ display path
- ย้อนกลับทุกเมื่อ: `sudo sh stage3_apply.sh --rollback` แล้วรีบูต
- สำรอง ESP ฉบับเต็มอยู่ที่ `stage3/EFI-backup-<timestamp>/` และ USB stick

## สิ่งที่ต้องระวังหลังรีบูต

- ถ้า `ioreg -rn Navi33Bringup` ไม่เจอ → ดู diagnostic ที่สคริปต์พิมพ์ให้
  (kext ถูก inject หรือไม่ / boot-args มี `navi33bringup=1` หรือไม่)
- Metal จะยังไม่มี (exit 2, `MTLCopyAllDevices()` ว่าง) — ถูกต้องตามคาด
  เพราะยังไม่มี IOAccelerator สำหรับ 7480; Stage 5-7 ยังไม่ได้ implement
