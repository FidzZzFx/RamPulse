# RamPulse 1.1 - Advanced RAM Cleaner (Windows 10/11 x64)

Dibuat oleh **FidzZz Try** - [FidzZzFx (FidzZz Try)](https://github.com/FidzZzFx)

## Yang baru di 1.1
- Pilihan bahasa saat pertama kali dibuka, bisa diganti lagi di Pengaturan > Umum > Bahasa. Bahasa bawaan: English.
  English, Bahasa Indonesia, 中文, 日本語, 한국어, Español, Français, Deutsch, Русский, Português.
- Tampilan baru: palet tenang satu warna aksen, panel bergaris tipis, tipografi per bahasa (YaHei UI / Yu Gothic UI / Malgun Gothic untuk CJK).
- Halaman Tentang: penjelasan aplikasi, pembuat, tombol ke profil GitHub, dan donasi lewat QRIS.
- Notifikasi pembersihan otomatis tidak ditampilkan jika interval pembersihan otomatis kurang dari 30 menit.
- Seluruh source tanpa komentar.

## Cara kerja inti
Untuk setiap proses yang boleh dibuka oleh user, `OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_SET_QUOTA)` lalu `EmptyWorkingSet()` (psapi).
Working set diukur dengan `GetProcessMemoryInfo` sebelum dan sesudah, RAM sistem dengan `GlobalMemoryStatusEx`. Proses yang tidak bisa dibuka
dilewati dan alasannya dicatat. Tidak ada proses yang dihentikan, tidak ada konfigurasi sistem yang diubah. Hak `SeDebugPrivilege` dan
`SeIncreaseQuotaPrivilege` diaktifkan hanya jika token memilikinya (mode Administrator).

## Fitur
Dasbor (gauge, RAM total/terpakai/tersedia, proses, CPU, grafik 2 menit, aktivitas terbaru) - Pembersih (manual + rincian hasil, daftar proses
dilewati + alasan) - Riwayat - Pengaturan - Tentang - ikon tray + menu klik kanan - tema gelap/terang - DPI per-monitor.

Auto clean: interval detik/menit/jam (min 5 detik), pemicu RAM >= X%, tunda saat CPU > 80%, jeda + auto-resume.
Peringatan muncul untuk interval < 1 menit.

## Notifikasi
Notifikasi tray hanya muncul setelah pembersihan otomatis jika interval otomatis 30 menit atau lebih. Selama pembersihan otomatis aktif dengan
interval di bawah 30 menit, notifikasi dimatikan (baris Notifikasi di Pengaturan menampilkan keterangannya).

## Donasi
Kode QRIS diunduh sekali saat halaman Tentang pertama dibuka dari
`https://raw.githubusercontent.com/FidzZzFx/pictures/main/qris.jpg` dan disimpan di `%APPDATA%\RamPulse\qris.jpg`.

## Data
`%APPDATA%\RamPulse\settings.ini`, `history.tsv`, `qris.jpg`. Startup: `HKCU\Software\Microsoft\Windows\CurrentVersion\Run` (nilai `RamPulse`,
argumen `--startup`). Argumen lain: `--minimized`, `--selftest` (satu pembersihan tanpa UI, hasil di `%APPDATA%\RamPulse\selftest.txt`).

## Build (Linux, mingw-w64)
`sudo apt install mingw-w64 && ./build.sh` menghasilkan `dist/RamPulse.exe`.
Struktur source: `src/main.cpp` (aplikasi), `src/lang.h` (teks 10 bahasa), `src/app.rc`, `assets/`.
