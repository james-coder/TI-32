# TI-32

TI-32 is a hardware + firmware + server enhancement for TI-84+ calculators that adds Wi-Fi-backed features.
An ESP32 board connects to the calculator link port, speaks the TI link protocol, and proxies requests to a local
server for GPT, images, and program downloads.

![built pcb](./pcb/built.png)

## Legal and trademark notice

This project is **not** affiliated with, sponsored by, or endorsed by Texas Instruments Incorporated.
“TI”, “TI‑84 Plus”, and related marks are trademarks of Texas Instruments Incorporated; all other
trademarks are the property of their respective owners.

This repository is provided for educational and experimental purposes. Use it at your own risk.
You are responsible for complying with any applicable laws, school or test policies, and device
terms of use. No warranty is provided.

## Project Overview

Core pieces:
- **ESP32 firmware**: `esp32/esp32.ino` handles link protocol, Wi-Fi, and HTTP requests.
- **Calculator launcher**: `programs/LAUNCHER.8xp` is the TI-BASIC UI the user runs.
- **Node server**: `server/` hosts routes for GPT, images, chat, and program downloads.
- **Optional Python server**: `server-py/server.py` is an experimental helper that types into Discord via pyautogui.

## What Is `.mjs`?

`.mjs` files are **Node.js ES modules**. You run them with `node` just like `.js`, but they use the modern `import`
syntax. This repo uses `.mjs` for the server and for build helper scripts.

Examples:
- `node prepareimage.mjs ./test-images/Lenna_(test_image).png`
- `node prepprog.mjs ./programs/LAUNCHER.8xp ./launcher.var`

## Repository Layout (Key Files)

- `esp32/esp32.ino` - ESP32 firmware sketch.
- `esp32/secrets.h` - Wi-Fi + server settings (edit this locally).
- `esp32/launcher.h` - Embedded launcher program (generated).
- `programs/LAUNCHER.8xp` - TI-BASIC launcher program (transfer to calculator).
- `server/index.mjs` - Node/Express server.
- `server/programs/` - Programs served to the calculator.
- `server/images/` - Monochrome image payloads served to the calculator (generated).
- `prepare8xp.mjs` / `prepprog.mjs` - Strip `.8xp` headers and output raw var bytes.
- `prepareimage.mjs` / `image.mjs` - Convert images to TI-84+ monochrome format.
- `preplauncher.sh` - Regenerates `esp32/launcher.h` from the launcher `.8xp`.

## Quick Start

### 1) Configure and build the ESP32 firmware

Prereqs:
- Arduino IDE or Arduino CLI
- ESP32 core installed (esp32:esp32)
- Libraries: **ArTICL** (for `TICL.h`, `CBL2.h`, `TIVar.h`) and **UrlEncode**

Steps:
1. Edit `esp32/secrets.h`:
   - `WIFI_SSID`, `WIFI_PASS`
   - `SERVER` (example: `http://192.168.1.50:8080`)
   - `CHAT_NAME` (short ID shown in chat)
2. Open `esp32/esp32.ino` and select the board (the PCB uses **Seeeduino XIAO ESP32C3**).
   You can purchase the Seeed Studio XIAO ESP32C3 here:
   ```
   https://amzn.to/3NTeTG5
   ```
3. Compile and flash.

#### Flashing a stock Seeeduino XIAO ESP32C3

#### Installing Arduino CLI

macOS/Linux (Homebrew):
```
brew update
brew install arduino-cli
```

macOS/Linux (install script):
```
curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | sh
```
To install into a specific directory:
```
curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | BINDIR=~/local/bin sh
```

Windows:
- Download a prebuilt Arduino CLI binary from the official **Latest release** section and add it to your `PATH`.
- Reference:
  ```
  https://docs.arduino.cc/arduino-cli/installation/#latest-release
  ```
- If you want to use the install script on Windows, run it from Git Bash.

Arduino IDE:
1. Install the ESP32 board package.
2. Select board: `XIAO_ESP32C3`.
3. Select the correct serial port.
4. Click **Upload**.

Arduino CLI:
```
arduino-cli core install esp32:esp32
arduino-cli lib install "UrlEncode"
arduino-cli lib install --git-url https://github.com/KermMartian/ArTICL.git

cp esp32/secrets.h.example esp32/secrets.h
# edit esp32/secrets.h before uploading

arduino-cli compile --fqbn esp32:esp32:XIAO_ESP32C3 esp32
arduino-cli upload -p /dev/ttyACM0 --fqbn esp32:esp32:XIAO_ESP32C3 esp32
```

Optional (smaller firmware, no Bluetooth/mesh):
```
arduino-cli compile --fqbn esp32:esp32:XIAO_ESP32C3 \
  --build-property "compiler.c.elf.libs=@esp32/ld_libs.no_bt_mesh" \
  esp32
```
This uses `esp32/ld_libs.no_bt_mesh` to avoid linking BT/mesh libraries. The weak stubs in
`esp32/no_bt_mesh_stubs.c` satisfy the remaining symbols.

If upload fails, recheck the port and put the board in bootloader mode, then try again.

#### CI build artifacts

The GitHub Action uploads a build artifact named `esp32-build` that contains the compiled binaries.
You can download it from the Actions run summary.

#### Web flasher (Netlify)

This repo includes a static web flasher in `web-flasher/` built on ESP Web Tools. It uses Web Serial
in Chromium-based browsers to flash a stock Seeeduino XIAO ESP32C3.

Quick setup:
1. Put firmware binaries in `web-flasher/firmware/` as:
   - `bootloader.bin`
   - `partitions.bin`
   - `firmware.bin`
2. Confirm `web-flasher/manifest.json` points to those files.
3. Deploy the folder with Netlify (publish directory: `web-flasher`).

Optional helper to build + copy from Arduino CLI output:
```
./scripts/prepare-web-flasher.sh
```

Notes:
- Requires Chrome/Edge with Web Serial.
- To enter bootloader mode: hold **BOOT**, tap **RESET**, then release **BOOT**.

Release update (optional):
- The workflow `.github/workflows/web-flasher-release.yml` builds firmware on GitHub Release publish.
- It commits the latest firmware binaries + manifest version into `web-flasher/`, so Netlify (Git-connected)
  serves the current release without any Netlify credentials.
- The workflow `.github/workflows/release-artifacts.yml` runs on tag push (`v*`) to create/update a GitHub Release
  and attach the firmware binaries.

#### Wi-Fi setup portal (SoftAP)

On boot, the ESP32 tries to connect using saved credentials (from `secrets.h` or prior setup).
If it can't connect, it starts a SoftAP named `TI-32-SETUP-XXXX` and launches a captive portal.
On most Android/iOS phones the portal opens automatically. If not, browse to `http://192.168.4.1`.
The portal lists nearby access points (tap to fill SSID) and lets you enter credentials.

#### Power management (light sleep)

By default the ESP32 enters light sleep after a short idle period and wakes on TI link activity.
To disable for testing, add this to `esp32/secrets.h`:

```
#define POWER_MGMT_ENABLED 0
```

### 2) Run the server (local machine)

Prereqs:
- Node.js + npm
- `OPENAI_API_KEY` for GPT endpoints
- (Optional) ImageMagick for image conversion scripts

Steps:
1. `cd server`
2. `npm install`
3. Create `server/images` if you plan to use the image features:
   - `mkdir -p server/images`
4. Start the server:
   - `OPENAI_API_KEY=... node index.mjs`
   - Optionally set `PORT` (default is 8080).

The server uses the working directory to locate:
- `server/programs/` for downloadable programs
- `server/images/` for image data
- `server/chat.json` for chat history (auto-created)

### 3) Load the launcher on the calculator

Transfer `programs/LAUNCHER.8xp` to the calculator using your preferred TI transfer tool, then run it.

Tip: The launcher can request an update from the ESP32 (`SETTINGS -> UPDATE`), which sends an embedded program
named `TI32` from the ESP32 to the calculator. That embedded program is generated from `programs/LAUNCHER.8xp`.

## Updating the Launcher

If you change `programs/LAUNCHER.8xp`, regenerate the embedded copy and reflash the ESP32:

```
./preplauncher.sh
```

If you edit `programs/LAUNCHER.8xp.txt`, you must first re-tokenize it into a `.8xp` (see **Editing TI-BASIC
programs** below), then run `./preplauncher.sh`.

## Editing TI-BASIC programs (.8xp <-> .txt)

This repo uses **ti-tools** for reliable conversion between `.8xp` and `.txt`.

Install (requires Rust):
```
git clone https://github.com/cqb13/ti-tools.git
cd ti-tools
cargo build --release
# binary: ./target/release/ti-tools
# or add to PATH:
cargo install --path .
```

Examples:
```
ti-tools convert ./programs/LAUNCHER.8xp -o ./programs/LAUNCHER.8xp.txt
ti-tools convert ./programs/LAUNCHER.8xp.txt -o ./programs/LAUNCHER.8xp
```

You can also use mass mode for directories:
```
ti-tools convert ./programs -o ./programs --mass
```

Helper wrapper:
```
./scripts/ti-tools-convert.sh ./programs/LAUNCHER.8xp ./programs/LAUNCHER.8xp.txt
```

Compare two `.8xp` files while ignoring the 42-byte comment field:
```
./scripts/compare-8xp.py ./programs/LAUNCHER.8xp ./programs/LAUNCHER.roundtrip.8xp
```

### Reproducible `.8xp` Round-Trip Verification

To verify that `.8xp -> .txt -> .8xp` round-tripping works (ignoring the 42-byte comment field that some tools
rewrite), use ti-tools with a consistent display/encode pair and then compare.

Recommended settings:
- `--display-mode accessible`
- `--encode-mode max`

Steps:
1. Convert the original `.8xp` to text:
   ```
   ti-tools convert ./programs/LAUNCHER.8xp -o /tmp/LAUNCHER.8xp.txt --display-mode accessible
   ```
2. Convert the text back to `.8xp`:
   ```
   ti-tools convert /tmp/LAUNCHER.8xp.txt -o /tmp/LAUNCHER.roundtrip.8xp --encode-mode max
   ```
3. Compare while ignoring the comment field:
   ```
   ./scripts/compare-8xp.py ./programs/LAUNCHER.8xp /tmp/LAUNCHER.roundtrip.8xp
   ```

Expected output:
```
MATCH (comment bytes ignored)
```

Note: A byte-for-byte match is not expected because the `.8xp` comment field may be rewritten by the converter.

## Generating Images

To add images for the calculator:

1. Make sure ImageMagick is installed and `server/images` exists.
2. Convert a file:
   ```
   node prepareimage.mjs ./test-images/Lenna_(test_image).png
   ```

This writes a monochrome 96x63 payload into `server/images/`.
Use `./genfiles.sh` to bulk-convert everything in `test-images/`.

## Programs Served by the Server

Drop `.8xp` files into `server/programs/`. The server strips headers on-the-fly using `prepare8xp.mjs` and sends
raw program bytes to the calculator.

## Optional: `server-py`

`server-py/server.py` is a small Flask app that uses `pyautogui` to type into Discord. It is not required for
the core TI-32 flow and should be considered experimental.

## Features to be Added

- Change Wi-Fi settings directly from calculator (PENDING TESTING)
- Watchdog when receiving items (PENDING TESTING)
- Support for color images
- Action text during waiting phase
- Support for multi-page response from GPT
- Support for chat history from GPT
- Support for bigger menu (320x240 resolution only)
- Support for lowercase text
- Basic Web Browsing
- HTTPS Encryption
- Email Send and Read
- Discord Access
- Get local weather
- Control computer wirelessly
- QR Code & Barcode scanner
- Video player

## Bug Fixes

- Images don't work
- GPT Menu closes immediately when receiving response
- App transfer fails

## Video
[![YouTube](http://i.ytimg.com/vi/Bicjxl4EcJg/hqdefault.jpg)](https://www.youtube.com/watch?v=Bicjxl4EcJg)
