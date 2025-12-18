# Airgapped-Pi-Toolkit
A dual-core Cyberdeck combining a Raspberry Pi Zero V1.3 and ESP32 via UART. Features active/passive Wi-Fi analysis (SIGINT), a wireless Telnet bridge for air-gapped terminal access, and USB HID payload injection. Built with a custom Python/C++ OS, headless OLED/Rotary encoder UI, and star-topology power management.

📖 Overview

The Airgapped-Pi-Toolkit is a custom-engineered field computer designed to bridge the gap between air-gapped hardware and wireless interactivity. It integrates a Raspberry Pi Zero V1.3 (Logic/USB HID) and an ESP32 (Radio/Display) via a raw UART serial link.

The device functions as a standalone "offensive security tool" capable of passive Wi-Fi traffic visualization, active network scanning, and automated USB payload injection, all controlled via a headless physical interface.

# Hardware Architecture

Primary Core: Raspberry Pi Zero V1.3 (Linux/Debian) - Handles OS logic, USB Gadget driver, and Python control scripts.

Co-Processor: ESP32-WROOM-32 (C++) - Handles real-time Wi-Fi scanning (Promiscuous Mode), OLED/Matrix driving, and the Telnet-to-Serial bridge.

Interlink: Bi-directional UART (TX/RX) @ 115200 Baud.

Displays: * 128x64 OLED (Data/Menu) via I2C.

4x 8x8 LED Matrix (Visuals/Graphs) via SPI.

Input: Rotary Encoder (Navigation) + Push Button (Select).

Power: Custom Star-Topology distribution to prevent brownouts during high-load operations. (ESP-32 and 72MAXX dot matrix powered by rail voltage, OLED Display powered by the ESP-32, Pi and 7" display for configuration powered inidiviually with wall power supplies)

# Key Features

1. Passive Signals Intelligence (SIGINT)

Traffic Rain: Uses the ESP32 in Promiscuous Mode to visualize real-time 2.4GHz Wi-Fi packet density as a "Matrix Rain" effect on the LED grid.

War Driving: Scans for SSID beacons and displays signal strength/encryption details on the OLED.

2. Air-Gapped Wireless Bridge

Solves the Pi Zero V1.3's lack of Wi-Fi by turning the ESP32 into a Telnet Server.

Connects to an iPad/Phone via Wi-Fi and tunnels terminal commands over Serial to the Pi, granting wireless shell access to an offline device.

3. USB HID Attack Vector ("Rubber Ducky")

Configures the Pi's USB data port as a generic Human Interface Device (Keyboard).

Injects pre-written keystroke payloads (e.g., "Hello World", "Open Terminal") into a target machine selected via the rotary encoder menu.

4. Headless "Cyberdeck OS"

Custom Python-based menu system (master_deck.py) launched via rc.local/crontab.

Monitors system health (CPU Load, Uptime, Disk Space) without an external monitor.

# Repository Structure

/ESP32_Firmware/: C++ code for the ESP32 (Display drivers, Wi-Fi stack, Serial logic).

/Pi_Python/: Python scripts running on the Pi (master_deck.py, hid_codes.py).

/Scripts/: Bash scripts for USB Gadget configuration (cyberdeck_usb).

# Installation & Setup

Prerequisites: * Raspberry Pi OS (Lite/Headless recommended)

Arduino IDE (for ESP32 flashing)

Python 3 + RPi.GPIO, pyserial, psutil

1. Configure Pi USB Gadget:
Add dtoverlay=dwc2 to /boot/config.txt and modules-load=dwc2,libcomposite to /boot/cmdline.txt. Run the cyberdeck_usb script on boot.

2. Deploy Python Controller:
Move master_deck.py and hid_codes.py to /boot/firmware/Deck_Codes/. Add to crontab:
@reboot cd /boot/firmware/Deck_Codes && /usr/bin/python3 master_deck.py &

3. Flash ESP32:
Upload ESP32_Cyberdeck.ino to the ESP32 via USB.

4. Wire & Power:
Connect Pi TX/RX to ESP32 RX/TX. Ensure a common ground. Power via 5V rail (Star Topology).

⚠️ Disclaimer

This project is for educational and defensive security research purposes only. Use responsibly and only on networks/devices you own or have permission to test.
