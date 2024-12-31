# QuickTroll - RFID Shopping Cart System

QuickTroll is an RFID-based shopping cart system using an ESP8266 and MFRC522 RFID module. It allows users to scan products, manage their cart via a web interface, and complete payments using Razorpay.


## Features

- RFID Product Scanning: Add/remove products using RFID tags.
- Web Interface: Real-time cart updates with add/remove/increment/decrement options.
- Payment Integration: Secure payments via Razorpay.
- Invoice Generation: Automatically generates a detailed invoice post-payment.
- Dynamic Product Management: Easily add/modify products in the code.


## Hardware

1. ESP8266 (e.g., NodeMCU)
2. MFRC522 RFID Module
3. RFID Tags
4. Jumper Wires


## Wiring

| MFRC522 Pin | ESP8266 Pin |
|-------------|-------------|
| VCC         | 3.3V        |
| GND         | GND         |
| RST         | D3          |
| SDA (SS)    | D4          |
| MOSI        | D7          |
| MISO        | D6          |
| SCK         | D5          |


## Setup

1. Install Libraries:
   - MFRC522, ESP8266WiFi, ESP8266WebServer, WebSocketsServer, ArduinoJson.

2. Update WiFi Credentials:
   - Replace ssid and password in the code.

3. Upload Code:
   - Upload the code to your ESP8266.

4. Access Web Interface:
   - Open the Serial Monitor to get the IP address and access the interface in a browser.

---

## Usage

1. Scan Products: Use RFID tags to add/remove items.
2. Adjust Quantities: Use the web interface to modify quantities.
3. Checkout: Enter customer details and complete payment.
4. View Invoice: An invoice is generated after payment.

---

## Customization

1. Add Products:
   ```cpp
   std::vector<Product> products = {
     {"F3A57B11", "Rice", 10.00, "https://i.imgur.com/CZ98GeU.jpg"},
     {"24550D04", "Oil", 20.00, "https://i.imgur.com/ijfSykj.jpg"},
   };
   ```

2. Change Payment Gateway:
   - Replace the Razorpay key in the pay_now() function.


## Troubleshooting

- RFID Not Working: Check wiring and power supply.
- Web Interface Issues: Verify WiFi connection and IP address.
- Payment Fails: Ensure correct Razorpay key and customer details.


Enjoy QuickTroll! 🛒✨
