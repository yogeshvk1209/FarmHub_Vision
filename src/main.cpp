#include "esp_camera.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <base64.h>
#include "secrets.h" // Ensure WIFI_SSID, WIFI_PASS, and api_host are here

// ==========================================
// AI-THINKER CAMERA PIN DEFINITIONS
// ==========================================
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// ==========================================
// CONFIGURATION
// ==========================================
const char* api_host = "7v51v462zk.execute-api.ap-south-1.amazonaws.com";
const char* api_path = "/prod/upload";

// Prototypes
bool initCamera();
void uploadToAPIGateway(String base64Data);

void setup() {
    // 1. Start Serial for Debugging
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n--- FarmHub V8.2: Starting Boot Sequence ---");

    // 2. Hardware Priority: Init Camera first while system is quiet
    if (!initCamera()) {
        Serial.println("Camera Init Failed! Restarting...");
        delay(5000);
        ESP.restart();
    }
    Serial.println("Camera Hardware Initialized.");

    // 3. Network: Connect with Google DNS to bypass 4G DNS issues
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    IPAddress dns(8, 8, 8, 8); 
    WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, dns);

    Serial.print("Connecting to 4G WiFi");
    unsigned long startWait = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startWait < 20000) {
        delay(500);
        Serial.print(".");
    }

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("\nWiFi Failed. Entering recovery sleep...");
        esp_sleep_enable_timer_wakeup(2ULL * 60ULL * 1000000ULL);
        esp_deep_sleep_start();
    }
    Serial.println("\nWiFi Connected.");

    // 4. Capture Phase
    camera_fb_t * fb = esp_camera_fb_get();
    if (!fb) {
        Serial.println("Camera Capture Failed. Restarting...");
        ESP.restart();
    }
    Serial.println("Image Captured.");

    // 5. Encoding Phase
    // Release the camera buffer immediately after encoding to free RAM for SSL
    String base64Image = base64::encode(fb->buf, fb->len);
    esp_camera_fb_return(fb); 
    Serial.printf("Encoded Image Size: %d bytes\n", base64Image.length());

    // 6. Cloud Phase
    uploadToAPIGateway(base64Image);

    // 7. Automation: Deep Sleep 15 Minutes
    Serial.println("Cycle Complete. Entering Deep Sleep.");
    esp_sleep_enable_timer_wakeup(15ULL * 60ULL * 1000000ULL);
    esp_deep_sleep_start();
}

void uploadToAPIGateway(String base64Data) {
    WiFiClientSecure client;
    client.setInsecure();
    client.setTimeout(30000); // Increased to 30s for the slower paced upload

    if (client.connect(api_host, 443)) {
        Serial.println("Connected. Streaming RAW Base64 (Paced)...");

        size_t contentLength = base64Data.length();

        client.print("POST " + String(api_path) + " HTTP/1.1\r\n");
        client.print("Host: " + String(api_host) + "\r\n");
        client.println("Content-Type: text/plain");
        client.print("Content-Length: "); client.println(contentLength);
        client.println("Connection: close");
        client.println(); 
        
        size_t total = base64Data.length();
        size_t chunkSize = 1024;
        for (size_t i = 0; i < total; i += chunkSize) {
            size_t len = (total - i < chunkSize) ? (total - i) : chunkSize;
            client.print(base64Data.substring(i, i + len));
            
            Serial.print("#");
            
            // PACING: Give the 4G router 50ms to breathe
            delay(50); 
            yield(); 
        }
        client.println(); 
        
        Serial.println("\nStream finished. Awaiting Response...");

        unsigned long startResponse = millis();
        while (millis() - startResponse < 20000) {
            while (client.available()) {
                String line = client.readStringUntil('\n');
                Serial.println("AWS >> " + line);
                startResponse = millis();
            }
            if (!client.connected()) break;
        }
    } else {
        Serial.println("API Connection Failed.");
    }
    client.stop();
}

bool initCamera() {
    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;
    config.pin_sccb_sda = SIOD_GPIO_NUM;
    config.pin_sccb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG;

    // Use VGA for better detail
    config.frame_size = FRAMESIZE_VGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;

    esp_err_t err = esp_camera_init(&config);
    return (err == ESP_OK);
}

void loop() {
    // Logic resides in setup for deep sleep stability
}