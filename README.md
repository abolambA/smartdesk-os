# SmartDesk OS

AI-Powered Personal Productivity Hub  
**Arduino UNO Q × Qualcomm Hackathon 2026**

## What it does
SmartDesk OS is a compact edge-AI device that sits on your desk and 
monitors three dimensions of your work session in real time:
- **Focus state** — detects when you're focused, distracted, or away
- **Posture health** — detects slouching and delivers subtle nudges
- **Workspace conditions** — adapts lighting based on ambient sensors

All AI inference runs locally on the Qualcomm QRB2210. No cloud. No subscriptions.

## Project structure
- `firmware/` — Arduino sketch for STM32 MCU
- `mpu/` — Python application stack for Qualcomm Linux MPU
- `dashboard/` — App Lab web dashboard (HTML/CSS/JS)
- `smartdesk_companion/` — Flutter mobile app (iOS + Android)
- `enclosure/` — 3D printable STL files
- `models/` — Edge Impulse model exports

## Build status
🔨 Phase 1 — Foundation (in progress)