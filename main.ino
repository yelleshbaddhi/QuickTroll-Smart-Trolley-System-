#include <SPI.h>
#include <MFRC522.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>
#include <time.h>
#include <vector>

// RFID pins
constexpr uint8_t RST_PIN = D3;
constexpr uint8_t SS_PIN = D4;

MFRC522 rfid(SS_PIN, RST_PIN);
MFRC522::MIFARE_Key key;

// WiFi credentials
const char* ssid = "Kossn-Lab-2";
const char* password = "L12345678";

ESP8266WebServer server(80);
WebSocketsServer webSocket(81);

// Product structure
struct Product {
  String uid;
  String name;
  float price;
  String imageUrl;
};

// Define products dynamically
std::vector<Product> products = {
  {"F3A57B11", "Rice", 10.00, "https://i.imgur.com/CZ98GeU.jpg"},
  {"24550D04", "Oil", 20.00, "https://i.imgur.com/ijfSykj.jpg"},
  {"2046E959", "Books", 30.00, "https://i.imgur.com/PS43M6v.jpg"},
  {"6007DD14", "Apple", 30.00, "https://i.imgur.com/WZk8YiT.jpg"},
  // Add more products here as needed
};

// Keep track of quantities dynamically
std::vector<int> quantities(products.size(), 0);

// Mode: 0 for adding, 1 for removing
int mode = 0;

String billDetails;
String lastScannedUID = "";
unsigned long lastScanTime = 0;
const unsigned long debounceDelay = 500; // Debounce delay in milliseconds

void setup() {
  Serial.begin(115200);
  SPI.begin();
  rfid.PCD_Init();

  // Connect to WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // Set up web server routes
  server.on("/", handleRoot);
  server.on("/remove", HTTP_POST, handleRemove);
  server.on("/increment", HTTP_POST, handleIncrement);
  server.on("/decrement", HTTP_POST, handleDecrement);
  server.on("/payment_success", HTTP_POST, handlePaymentSuccess);
  server.on("/bill", HTTP_GET, handleBill);
  server.on("/toggle_mode", HTTP_POST, handleToggleMode);
  server.begin();
  Serial.println("HTTP server started");

  // Start WebSocket server
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  Serial.println("WebSocket server started");

  // Set the time to IST (Indian Standard Time)
  configTime(19800, 0, "pool.ntp.org"); // IST is UTC+5:30
}

void loop() {
  server.handleClient();
  webSocket.loop();

  if (rfid.PICC_IsNewCardPresent()) {
    if (rfid.PICC_ReadCardSerial()) {
      String tag = "";
      for (byte i = 0; i < 4; i++) {
        tag += (rfid.uid.uidByte[i] < 0x10 ? "0" : "") + String(rfid.uid.uidByte[i], HEX);
      }
      tag.toUpperCase();
      Serial.print("Scanned UID: ");
      Serial.println(tag);

      // Debounce the RFID read
      unsigned long currentTime = millis();
      if (tag == lastScannedUID && (currentTime - lastScanTime) < debounceDelay) {
        Serial.println("Debounce: Ignoring duplicate scan");
        rfid.PICC_HaltA();
        rfid.PCD_StopCrypto1();
        return;
      }
      lastScannedUID = tag;
      lastScanTime = currentTime;

      bool productFound = false;
      for (size_t i = 0; i < products.size(); i++) {
        if (tag == products[i].uid) {
          if (mode == 0) {
            quantities[i]++;
            Serial.printf("Added: %s - Price: \$%.2f - Quantity: %d\n",
                          products[i].name.c_str(), products[i].price, quantities[i]);
          } else {
            if (quantities[i] > 0) {
              quantities[i]--;
              Serial.printf("Removed: %s - Price: \$%.2f - Quantity: %d\n",
                            products[i].name.c_str(), products[i].price, quantities[i]);
            }
          }
          productFound = true;
          break;
        }
      }

      if (!productFound) {
        Serial.println("Unknown card detected");
      }

      rfid.PICC_HaltA();
      rfid.PCD_StopCrypto1();

      sendWebSocketUpdate();
    } else {
      Serial.println("Failed to read card serial");
      rfid.PCD_Init(); // Reinitialize the RFID reader
    }
  }
}

void handleRoot() {
  String html = "<!DOCTYPE html><html lang='en'><head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<link rel='stylesheet' href='https://stackpath.bootstrapcdn.com/bootstrap/4.5.2/css/bootstrap.min.css'>";
  html += "<link rel='stylesheet' href='https://cdnjs.cloudflare.com/ajax/libs/font-awesome/5.15.4/css/all.min.css'>";
  html += "<style>";
  html += ":root {";
  html += "    --primary-color: #4a90e2;";
  html += "    --secondary-color: #f44336;";
  html += "    --text-color: #333;";
  html += "    --light-gray: #f5f5f5;";
  html += "    --medium-gray: #e0e0e0;";
  html += "    --dark-gray: #757575;";
  html += "}";
  html += "* {";
  html += "    box-sizing: border-box;";
  html += "    margin: 0;";
  html += "    padding: 0;";
  html += "}";
  html += "body {";
  html += "    font-family: 'Poppins', sans-serif;";
  html += "    background-color: var(--light-gray);";
  html += "    color: var(--text-color);";
  html += "    line-height: 1.6;";
  html += "}";
  html += ".container {";
  html += "    width: 90%;";
  html += "    max-width: 1200px;";
  html += "    margin: 2rem auto;";
  html += "    background-color: #fff;";
  html += "    padding: 1.5rem;";
  html += "    border-radius: 12px;";
  html += "    box-shadow: 0 10px 30px rgba(0, 0, 0, 0.1);";
  html += "    display: flex;";
  html += "    gap: 1.5rem;";
  html += "}";
  html += ".cart-items {";
  html += "    flex: 2;";
  html += "}";
  html += ".cart-header {";
  html += "    display: flex;";
  html += "    align-items: center;";
  html += "    margin-bottom: 1rem;";
  html += "    padding-bottom: 0.75rem;";
  html += "    border-bottom: 1px solid var(--medium-gray);";
  html += "}";
  html += ".cart-header h1 {";
  html += "    font-size: 1.5rem;";
  html += "    font-weight: 600;";
  html += "}";
  html += ".cart-icon {";
  html += "    font-size: 1.25rem;";
  html += "    color: var(--primary-color);";
  html += "    position: relative;";
  html += "    margin-left: auto;";
  html += "}";
  html += ".cart-count {";
  html += "    position: absolute;";
  html += "    top: -5px;";
  html += "    right: -5px;";
  html += "    background-color: var(--secondary-color);";
  html += "    color: white;";
  html += "    border-radius: 50%;";
  html += "    width: 18px;";
  html += "    height: 18px;";
  html += "    display: flex;";
  html += "    justify-content: center;";
  html += "    align-items: center;";
  html += "    font-size: 0.75rem;";
  html += "    font-weight: bold;";
  html += "}";
  html += ".cart-item {";
  html += "    display: flex;";
  html += "    align-items: center;";
  html += "    padding: 1rem 0;";
  html += "    border-bottom: 1px solid var(--medium-gray);";
  html += "}";
  html += ".cart-item:last-child {";
  html += "    border-bottom: none;";
  html += "}";
  html += ".cart-item img {";
  html += "    width: 90px;";
  html += "    height: 90px;";
  html += "    border-radius: 8px;";
  html += "    object-fit: cover;";
  html += "}";
  html += ".cart-item-details {";
  html += "    flex-grow: 1;";
  html += "    margin-left: 1rem;";
  html += "}";
  html += ".cart-item-details h2 {";
  html += "    font-size: 1rem;";
  html += "    margin-bottom: 0.5rem;";
  html += "    font-weight: 600;";
  html += "}";
  html += ".cart-item-details p {";
  html += "    color: var(--dark-gray);";
  html += "    font-size: 0.85rem;";
  html += "    margin-bottom: 0.5rem;";
  html += "}";
  html += ".cart-item-price {";
  html += "    font-size: 1rem;";
  html += "    font-weight: 600;";
  html += "    margin-left: 1rem;";
  html += "}";
  html += ".quantity-selector {";
  html += "    display: flex;";
  html += "    align-items: center;";
  html += "    margin-top: 0.5rem;";
  html += "}";
  html += ".quantity-selector button {";
  html += "    background-color: var(--light-gray);";
  html += "    border: none;";
  html += "    width: 28px;";
  html += "    height: 28px;";
  html += "    font-size: 0.9rem;";
  html += "    cursor: pointer;";
  html += "    transition: background-color 0.3s;";
  html += "}";
  html += ".quantity-selector button:hover {";
  html += "    background-color: var(--medium-gray);";
  html += "}";
  html += ".quantity-selector input {";
  html += "    width: 35px;";
  html += "    height: 28px;";
  html += "    text-align: center;";
  html += "    font-size: 0.9rem;";
  html += "    border: 1px solid var(--medium-gray);";
  html += "    margin: 0 0.5rem;";
  html += "}";
  html += ".cart-summary {";
  html += "    flex: 1;";
  html += "    background-color: var(--light-gray);";
  html += "    padding: 1rem;";
  html += "    border-radius: 8px;";
  html += "    align-self: flex-start;";
  html += "    position: sticky;";
  html += "    top: 2rem;";
  html += "}";
  html += ".cart-summary h2 {";
  html += "    font-size: 1.25rem;";
  html += "    margin-bottom: 1rem;";
  html += "    padding-bottom: 0.75rem;";
  html += "    border-bottom: 1px solid var(--medium-gray);";
  html += "}";
  html += ".input-group {";
  html += "    margin-bottom: 0.75rem;";
  html += "}";
  html += ".input-group label {";
  html += "    display: block;";
  html += "    margin-bottom: 0.25rem;";
  html += "    font-size: 0.9rem;";
  html += "}";
  html += ".input-group input {";
  html += "    width: 100%;";
  html += "    padding: 0.5rem;";
  html += "    border: 1px solid var(--medium-gray);";
  html += "    border-radius: 5px;";
  html += "    font-size: 0.9rem;";
  html += "}";
  html += ".cart-summary p {";
  html += "    margin-bottom: 0.5rem;";
  html += "    font-size: 0.9rem;";
  html += "}";
  html += ".cart-summary .apply-code {";
  html += "    margin-top: 1rem;";
  html += "}";
  html += ".cart-summary .apply-code input {";
  html += "    width: 100%;";
  html += "    padding: 0.5rem;";
  html += "    margin-bottom: 0.5rem;";
  html += "    border: 1px solid var(--medium-gray);";
  html += "    border-radius: 5px;";
  html += "    font-size: 0.9rem;";
  html += "}";
  html += ".cart-summary .apply-code button, .cart-summary .checkout-btn {";
  html += "    width: 100%;";
  html += "    padding: 0.5rem;";
  html += "    border: none;";
  html += "    border-radius: 5px;";
  html += "    cursor: pointer;";
  html += "    font-size: 0.9rem;";
  html += "    font-weight: 500;";
  html += "    transition: background-color 0.3s;";
  html += "}";
  html += ".cart-summary .apply-code button {";
  html += "    background-color: var(--primary-color);";
  html += "    color: white;";
  html += "}";
  html += ".cart-summary .apply-code button:hover {";
  html += "    background-color: #3a7bc8;";
  html += "}";
  html += ".cart-summary .checkout-btn {";
  html += "    background-color: var(--secondary-color);";
  html += "    color: white;";
  html += "    font-weight: 600;";
  html += "    margin-top: 1rem;";
  html += "}";
  html += ".cart-summary .checkout-btn:hover {";
  html += "    background-color: #d1004a;";
  html += "}";
  html += ".total {";
  html += "    font-size: 1rem;";
  html += "    font-weight: 600;";
  html += "    margin-top: 1rem;";
  html += "    padding-top: 0.75rem;";
  html += "    border-top: 1px solid var(--medium-gray);";
  html += "}";
  html += ".remove-button {";
  html += "    background-color: #ff4d4d;";
  html += "    color: white;";
  html += "    border: none;";
  html += "    padding: 0.5rem 1rem;";
  html += "    border-radius: 5px;";
  html += "    cursor: pointer;";
  html += "    font-size: 0.9rem;";
  html += "    font-weight: 500;";
  html += "    transition: background-color 0.3s;";
  html += "    margin-left: 1rem;";
  html += "}";
  html += ".remove-button:hover {";
  html += "    background-color: #ff1a1a;";
  html += "}";
  html += "@media (max-width: 768px) {";
  html += "    .container {";
  html += "        flex-direction: column;";
  html += "        padding: 1rem;";
  html += "    }";
  html += "    .cart-items, .cart-summary {";
  html += "        width: 100%;";
  html += "    }";
  html += "    .cart-summary {";
  html += "        margin-top: 2rem;";
  html += "        position: static;";
  html += "    }";
  html += "    .cart-header h1 {";
  html += "        font-size: 1.25rem;";
  html += "    }";
  html += "    .cart-item {";
  html += "        flex-direction: row;";
  html += "        align-items: center;";
  html += "    }";
  html += "    .cart-item img {";
  html += "        width: 60px;";
  html += "        height: 60px;";
  html += "        margin-right: 1rem;";
  html += "    }";
  html += "    .cart-item-details {";
  html += "        margin-left: 0;";
  html += "        flex-grow: 1;";
  html += "    }";
  html += "    .cart-item-price {";
  html += "        margin-left: 0;";
  html += "        margin-top: 0.5rem;";
  html += "    }";
  html += "    .quantity-selector {";
  html += "        margin-top: 0;";
  html += "    }";
  html += "    .toggle {";
  html += "        width: 30px;";
  html += "        height: 15px;";
  html += "    }";
  html += "    .toggle::after {";
  html += "        width: 15px;";
  html += "        height: 15px;";
  html += "    }";
  html += "    #toggleText {";
  html += "        display: none;";
  html += "    }";
  html += "    .remove-button {";
  html += "        background-color: transparent;";
  html += "        color: #ff4d4d;";
  html += "        border: none;";
  html += "        padding: 0;";
  html += "        font-size: 1.25rem;";
  html += "    }";
  html += "    .remove-button:hover {";
  html += "        color: #ff1a1a;";
  html += "    }";
  html += "}";
  html += "</style>";
  html += "<title>QuickTroll</title></head><body>";
  html += "<div class='container'>";
  html += "<div class='cart-items'>";
  html += "<div class='cart-header'>";
  html += "<img src='https://i.imgur.com/TE7ygoV.jpg' alt='Logo' style='width: 50px; height: 50px;'>";
  html += "<h1>QuickTroll</h1>";
  html += "<div class='cart-icon'>";
  html += "<i class='fas fa-shopping-cart'></i>";
  html += "<span id='cartItemCount' class='cart-count'>0</span>";
  html += "</div>";
  html += "</div>";
  html += "<div style='display: flex; align-items: center; justify-content: space-between;'>";
  html += "<div style='display: flex; align-items: center;'>";
  html += "<input type='checkbox' id='switch' class='checkbox' onclick='toggleMode()' />";
  html += "<label for='switch' class='toggle'></label>";
  html += "<div id='toggleText' style='margin-left: 10px; font-weight: bold;'>Add</div>";
  html += "</div>";
  html += "<span style='margin-left: auto;'>IP Address: " + WiFi.localIP().toString() + "</span>";
  html += "</div>";
  html += "<div id='cartItems'>";
  html += "<div class='empty-cart'>";
  html += "<p>Your cart is empty</p>";
  html += "</div>";
  html += "</div>";
  html += "</div>";
  html += "<div class='cart-summary'>";
  html += "<h2>Order Summary</h2>";
  html += "<div class='input-group'>";
  html += "<label for='name'><strong>Customer Name:</strong></label>";
  html += "<input type='text' id='name' name='name' placeholder='Enter your name' required>";
  html += "</div>";
  html += "<div class='input-group'>";
  html += "<label for='email'><strong>Email (optional):</strong></label>";
  html += "<input type='email' id='email' name='email' placeholder='Enter your email'>";
  html += "</div>";
  html += "<div class='input-group'>";
  html += "<label for='phone'><strong>Phone Number:</strong></label>";
  html += "<input type='tel' id='phone' name='phone' placeholder='Enter your phone number' required>";
  html += "</div>";
  html += "<p><strong>Subtotal:</strong> $<span id='subtotal'>0.00</span></p>";
  html += "<p><strong>Shipping:</strong> $<span id='shipping'>0.00</span></p>";
  html += "<p><strong>Tax:</strong> $<span id='tax'>0.00</span></p>";
  html += "<p class='total'><strong>Total:</strong> $<span id='total'>0.00</span></p>";
  html += "<button class='btn btn-primary mb-3' onclick='continueShopping()'>Continue Shopping</button>";
  html += "<button id='proceedToCheckoutButton' class='checkout-btn' onclick='proceedToCheckout()'>Proceed to Checkout</button>";
  html += "</div>";
  html += "</div>";
  html += "<script src='https://code.jquery.com/jquery-3.6.1.min.js'></script>";
  html += "<script src='https://checkout.razorpay.com/v1/checkout.js'></script>";
  html += "<script type='text/javascript'>";
  html += "var socket = new WebSocket('ws://' + location.hostname + ':81/');";
  html += "socket.onmessage = function(event) { updateCartDetails(event.data); };";
  html += "function updateCartDetails(data) {";
  html += "  var cart = JSON.parse(data);";
  html += "  var cartItemsDiv = document.getElementById('cartItems');";
  html += "  cartItemsDiv.innerHTML = '';";
  html += "  var totalPrice = 0.0;";
  html += "  cart.quantities.forEach((quantity, i) => {";
  html += "    if (quantity > 0) {";
  html += "      var cartItemDiv = document.createElement('div');";
  html += "      cartItemDiv.className = 'cart-item';";
  html += "      cartItemDiv.innerHTML = `";
  html += "        <img src='${cart.products[i].imageUrl}' alt='${cart.products[i].name}'>";
  html += "        <div class='cart-item-details'>";
  html += "          <h2>\${cart.products[i].name}</h2>";
  html += "          <p>Price: $${cart.products[i].price.toFixed(2)}</p>";
  html += "          <div class='quantity-selector'>";
  html += "            <button onclick='decrementProduct(${i})'><i class='fas fa-minus'></i></button>";
  html += "            <span>${quantity}</span>";
  html += "            <button onclick='incrementProduct(\${i})'><i class='fas fa-plus'></i></button>";
  html += "          </div>";
  html += "        </div>";
  html += "        <p class='cart-item-price'>$${(quantity * cart.products[i].price).toFixed(2)}</p>";
  html += "        <button class='remove-button' onclick='removeProduct(\${i})'><i class='fas fa-trash'></i></button>";
  html += "      `;";
  html += "      cartItemsDiv.appendChild(cartItemDiv);";
  html += "      totalPrice += quantity * cart.products[i].price;";
  html += "    }";
  html += "  });";
  html += "  document.getElementById('subtotal').innerText = totalPrice.toFixed(2);";
  html += "  document.getElementById('shipping').innerText = '0.00';";
  html += "  document.getElementById('tax').innerText = (totalPrice * 0.18).toFixed(2);";
  html += "  document.getElementById('total').innerText = (totalPrice * 1.18).toFixed(2);";
  html += "  document.getElementById('cartItemCount').innerText = cart.quantities.reduce((a, b) => a + b, 0);";
  html += "  if (totalPrice > 0) {";
  html += "    document.getElementById('proceedToCheckoutButton').style.display = 'block';";
  html += "  } else {";
  html += "    document.getElementById('proceedToCheckoutButton').style.display = 'none';";
  html += "  }";
  html += "}";
  html += "function removeProduct(index) {";
  html += "  fetch('/remove', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ index: index }) });";
  html += "}";
  html += "function incrementProduct(index) {";
  html += "  fetch('/increment', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ index: index }) });";
  html += "}";
  html += "function decrementProduct(index) {";
  html += "  fetch('/decrement', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ index: index }) });";
  html += "}";
  html += "function proceedToCheckout() {";
  html += "  var name = document.getElementById('name').value;";
  html += "  var email = document.getElementById('email').value;";
  html += "  var phone = document.getElementById('phone').value;";
  html += "  if (name && phone) {";
  html += "    pay_now(name, email, phone);";
  html += "  } else {";
  html += "    alert('Please fill in all required fields.');";
  html += "  }";
  html += "}";
  html += "function pay_now(name, email, phone){";
  html += "  var amount = parseFloat(document.getElementById('total').innerText);";
  html += "  var options = {";
  html += "    'key': 'rzp_test_dhYJFlohg88eyl',";
  html += "    'amount': amount * 100,";
  html += "    'currency': 'INR',";
  html += "    'name': 'QuickTroll',";
  html += "    'description': 'Payment for your items',";
  html += "    'prefill': { 'name': name, 'email': email, 'contact': phone },";
  html += "    'handler': function (response) {";
  html += "      fetch('/payment_success', {";
  html += "        method: 'POST',";
  html += "        headers: { 'Content-Type': 'application/json' },";
  html += "        body: JSON.stringify({";
  html += "          payment_id: response.razorpay_payment_id,";
  html += "          amount: amount * 100,";
  html += "          name: name,";
  html += "          email: email,";
  html += "          phone: phone";
  html += "        })";
  html += "      }).then(response => response.json())";
  html += "        .then(data => {";
  html += "          if (data.status === 'success') {";
  html += "            window.location.href = '/bill';";
  html += "          } else {";
  html += "            alert('Payment failed. Please try again.');";
  html += "          }";
  html += "        });";
  html += "    }";
  html += "  };";
  html += "  var rzp1 = new Razorpay(options);";
  html += "  rzp1.open();";
  html += "}";
  html += "function continueShopping() {";
  html += "  window.location.href = '/';";
  html += "}";
  html += "function toggleMode() {";
  html += "  fetch('/toggle_mode', { method: 'POST' }).then(response => response.json()).then(data => {";
  html += "    document.getElementById('toggleText').innerText = 'Mode: ' + data.mode;";
  html += "  });";
  html += "}";
  html += "</script>";
  html += "</body></html>";

  server.send(200, "text/html", html);
}

void handleToggleMode() {
  mode = (mode + 1) % 2; // Toggle between 0 and 1
  Serial.printf("Mode toggled to: %s\n", mode == 0 ? "Add" : "Remove");
  server.send(200, "application/json", "{\"status\":\"success\",\"mode\":\"" + String(mode == 0 ? "Add" : "Remove") + "\"}");
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  // Handle WebSocket events if needed
}

void sendWebSocketUpdate() {
  DynamicJsonDocument doc(1024);
  JsonArray quantitiesArray = doc.createNestedArray("quantities");
  JsonArray productsArray = doc.createNestedArray("products");
  float totalPrice = 0.0;

  for (size_t i = 0; i < products.size(); i++) {
    quantitiesArray.add(quantities[i]);
    JsonObject product = productsArray.createNestedObject();
    product["name"] = products[i].name;
    product["price"] = products[i].price;
    product["imageUrl"] = products[i].imageUrl;
    totalPrice += quantities[i] * products[i].price;
  }

  doc["totalPrice"] = totalPrice;

  String json;
  serializeJson(doc, json);
  webSocket.broadcastTXT(json);
}

void handleRemove() {
  DynamicJsonDocument doc(64);
  DeserializationError error = deserializeJson(doc, server.arg("plain"));

  if (error) {
    server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Invalid JSON\"}");
    return;
  }

  int index = doc["index"];
  if (index >= 0 && index < static_cast<int>(products.size()) && quantities[index] > 0) {
    quantities[index] = 0;
    Serial.printf("Removed: %s\n", products[index].name.c_str());
    sendWebSocketUpdate();
    server.send(200, "application/json", "{\"status\":\"success\"}");
  } else {
    server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Invalid index\"}");
  }
}

void handleIncrement() {
  DynamicJsonDocument doc(64);
  DeserializationError error = deserializeJson(doc, server.arg("plain"));

  if (error) {
    server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Invalid JSON\"}");
    return;
  }

  int index = doc["index"];
  if (index >= 0 && index < static_cast<int>(products.size())) {
    quantities[index]++;
    Serial.printf("Incremented: %s\n", products[index].name.c_str());
    sendWebSocketUpdate();
    server.send(200, "application/json", "{\"status\":\"success\"}");
  } else {
    server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Invalid index\"}");
  }
}

void handleDecrement() {
  DynamicJsonDocument doc(64);
  DeserializationError error = deserializeJson(doc, server.arg("plain"));

  if (error) {
    server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Invalid JSON\"}");
    return;
  }

  int index = doc["index"];
  if (index >= 0 && index < static_cast<int>(products.size()) && quantities[index] > 0) {
    quantities[index]--;
    Serial.printf("Decremented: %s\n", products[index].name.c_str());
    sendWebSocketUpdate();
    server.send(200, "application/json", "{\"status\":\"success\"}");
  } else {
    server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Invalid index or quantity already zero\"}");
  }
}

void handlePaymentSuccess() {
  if (server.method() != HTTP_POST) {
    server.send(405, "text/plain", "Method Not Allowed");
    return;
  }

  DynamicJsonDocument doc(1024);
  DeserializationError error = deserializeJson(doc, server.arg("plain"));

  if (error) {
    Serial.println("Failed to parse JSON");
    server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Invalid JSON\"}");
    return;
  }

  String paymentId = doc["payment_id"].as<String>();
  String name = doc["name"].as<String>();
  String email = doc["email"].as<String>();
  String phone = doc["phone"].as<String>();
  float amount = doc["amount"].as<float>() / 100.0; // Convert paise to currency

  // Store the bill details
  DynamicJsonDocument billDoc(2048);
  billDoc["billNumber"] = random(100000, 999999);
  billDoc["paymentId"] = paymentId;
  billDoc["upiPaymentId"] = "UPI" + String(random(1000000000, 9999999999ULL));
  billDoc["name"] = name;
  billDoc["email"] = email;
  billDoc["phone"] = phone;
  billDoc["amount"] = amount;
  JsonArray productsArray = billDoc.createNestedArray("products");

  for (size_t i = 0; i < products.size(); i++) {
    if (quantities[i] > 0) {
      JsonObject product = productsArray.createNestedObject();
      product["name"] = products[i].name;
      product["quantity"] = quantities[i];
      product["price"] = products[i].price;
    }
  }

  billDoc["dateTime"] = getDateTime();

  // Serialize the JSON document to a string
  serializeJson(billDoc, billDetails);

  // Clear the quantities of all products
  for (size_t i = 0; i < products.size(); i++) {
    quantities[i] = 0;
  }

  Serial.println("Payment successful. Cart cleared.");
  sendWebSocketUpdate(); // Send updated quantities to WebSocket
  server.send(200, "application/json", "{\"status\":\"success\"}");
}

String getDateTime() {
  time_t now;
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return "Failed to obtain time";
  }
  char datetime[50];
  strftime(datetime, sizeof(datetime), "%d %B %Y - %H:%M:%S", &timeinfo);
  return String(datetime);
}

void handleBill() {
  if (server.method() != HTTP_GET) {
    server.send(405, "text/plain", "Method Not Allowed");
    return;
  }

  String html = "<!DOCTYPE html><html lang='en'><head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<title>Billing Invoice</title>";
  html += "<style>";
  html += "* { box-sizing: border-box; margin: 0; padding: 0; }";
  html += "body { font-family: 'Poppins', sans-serif; font-size: 14px; color: #333; background-color: #f5f5f5; padding: 20px; }";
  html += ".invoice-container { max-width: 800px; margin: 40px auto; background-color: #fff; padding: 40px; box-shadow: 0 4px 15px rgba(0, 0, 0, 0.1); border-radius: 12px; }";
  html += ".invoice-header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 40px; border-bottom: 2px solid #f0f0f0; padding-bottom: 10px; }";
  html += ".invoice-header img { width: 50px; height: 50px; margin-left: 10px; }";
  html += ".invoice-header h1 { font-size: 30px; font-weight: 600;}";
  html += ".invoice-header p { font-size: 14px; color: #757575; }";
  html += ".invoice-number { font-size: 14px; color: #757575; }";
  html += ".invoice-number span { font-size: 24px; font-weight: 600; color: #4a90e2; }";
  html += ".invoice-date { text-align: right; font-size: 18px; color: #757575; margin-bottom: 20px; }";
  html += ".invoice-info { display: flex; justify-content: space-between; margin-bottom: 40px; }";
  html += ".invoice-info div { flex: 1; margin-right: 20px; }";
  html += ".invoice-info div:last-child { margin-right: 0; }";
  html += ".invoice-info p { font-size: 14px; line-height: 1.6; }";
  html += ".invoice-info p strong { font-weight: 600; color: #4a90e2; }";
  html += ".invoice-table { width: 100%; border-collapse: collapse; margin-bottom: 40px; }";
  html += ".invoice-table th, .invoice-table td { padding: 15px; text-align: left; border-bottom: 1px solid #ddd; }";
  html += ".invoice-table th { background-color: #f8f9fa; font-weight: 600; }";
  html += ".invoice-table tr:nth-child(even) { background-color: #f9f9f9; }";
  html += ".invoice-total { display: flex; flex-direction: column; align-items: flex-end; font-size: 18px; font-weight: 600; margin-bottom: 30px; color: #333; }";
  html += ".invoice-total div { margin-bottom: 10px; }";
  html += ".invoice-total span { margin-left: 15px; }";
  html += ".invoice-footer { text-align: center; font-size: 14px; color: #666; }";
  html += ".btn { display: inline-block; padding: 12px 25px; font-size: 14px; font-weight: 600; border-radius: 8px; text-decoration: none; transition: all 0.3s ease; margin-right: 10px; }";
  html += ".btn-print { background-color: #4a90e2; color: #fff; }";
  html += ".btn-print:hover { background-color: #3a7bc8; box-shadow: 0 4px 10px rgba(74, 144, 226, 0.2); }";
  html += ".btn-back { background-color: #6c757d; color: #fff; }";
  html += ".btn-back:hover { background-color: #495057; box-shadow: 0 4px 10px rgba(108, 117, 125, 0.2); }";
  html += ".invoice-footer p { margin-top: 20px; font-size: 16px; color: #333; }";
  html += "@media (max-width: 768px) {";
  html += "  .invoice-info { flex-direction: column; }";
  html += "  .invoice-info div { margin-bottom: 20px; margin-right: 0; }";
  html += "  .invoice-header { flex-direction: column; align-items: flex-start; }";
  html += "  .invoice-header h1 { margin-bottom: 10px; }";
  html += "  .invoice-header p { margin-bottom: 10px; }";
  html += "  .invoice-date { text-align: left; margin-bottom: 10px; }";
  html += "  .invoice-table th, .invoice-table td { padding: 10px; }";
  html += "  .invoice-total { align-items: center; }";
  html += "  .invoice-total div { margin-bottom: 5px; }";
  html += "}";
  html += "</style></head><body>";
  html += "<div class='invoice-container'>";
  html += "<div class='invoice-header'>";
  html += "<div>";
  html += "<p class='invoice-number'><span>Invoice</span> #";

  DynamicJsonDocument doc(2048);
  DeserializationError error = deserializeJson(doc, billDetails);

  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    html += "Error</p>";
  } else {
    html += String(doc["billNumber"].as<long>()) + "</p>";
  }

  html += "</div>";
  html += "<div style='display: flex; align-items: center;'>";
  html += "<img src='https://i.imgur.com/TE7ygoV.jpg' alt='Logo'>";
  html += "<h1>QuickTroll</h1>";
  html += "</div>";
  html += "</div>";

  if (!error) {
    String dateTime = doc["dateTime"].as<String>();
    html += "<div class='invoice-date'>" + dateTime + "</div>";

    html += "<div class='invoice-info'>";
    html += "<div>";
    html += "<p><strong>From:</strong></p>";
    html += "<p>Kakatiya Olympoid School</p>";
    html += "<p>Nizambad</p>";
    html += "</div>";
    html += "<div>";
    html += "<p><strong>To:</strong></p>";
    html += "<p>" + doc["name"].as<String>() + "</p>";
    html += "<p>" + doc["email"].as<String>() + "</p>";
    html += "<p>" + doc["phone"].as<String>() + "</p>";
    html += "</div>";
    html += "<div>";
    html += "<p><strong>Payment ID:</strong> " + doc["paymentId"].as<String>() + "</p>";
    html += "<p><strong>UPI Payment ID:</strong> " + doc["upiPaymentId"].as<String>() + "</p>";
    html += "</div>";
    html += "</div>";

    html += "<table class='invoice-table'>";
    html += "<thead><tr><th>Item</th><th>Quantity</th><th>Price</th><th>Total</th></tr></thead>";
    html += "<tbody>";

    float totalAmount = 0.0;
    JsonArray productsArray = doc["products"].as<JsonArray>();
    for (JsonVariant product : productsArray) {
      html += "<tr>";
      html += "<td>" + product["name"].as<String>() + "</td>";
      html += "<td>" + String(product["quantity"].as<int>()) + "</td>";
      html += "<td>$" + String(product["price"].as<float>(), 2) + "</td>";
      float productTotal = product["quantity"].as<int>() * product["price"].as<float>();
      html += "<td>$" + String(productTotal, 2) + "</td>";
      html += "</tr>";
      totalAmount += productTotal;
    }

    html += "</tbody></table>";

    html += "<div class='invoice-total'>";
    html += "<div><p><strong>Subtotal:</strong> <span>$" + String(totalAmount, 2) + "</span></p></div>";
    html += "<div><p><strong>GST (18%):</strong> <span>$" + String(totalAmount * 0.18, 2) + "</span></p></div>";
    html += "<div><p><strong>Total:</strong> <span>$" + String(totalAmount * 1.18, 2) + "</span></p></div>";
    html += "</div>";

    html += "<div class='invoice-footer'>";
    html += "<a href='#' class='btn btn-print' onclick='window.print()'>Print</a>";
    html += "<a href='/' class='btn btn-back'>Back to Home</a>";
    html += "<p>Thank you for your business!</p>";
    html += "</div>";
  }

  html += "</div></body></html>";

  server.send(200, "text/html", html);

  // Clear the bill details after displaying the bill
  billDetails = "";
}
