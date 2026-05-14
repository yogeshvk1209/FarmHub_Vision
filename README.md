# FarmHub Vision

## Overview
**FarmHub Vision** is an image capture and transmission node based on the **ESP32-CAM (AI-Thinker)**. It captures high-quality snapshots of the farm environment and uploads them to the cloud for remote monitoring. To conserve power, the device operates on a "Snapshot-and-Sleep" cycle, spending most of its time in Deep Sleep.

## Features
- **High-Res Imaging:** Captures images at SVGA resolution (800x600).
- **Base64 Encoding:** Converts raw image buffers into Base64 strings for simple text-based API transmission.
- **Secure Upload:** Posts image data to an AWS API Gateway endpoint via HTTPS.
- **Power Efficiency:** Uses ESP32 Deep Sleep to minimize battery drain between captures.
- **Network Stability:** Uses manual DNS configuration (Google 8.8.8.8) to bypass common 4G/Cellular DNS resolution issues.
- **Paced Streaming:** Streams large Base64 payloads in 1KB chunks with delays to ensure stability over low-bandwidth cellular connections.

## Hardware Configuration
Uses the standard **AI-Thinker ESP32-CAM** pinout:
- **Camera Pins:** Standard configuration (D0-D7, XCLK, PCLK, VSYNC, HREF, SIOD, SIOC).
- **Flash LED:** GPIO 4 (Note: can be used for illumination if needed).
- **Status LED:** GPIO 33 (Onboard red LED).

## Operation Cycle
1. **Wake up** from Deep Sleep.
2. **Initialize Camera** hardware.
3. **Connect to WiFi** (via 4G Router managed by Supervisor).
4. **Capture Image** and encode to Base64.
5. **Release Camera Buffer** to free RAM for SSL/TLS handshake.
6. **POST to Cloud** via AWS API Gateway.
7. **Enter Deep Sleep** for 15 minutes.

## Communication
- **Protocol:** HTTPS (Port 443).
- **Authentication:** `x-api-key` header for AWS API Gateway.
- **Payload:** Raw text (Base64 string) in the POST body.

## Setup & Deployment
1. **Secrets:** Copy `src/example_secrets.h` to `src/secrets.h` and configure:
   - `WIFI_SSID` & `WIFI_PASS`
   - `AWS_API_HOST`
   - `AWS_API_PATH`
   - `AWS_API_KEY`
2. **PlatformIO:** Use the provided `platformio.ini` to build and upload to an ESP32-CAM.
3. **Power:** Designed to be powered via the 4G Router's power rail or a shared battery managed by the Supervisor.
