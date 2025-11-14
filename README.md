# 🧠 ESP32 Brain Wave Monitor

> A real-time, local-first EEG monitoring system built with ESP32, featuring AI-powered analysis and web-based visualization - all without cloud dependencies.

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-ESP32-green.svg)](https://www.espressif.com/en/products/socs/esp32)
[![Python](https://img.shields.io/badge/python-3.x-yellow.svg)](https://www.python.org/)

---

## 📋 Table of Contents

- [Overview](#-overview)
- [Features](#-features)
- [Hardware Requirements](#-hardware-requirements)
- [Software Requirements](#-software-requirements)
- [System Architecture](#-system-architecture)
- [Installation](#-installation)
- [Hardware Setup](#-hardware-setup)
- [Configuration](#-configuration)
- [Usage](#-usage)
- [Code Structure](#-code-structure)
- [API Endpoints](#-api-endpoints)
- [Troubleshooting](#-troubleshooting)
- [Project Details](#-project-details)
- [Contributing](#-contributing)
- [Contact](#-contact)

---

## 🎯 Overview

This project creates a complete EEG brainwave monitoring system that runs entirely on local hardware. It captures brain signals from a consumer EEG headset, processes them on an ESP32 microcontroller, and displays real-time metrics through a responsive web interface accessible from any device on your local network.

**Key Highlights:**
- ✅ **Privacy-First**: All data stays on your local network
- ✅ **Real-Time**: Live visualization with <100ms latency
- ✅ **AI-Powered**: Gemini API integration for intelligent analysis
- ✅ **Mobile-Ready**: Responsive web interface works on phones, tablets, and desktops
- ✅ **Data Logging**: Optional SD card storage for historical analysis
- ✅ **Budget-Friendly**: Total cost under R2,400 (~$140 USD)

---

## ✨ Features

### Real-Time Monitoring
- **Attention & Meditation Metrics**: 0-100 scale tracking
- **Signal Quality Indicator**: Real-time connection status (0-200 scale)
- **8 EEG Frequency Bands**:
  - Delta (0.5-4 Hz) - Deep sleep patterns
  - Theta (4-8 Hz) - Relaxation, meditation
  - Low Alpha (8-10 Hz) - Wakeful relaxation
  - High Alpha (10-12 Hz) - Alert relaxation
  - Low Beta (12-18 Hz) - Active thinking
  - High Beta (18-30 Hz) - Intense focus
  - Low Gamma (30-40 Hz) - High-level processing
  - High Gamma (40-100 Hz) - Peak cognitive activity

### Mental State Analysis
- Automatic state classification: High Focus, Very Relaxed, Balanced, Neutral, etc.
- Trend analysis with 10-sample history buffer
- Signal confidence scoring (High/Medium/Low)

### Web Dashboard
- **Main Monitor**: Live charts with Chart.js visualization
- **AI Chat Interface**: Interactive analysis with Google Gemini
- **Mobile-Optimized**: Clamp-based responsive design
- **Dark/Light Theme**: Modern gradient UI

### Data Management
- Hex packet logging and debugging
- CSV export to SD card (timestamp, all metrics, mental state)
- Packet statistics (total, valid, success rate)
- Data reset functionality

---

## 🔧 Hardware Requirements

### Essential Components

| Component | Specification | Cost (Approx) |
|-----------|--------------|---------------|
| **ESP32 Development Board** | ESP32-WROOM-32 or similar | R150 - R250 |
| **EEG Headset** | ThinkGear-compatible single-electrode | R1,800 - R2,000 |
| **USB Receiver** | Comes with headset (wireless dongle) | Included |
| **Micro USB Cable** | For ESP32 programming/power | R30 - R50 |
| **SD Card Module** (Optional) | SPI interface, any capacity | R40 - R80 |
| **Power Supply** | 5V USB or battery pack | R50 - R100 |

### Connections

```
EEG Headset (Transmitter)
    ↓ [2.4GHz Wireless]
USB Receiver Dongle → PC COM Port (e.g., COM9)
    ↓ [Python Bridge via WiFi]
ESP32 Board (192.168.x.x)
    ↓ [HTTP Server]
Web Browser (Phone/Laptop/Tablet)
```

### Pin Configuration (Optional SD Card)

```cpp
SD_CS Pin: GPIO 5
SPI MOSI: GPIO 23
SPI MISO: GPIO 19
SPI SCK: GPIO 18
```

---

## 💻 Software Requirements

### Development Environment

- **Arduino IDE** 1.8.19+ or 2.x
  - ESP32 Board Manager v2.0.14 (NOT newer - avoid LEDC errors)
  - Libraries: WiFi, WebServer, SD, SPI, ArduinoJson
  
- **Python** 3.7+
  - `pyserial` library
  - `requests` library

- **USB Drivers**
  - Silicon Labs CP210x USB to UART Bridge

### Browser Compatibility

- ✅ Chrome/Edge 90+
- ✅ Firefox 88+
- ✅ Safari 14+
- ✅ Mobile browsers (iOS/Android)

---

## 🏗️ System Architecture

### Data Flow

```
┌─────────────────┐
│  EEG Headset    │
│  (Transmitter)  │
└────────┬────────┘
         │ 2.4GHz Wireless
         ↓
┌─────────────────┐
│  USB Receiver   │ ←→ PC USB Port (COM9)
└────────┬────────┘
         │ Serial (115200 baud)
         ↓
┌─────────────────┐
│ Python Bridge   │ ←→ Reads hex data, formats packets
└────────┬────────┘
         │ HTTP POST to /push
         ↓
┌─────────────────┐
│    ESP32        │ ←→ WiFi AP (192.168.137.x)
│  Web Server     │     Parses ThinkGear protocol
└────────┬────────┘     Analyzes mental states
         │
         ├─→ [Chart.js] Real-time graphs
         ├─→ [Gemini API] AI analysis
         └─→ [SD Card] CSV logging
```

### Python Bridge Script

The bridge script acts as a translator between the serial COM port and ESP32's WiFi interface:

```python
import serial
import requests
import time

ESP32_IP = "192.168.137.75"  # Your ESP32 IP address
COM_PORT = "COM9"             # USB receiver port
BAUD_RATE = 115200            # Match ESP32 serial monitor

print("Connecting to COM9...")
ser = serial.Serial(COM_PORT, BAUD_RATE, timeout=1)
print(f"Real bridge: COM9 -> http://{ESP32_IP}")

while True:
    if ser.in_waiting:
        # Read raw bytes from headset receiver
        data = ser.read(ser.in_waiting)
        
        # Convert to hex string format
        hex_data = ' '.join(f'{b:02x}' for b in data)
        
        try:
            # POST hex data to ESP32
            requests.post(f"http://{ESP32_IP}/push", 
                         data={'data': hex_data}, 
                         timeout=0.5)
            print(f"Sent {len(data)} bytes", end='\r')
        except Exception as e:
            print(f"Error: {e}")
    
    time.sleep(0.01)  # 10ms polling interval
```

**How it works:**
1. Opens serial connection to USB receiver at 115200 baud
2. Continuously polls for incoming data (10ms interval)
3. Converts raw bytes to hexadecimal string format
4. POSTs hex data to ESP32's `/push` endpoint via HTTP
5. Handles timeouts and connection errors gracefully

---

## 📦 Installation

### Step 1: Install Arduino IDE & Drivers

#### Windows

1. **Download Arduino IDE**:
   ```
   https://www.arduino.cc/en/software
   ```
   Choose: `Windows Win 10 and newer, 64 bits`

2. **Install Silicon Labs Driver**:
   - Download from: https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers
   - Extract ZIP file
   - Run `CP210xVCPInstaller_x64.exe`
   - Restart PC

3. **Verify Installation**:
   - Connect ESP32 via USB
   - Open Device Manager (`Win + X` → Device Manager)
   - Check `Ports (COM & LPT)` for "Silicon Labs CP210x USB to UART Bridge"

#### Alternative: Microsoft Store

```
Search: "Arduino IDE"
Install: Arduino IDE by Arduino LLC
```

### Step 2: Configure Arduino IDE for ESP32

1. **Add ESP32 Board Manager URL**:
   - Open Arduino IDE
   - `File` → `Preferences`
   - In "Additional Board Manager URLs", add:
     ```
     https://espressif.github.io/arduino-esp32/package_esp32_index.json
     ```

2. **Install ESP32 Board Package**:
   - `Tools` → `Board` → `Boards Manager`
   - Search: `esp32`
   - Install: `esp32 by Espressif Systems` **v2.0.14** (NOT latest!)
   - ⚠️ **Important**: Newer versions (v3.x) cause LEDC motor driver errors

3. **Select Board & Port**:
   - `Tools` → `Board` → `ESP32 Arduino` → `ESP32 Dev Module`
   - `Tools` → `Port` → Select your COM port (e.g., COM11)
   - `Tools` → `Partition Scheme` → `Huge APP (3MB No OTA/1MB SPIFFS)`

### Step 3: Install Required Libraries

```
Tools → Manage Libraries
```

Search and install:
- ✅ **ArduinoJson** (v6.x)
- ✅ **SD** (Built-in)
- ✅ **SPI** (Built-in)
- ✅ **WiFi** (Built-in)
- ✅ **WebServer** (Built-in)

### Step 4: Install Python & Libraries

1. **Download Python**:
   ```
   https://www.python.org/downloads/
   ```
   ✅ Check "Add Python to PATH" during installation

2. **Verify Installation**:
   ```bash
   python --version
   ```

3. **Install Required Libraries**:
   ```bash
   pip install pyserial requests
   ```

---

## 🔌 Hardware Setup

### 1. EEG Headset Setup

```
┌───────────────────────────┐
│   EEG Headset Assembly    │
├───────────────────────────┤
│  1. Sensor Pad            │ ←─ Forehead placement
│  2. Ear Clip              │ ←─ Reference electrode
│  3. On/Off Switch         │ ←─ Power control
│  4. Micro USB Port        │ ←─ Charging only
│  5. LED Indicator         │ ←─ Status (see below)
└───────────────────────────┘
```

**LED Status Indicators:**
- 🔴 **Blinking Red**: Searching for receiver
- 🔵 **Solid Blue**: Connected to receiver
- 🟢 **Green Flash**: Good signal quality

**Wearing the Headset:**
1. Position sensor pad on center of forehead (just above eyebrows)
2. Clip reference electrode to earlobe (either ear)
3. Ensure good skin contact (clean skin, no hair interference)
4. Switch ON - wait for solid blue LED on USB receiver

### 2. USB Receiver Connection

```
USB Receiver → PC USB Port → Device Manager
```

**Verify Connection (Windows)**:
1. Right-click Start → `Device Manager`
2. Expand `Ports (COM & LPT)`
3. Look for: `Silicon Labs CP210x USB to UART Bridge (COMx)`
4. Note the COM port number (e.g., COM9)

**LED Indicators:**
- 🔴 **Red Only**: Receiver powered, no headset connection
- 🔵🔴 **Blue + Red**: Receiving data from headset

### 3. ESP32 Connection

```
ESP32 → Micro USB Cable → PC USB Port
```

**Verify Connection**:
- Device Manager should show: `COM11` (or similar)
- Arduino IDE → `Tools` → `Port` → Select the COM port
- LED on ESP32 board should light up

### 4. WiFi Network Setup

**Create Hotspot (Windows 10/11)**:
1. `Settings` → `Network & Internet` → `Mobile Hotspot`
2. Configure:
   ```
   Network Name: ESP32SEG
   Password: letmeinplease
   Band: 2.4 GHz (Important!)
   ```
3. Turn ON hotspot

**⚠️ Critical**: ESP32 only supports 2.4 GHz WiFi (not 5 GHz)

---

## ⚙️ Configuration

### 1. Update Arduino Code WiFi Credentials

Open the main `.ino` file and modify:

```cpp
const char* ssid = "ESP32SEG";         // Your hotspot name
const char* password = "letmeinplease"; // Your hotspot password
```

### 2. Get Gemini API Key

1. Visit: https://aistudio.google.com/
2. Sign in with Google account
3. Click `Get API Key` → `Create API key`
4. Create new project or select existing
5. Copy the generated key

**Update in Code** (Line ~700):
```cpp
let geminiApiKey='YOUR_API_KEY_HERE';
```

### 3. Upload Code to ESP32

1. Connect ESP32 via USB
2. Select correct port: `Tools` → `Port` → `COM11`
3. Click `Upload` button (→)
4. Wait for "Done uploading" message

**Open Serial Monitor**:
- `Tools` → `Serial Monitor`
- Set baud rate: `115200`
- You should see:
  ```
  ESP32 Brain Wave Monitor Starting...
  Connecting to WiFi.....
  WiFi connected!
  IP Address: 192.168.137.75
  Web server started
  ```

**⚠️ Important**: Note the IP address! You'll need it for Python script.

### 4. Configure Python Bridge Script

Update `bridge.py`:

```python
ESP32_IP = "192.168.137.75"  # ← Replace with YOUR ESP32 IP
COM_PORT = "COM9"             # ← Replace with YOUR receiver port
BAUD_RATE = 115200            # ← Keep this
```

---

## 🚀 Usage

### Starting the System

#### 1. Power On Hardware
```
1. Connect USB receiver to PC → Red LED
2. Switch ON headset → Blue LED on receiver
3. Connect ESP32 to PC → Upload code
4. Enable hotspot on PC
```

#### 2. Run Python Bridge
```bash
# Navigate to script directory
cd /path/to/project

# Run bridge
python bridge.py
```

**Expected Output**:
```
Connecting to COM9...
Real bridge: COM9 -> http://192.168.137.75
Sent 36 bytes
```

#### 3. Access Web Dashboard

**On PC** (same machine):
```
http://192.168.137.75
```

**On Mobile Device**:
1. Connect phone to `ESP32SEG` WiFi
2. Enter password: `letmeinplease`
3. Open browser → `http://192.168.137.75`

### Dashboard Features

#### Main Monitor (`/`)

- **Mental State Card**:
  - Primary state classification
  - Secondary analysis notes
  - Attention/Meditation trends
  - Signal confidence

- **Live Metrics**:
  - Signal Quality (0-200, lower is better)
  - Attention (0-100)
  - Meditation (0-100)
  - Packet statistics

- **Real-Time Chart**:
  - Dual-line graph (Attention in purple, Meditation in green)
  - 30-second rolling window
  - Auto-updating every 2 seconds

- **EEG Frequency Bands**:
  - 8 bands with live values
  - Formatted display (K for thousands, M for millions)

#### AI Chat Interface (`/chat`)

- **Left Panel**: Live metrics sidebar
- **Right Panel**: Chat interface with Gemini AI

**Features**:
- 🔊 Text-to-Speech for responses
- 📋 Copy to clipboard
- 🤖 Context-aware analysis (knows your current readings)
- ⚡ Fast response times (~1-2 seconds)

**Example Queries**:
```
"What do my current readings mean?"
"Am I focused right now?"
"How can I improve my meditation score?"
"What is the delta wave indicating?"
```

**⚠️ Important**: The AI provides scientifically honest analysis and clearly states limitations of consumer EEG devices.

---

## 📂 Code Structure

### ESP32 Firmware (`main.ino`)

```
├── Configuration
│   ├── WiFi credentials
│   ├── SD card CS pin
│   └── Server port (80)
│
├── Data Structures
│   ├── BrainData (current metrics)
│   ├── MentalState (analysis results)
│   └── History buffers (10 samples)
│
├── Setup Functions
│   ├── Serial initialization (115200)
│   ├── SD card setup
│   ├── WiFi connection
│   └── Web server routes
│
├── Main Loop
│   ├── Handle HTTP requests
│   ├── Analyze mental state (5s interval)
│   ├── Log data (10s interval)
│   └── Check data timeout (30s)
│
├── Data Processing
│   ├── parseBrainWaveData() → Main parser
│   ├── parseThinkGearPacket() → Protocol parser
│   ├── parseLooseFormat() → Fallback parser
│   └── generateRealisticVariation() → Smooth transitions
│
├── Analysis Functions
│   ├── analyzeMentalState() → State classification
│   ├── updateHistory() → Rolling buffer
│   └── calculateVariance() → Stability metric
│
└── HTTP Handlers
    ├── handleRoot() → Main dashboard HTML
    ├── handleChat() → AI chat interface HTML
    ├── handlePush() → Receive data from Python
    ├── handleValues() → JSON metrics API
    ├── handleMentalState() → JSON state API
    ├── handleDebug() → Debug info
    ├── handleStatus() → System status
    ├── handleReset() → Clear all data
    └── handleDownload() → CSV export
```

### Key Functions Explained

#### `parseBrainWaveData(String hexData)`

Parses incoming hex string from Python bridge:

1. **Sync Detection**: Looks for `AAAA` sync bytes
2. **Packet Length**: Reads length byte
3. **Payload Parsing**:
   - `0x02` → Signal Quality
   - `0x04` → Attention
   - `0x05` → Meditation
   - `0x80` → Raw Wave (16-bit signed)
   - `0x83` → ASIC_EEG_POWER (8 bands × 3 bytes)

**ThinkGear Packet Structure**:
```
[AA AA] [LEN] [PAYLOAD...] [CHECKSUM]
  Sync   Length  Data bytes   Verify
```

#### `analyzeMentalState()`

Classifies mental state based on metrics:

```cpp
if (avgAttention > 70 && avgMeditation < 30) {
    state = "High Focus";
} else if (avgMeditation > 70 && avgAttention < 30) {
    state = "Very Relaxed";
} else if (variance > 300) {
    state = "Variable";
}
// ... more conditions
```

**Confidence Calculation**:
- Signal Quality ≤ 30 + Low Variance → High Confidence
- Signal Quality ≤ 60 → Medium Confidence
- Signal Quality > 60 → Low Confidence

### Python Bridge Script

```python
# Key Components:

1. Serial Connection
   - Opens COM port at 115200 baud
   - 1-second timeout for safety
   
2. Data Polling Loop
   - Checks for incoming data every 10ms
   - Non-blocking read operation
   
3. Hex Conversion
   - Converts bytes to hex format: '3a 4f 2c'
   - Preserves packet boundaries
   
4. HTTP POST
   - Sends data to ESP32 /push endpoint
   - 500ms timeout to prevent blocking
   - Error handling for network issues
```

---

## 🌐 API Endpoints

### Data Endpoints

| Endpoint | Method | Description | Response |
|----------|--------|-------------|----------|
| `/` | GET | Main dashboard HTML | HTML page |
| `/chat` | GET | AI chat interface HTML | HTML page |
| `/push` | POST | Receive hex data from Python | `OK` or `No data` |
| `/data` | GET | Raw data buffer (last 500 chars) | Plain text |
| `/values` | GET | Current metrics JSON | JSON object |
| `/mental-state` | GET | Mental state analysis JSON | JSON object |
| `/debug` | GET | Debug information | Plain text |
| `/api/status` | GET | System status JSON | JSON object |
| `/api/reset` | POST | Reset all data | Plain text |
| `/download` | GET | Download CSV log | CSV file |

### JSON Response Examples

#### `/values` Response:
```json
{
  "quality": 45,
  "attention": 67,
  "meditation": 54,
  "delta": 125430,
  "theta": 87620,
  "lowAlpha": 45210,
  "highAlpha": 32100,
  "lowBeta": 23450,
  "highBeta": 15600,
  "lowGamma": 8700,
  "highGamma": 4300,
  "packets": 1523,
  "validPackets": 1456,
  "uptime": 456780,
  "memory": 234560,
  "dataValid": true
}
```

#### `/mental-state` Response:
```json
{
  "primaryState": "High Focus",
  "secondaryState": "Active concentration pattern",
  "attentionTrend": 68,
  "relaxationTrend": 45,
  "signalConfidence": "High"
}
```

---

## 🔍 Troubleshooting

### ESP32 Won't Connect to WiFi

**Symptoms**: Serial monitor shows endless dots `........`

**Solutions**:
1. ✅ Verify hotspot is ON and 2.4 GHz
2. ✅ Check SSID and password in code (case-sensitive!)
3. ✅ Move ESP32 closer to router/hotspot
4. ✅ Restart ESP32 (`Tools` → `Reset` or unplug/replug)
5. ✅ Try different WiFi channel in hotspot settings

### Python Bridge Not Connecting

**Symptoms**: `ConnectionRefusedError` or `Timeout`

**Solutions**:
1. ✅ Verify ESP32 IP address (check Serial Monitor)
2. ✅ Update `ESP32_IP` in Python script
3. ✅ Ensure PC is connected to same hotspot
4. ✅ Ping ESP32: `ping 192.168.137.75`
5. ✅ Check firewall isn't blocking Python

### No Data on Dashboard

**Symptoms**: "Waiting for data..." or `--` values

**Solutions**:
1. ✅ Verify headset is ON (blue LED on receiver)
2. ✅ Check Python bridge is running
3. ✅ Verify COM port in script matches Device Manager
4. ✅ Ensure good headset-to-skin contact
5. ✅ Check Serial Monitor for "Sent X bytes"
6. ✅ Try `/debug` endpoint for diagnostics

### Signal Quality Always Poor

**Symptoms**: Quality > 100, "Poor Signal" state

**Solutions**:
1. ✅ Clean forehead with alcohol wipe
2. ✅ Adjust headset position (centered on forehead)
3. ✅ Ensure ear clip has good contact
4. ✅ Remove hair from sensor contact points
5. ✅ Replace headset battery if old

### Arduino Upload Fails

**Error**: `A fatal error occurred: Failed to connect`

**Solutions**:
1. ✅ Hold `BOOT` button on ESP32 during upload
2. ✅ Check USB cable (must be data cable, not charge-only)
3. ✅ Select correct COM port
4. ✅ Try different USB port on PC
5. ✅ Update ESP32 driver (Silicon Labs CP210x)

### LEDC Motor Driver Error

**Error**: `error: 'LEDC_TIMER_13_BIT' was not declared`

**Solution**:
```
Tools → Boards Manager → Search "esp32"
Uninstall current version
Install version 2.0.14 (NOT latest)
```

### SD Card Not Detected

**Symptoms**: "SD card not available" message

**Solutions**:
1. ✅ Check SD card is formatted (FAT32)
2. ✅ Verify wiring: CS→GPIO5, MOSI→GPIO23, MISO→GPIO19, SCK→GPIO18
3. ✅ Ensure SD module has power (3.3V or 5V depending on module)
4. ✅ Try different SD card (some cards incompatible)

---

## 📊 Project Details

### Budget Breakdown

Total Cost: **R2,400** (~$140 USD)

| Item | Cost | Notes |
|------|------|-------|
| EEG Headset Kit | R1,800 - R2,000 | Includes transmitter + receiver |
| ESP32 Board | R150 - R250 | DevKit v1 or similar |
| Cables & Accessories | R80 - R150 | USB cables, jumper wires |

**Comparison to Commercial Systems:**
- Consumer EEG: R2,000 - R5,000 (limited features)
- Professional EEG: R10,000+ (lab-grade)
- **This Project: R2,400** (75% cost reduction!)

### Performance Metrics

Achieved from original project requirements:

- ✅ **Accuracy**: 82% mental state classification (target: 80%)
- ✅ **Latency**: 85ms average (target: <100ms)
- ✅ **Budget**: R2,400 (target: <R2,500)
- ✅ **Success Rate**: Typically >95% valid packets

### Technical Specifications

**ESP32 Specs:**
- CPU: Dual-core 240 MHz Xtensa LX6
- RAM: 520 KB SRAM
- Flash: 4 MB
- WiFi: 802.11 b/g/n (2.4 GHz only)
- GPIO: 34 pins

**Headset Specs:**
- Protocol: NeuroSky ThinkGear
- Sampling Rate: 512 Hz (raw wave)
- Frequency Response: 3-100 Hz
- Signal Quality: 0-200 (0 = best)
- Power: Rechargeable battery (USB charging)

### Data Flow Performance

```
Headset → Receiver: ~57,600 baud (wireless)
Receiver → PC: 115200 baud (USB serial)
Python Bridge: 10ms polling, <1ms processing
Bridge → ESP32: HTTP POST, ~20-50ms network latency
ESP32 Processing: <5ms parse + analysis
Dashboard Update: 2000ms interval (configurable)
Total Latency: ~85ms average
```

---

## 🤝 Contributing

Contributions are welcome! This project is educational and open-source.

### Areas for Improvement

- [ ] Multi-headset support
- [ ] Bluetooth connectivity (eliminate Python bridge)
- [ ] Advanced signal processing (filters, FFT)
- [ ] Machine learning state classification
- [ ] Mobile app (native iOS/Android)
- [ ] Cloud sync option (optional, privacy-respecting)
- [ ] Multi-language support

### How to Contribute

1. Fork the repository
2. Create feature branch (`git checkout -b feature/AmazingFeature`)
3. Commit changes (`git commit -m 'Add AmazingFeature'`)
4. Push to branch (`git push origin feature/AmazingFeature`)
5. Open Pull Request

---

## 📞 Contact

**Project Creator: GM Kabamba**
- 🎓 Cape Peninsula University of Technology

**Social Media:**
- 💬 Discord: `hnk0422_76455`
- 🐦 Twitter: [@HnkHorizon](https://twitter.com/HnkHorizon)
- 🎵 TikTok: [@codingfever](https://tiktok.com/@codingfever)
- 🎥 YouTube: [@HNK2005](https://youtube.com/@HNK2005)
- 📷 Instagram: [@hhnk.3693](https://instagram.com/hhnk.3693)

---

## 📄 License

This project is open-source and available for educational purposes.

**Educational Use**: ✅ Freely modify and learn
**Commercial Use**: ⚠️ Contact creator for licensing
**Attribution**: 🙏 Please credit GM Kabamba if you use this work

---

## ⚠️ Important Disclaimers

### Medical Disclaimer

> **This device is NOT a medical device.** It is an educational project demonstrating EEG signal processing. Do not use for medical diagnosis, treatment decisions, or health monitoring without professional medical oversight.

### Accuracy Limitations

Consumer EEG headsets have significant limitations:
- ❌ Cannot detect specific emotions (happiness, sadness, anger, etc.)
- ❌ Cannot diagnose medical/psychological conditions
- ❌ Cannot read thoughts or intentions
- ❌ Cannot determine physical states (hunger, pain, illness)
- ✅ Can measure general activity patterns (focus, relaxation)
- ✅ Can track relative changes over time
- ✅ Suitable for biofeedback and meditation training

### Privacy Notice

This system is designed to be **local-first** and **privacy-respecting**:
- ✅ All data stays on your local network
- ✅ No cloud uploads required
- ✅ Optional SD card logging (under your control)
- ⚠️ Gemini AI chat sends data to Google (only when you use chat feature)
- ✅ No user accounts, tracking, or telemetry

---

## 🎓 Learning Resources

### Understanding EEG

- **What is EEG?** Electroencephalography measures electrical activity in the brain
- **How it works:** Neurons communicate via electrical signals; EEG detects these through scalp electrodes
- **Frequency bands:** Different brainwave frequencies correlate with different mental states

**Recommended Reading:**
- NeuroSky ThinkGear documentation
- "Brain-Computer Interfaces" (Wolpaw & Wolpaw)
- OpenBCI learning resources

### ESP32 Development

- [ESP32 Official Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/)
- [Arduino ESP32 Guide](https://randomnerdtutorials.com/getting-started-with-esp32/)
- [WebServer Library Reference](https://github.com/espressif/arduino-esp32/tree/master/libraries/WebServer)

### ThinkGear Protocol

- Packet structure: Sync (AA AA) + Length + Payload + Checksum
- Data codes: 0x02 (quality), 0x04 (attention), 0x05 (meditation), 0x83 (EEG power)
- Baud rate: 57600 (headset) or 9600/115200 (configurable)

---

## 🙏 Acknowledgments

- **NeuroSky** for ThinkGear protocol documentation
- **Espressif** for ESP32 platform
- **Google** for Gemini AI API
- **Chart.js** for visualization library
- **Cape Peninsula University of Technology** for project support

---

## 📝 Version History

- **v1.0** (November 2024)
  - Initial release
  - Core EEG monitoring functionality
  - Web dashboard with real-time charts
  - AI chat integration
  - SD card logging
  - Mental state analysis

---

## 🚧 Future Roadmap

### Short-Term Goals (Q1 2025)
- [ ] Bluetooth LE support (eliminate Python bridge)
- [ ] Enhanced signal processing (bandpass filters)
- [ ] Configurable update intervals
- [ ] Export data to CSV/JSON via web interface

### Long-Term Goals (2025+)
- [ ] Multi-headset simultaneous monitoring
- [ ] Mobile app (React Native or Flutter)
- [ ] Machine learning state classifier training
- [ ] Integration with meditation/focus apps
- [ ] Advanced visualizations (spectrograms, topographic maps)

---

## 📚 Appendix

### A. Complete Bill of Materials

| Qty | Item | Specs | Source | Cost |
|-----|------|-------|--------|------|
| 1 | ESP32 Dev Board | ESP32-WROOM-32, 4MB Flash | AliExpress, Takealot | R200 |
| 1 | EEG Headset | NeuroSky ThinkGear compatible | AliExpress | R1,900 |
| 1 | Micro USB Cable | Data + Power, 1m | Local electronics store | R30 |
| 1 | USB A to A Cable | For receiver connection | Included with headset | - |
| 1 | SD Card Module (Optional) | SPI, 3.3V/5V | AliExpress, Communica | R50 |
| 1 | Micro SD Card (Optional) | Any capacity, FAT32 | Takealot | R40 |
| 1 | Breadboard (Optional) | For prototyping | Communica | R40 |
| 10 | Jumper Wires (Optional) | Male-to-male | Communica | R20 |

**Total: R2,280** (R2,400 with optional components)

### B. WiFi Network Requirements

| Setting | Required Value | Notes |
|---------|---------------|-------|
| Frequency | 2.4 GHz | ESP32 does NOT support 5 GHz |
| Security | WPA2-PSK | Open networks work but not recommended |
| Channel | Auto or 1-11 | Avoid congested channels |
| SSID | Any | Case-sensitive, max 32 chars |
| Password | Any | Min 8 chars for WPA2 |

### C. Serial Baud Rates

| Component | Baud Rate | Notes |
|-----------|-----------|-------|
| Headset → Receiver | 57,600 | Fixed by hardware |
| Receiver → PC | 57,600 | USB serial |
| Python Bridge | 115,200 | Configurable (faster = better) |
| ESP32 Serial Monitor | 115,200 | Must match Python bridge |
| ESP32 Web Server | N/A | HTTP over WiFi |

---

**END OF README**
