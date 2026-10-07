# Zipline Archive Manager

A professional archive creation and extraction tool similar to WinRAR, 7-Zip and
Bandizip, featuring a clean Apple-style interface. **Fully written in C++ (Qt 6)**
— the engine, the UI, and the self-extracting installer runtime are all native,
with no Python or PyInstaller required.

## ✨ Key Features

### Supported Formats
- **ZIP** – Universal compatibility with password protection (AES-256 / AES-128 / ZipCrypto)
- **7Z** – Excellent LZMA2 compression, with real AES-256 encryption
- **EXE** – Self-extracting Windows installers (native SFX, like WinRAR SFX)
- **RAR** – Extraction (reading) supported
- **TAR** – Unix archive format
- **GZIP** – Compressed TAR archives
- Also reads **.bz2** and **.xz**

### Professional Features
- 🎨 **Apple-Style Interface** – clean, modern design (Qt 6 Widgets)
- 🔐 **Password Protection** – AES-256, AES-128, ZipCrypto
- ⚙️ **Advanced Settings** – compression levels, methods, solid archives
- 📦 **Split Archives** – multi-volume output (`.001`, `.002`, …)
- 🔍 **Archive Testing** – verify integrity after creation
- 📊 **Progress Tracking** – real-time compression progress
- 🎯 **Drag & Drop** – easy file addition
- 📝 **Archive Comments**

### 🔥 Advanced EXE Self-Extracting Archives
Create professional self-extracting executables for easy, one-click installers:
- **Custom Icons** – use your company logo (`.ico`)
- **Smart Extraction** – default paths, auto-extract mode, subfolders, overwrite control
- **System Integration** – desktop shortcuts, Start Menu entries, Windows PATH
- **Registry Entries** – Add/Remove Programs (uninstaller) integration
- **Professional UI** – tabbed settings dialog with every option
- **Silent Mode** – `/S` for automated deployment (plus `/D=path`, `/P:password`)
- **Post-Extraction** – run a file, open the folder, create shortcuts
- **Security** – administrator elevation, optional password (AES-256)

The self-extractor is a tiny native Win32 program (no Python runtime bundled),
so the resulting installers are small, fast and dependency-free.

## 🏗️ How it works

```
Zipline (Qt app)                        Self-extracting .exe
 ├── core/archive_manager  ── libarchive (ZIP/TAR/GZIP + all readers)
 │                         ── core/sevenzip_writer  (7z + AES-256)
 │                         ── core/sevenzip_reader  (7z + AES-256 decrypt)
 │                         ── core/sfx_builder ─────┐
 └── ui/*  (Qt 6 Widgets, 1:1 with the original)    │ appends payload to →
                                                     ▼
                             sfx/stub_main.c  →  native Win32 installer stub
                             (LZMA2 + AES-256 decode, extractor GUI,
                              shortcuts, PATH, registry, silent mode)
```

Compression and crypto are provided by the vendored **LZMA SDK** (LZMA2, AES-256,
SHA-256) in `third_party/lzma`; standard formats and all readers use
**libarchive**.

## 🚀 Build & Run

### Dependencies
- A C++17 compiler and CMake ≥ 3.16
- **Qt 6** (Widgets) – the GUI
- **libarchive** – standard formats & readers
- **MinGW-w64** (`x86_64-w64-mingw32-gcc`) – to cross-build the Windows SFX stub
  on Linux. On Windows, MSVC/MinGW builds the stub natively.

On Debian/Ubuntu:
```bash
sudo apt install build-essential cmake qt6-base-dev libarchive-dev \
                 mingw-w64            # mingw only needed to build the SFX stub
```

### Build
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```
This produces `build/zipline` (the GUI app) and `build/sfx_stub.exe`
(the installer stub used for EXE creation).

### Run
```bash
./build/zipline                 # start the GUI application
./build/zipline --test          # run the test suite
./build/zipline --demo-exe      # create a demo self-extracting EXE
./build/zipline --create-files  # create sample test files
```

### Tests
```bash
ctest --test-dir build --output-on-failure
```
The suite round-trips every format (ZIP, ZIP+AES, 7Z, 7Z+AES, TAR, GZIP, EXE,
EXE+AES): create → list → extract → verify content. Encrypted 7z archives are
cross-checked against the reference `7z` tool, and the generated installers are
exercised under Wine (silent extraction, password, registry and shortcuts).

## 📋 Usage Guide

### Creating Archives
1. Click **New Archive** (Ctrl+N).
2. Add files with **Add Files** / **Add Folder**, or drag & drop.
3. Configure settings on the right: format, compression level/method, password,
   split volumes, comment.
4. For **EXE**, click **Advanced EXE Settings…** to configure the installer.
5. Click **Create Archive** and choose where to save.

### Opening / Extracting
1. Click **Open Archive** (Ctrl+O) and pick an archive (including Zipline SFX `.exe`).
2. View the contents.
3. **Extract All** (Ctrl+E) to a folder of your choice, or **Test Archive** (Ctrl+T).

## ⌨️ Keyboard Shortcuts
- `Ctrl+N` New Archive · `Ctrl+O` Open · `Ctrl+A` Add Files · `Ctrl+F` Add Folder
- `Ctrl+E` Extract All · `Ctrl+T` Test Archive · `Del` Remove Selected

## 🖥️ Running the self-extracting installers
The generated `.exe` runs on Windows 7/8/10/11:
```
installer.exe            # interactive extractor wizard
installer.exe /S         # silent extraction
installer.exe /D=C:\App  # override destination
installer.exe /P:secret  # supply a password (for encrypted installers)
```

## 📜 License
Open source. Contributions welcome. Vendored components retain their own licenses
(LZMA SDK — public domain; libarchive — BSD).
