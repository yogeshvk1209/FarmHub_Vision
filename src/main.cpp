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
    
    // 4. Capture Phase (With Active Buffer Flushing)
    Serial.println("Flushing stale overexposed frames from DMA buffer...");
    
    // Snapping 4 quick throwaway frames allows the AEC to step down seamlessly
    for (int i = 0; i < 4; i++) {
        camera_fb_t * throwaway = esp_camera_fb_get();
        if (throwaway) {
            esp_camera_fb_return(throwaway); // Toss it back immediately
            delay(150); // Small pause to let the sensor compute exposure updates
        }
    }
    
    // Now capture the 5th frame, which has the updated exposure settings
    Serial.println("Capturing exposure-optimized frame...");
    camera_fb_t * fb = esp_camera_fb_get();
    if (!fb) {
        Serial.println("Camera Capture Failed. Restarting...");
        ESP.restart();
    }
    Serial.printf("Image Captured. Raw Buffer Size: %d bytes\n", fb->len);

    // 5. Encoding Phase
    // Convert raw frame to Base64 text string and free memory immediately
    String base64Image = base64::encode(fb->buf, fb->len);
    esp_camera_fb_return(fb); 
    Serial.printf("Encoded Image Size: %d bytes\n", base64Image.length());

    // 6. Cloud Phase
    uploadToAPIGateway(base64Image);

    // 7. Automation: Deep Sleep 15 Minutes
    Serial.println("Cycle Complete. Entering Deep Sleep.");
    esp_sleep_enable_timer_wakeup(10ULL * 60ULL * 1000000ULL);
    esp_deep_sleep_start();
}

void uploadToAPIGateway(String base64Data) {
    WiFiClientSecure client;
    client.setInsecure();
    client.setTimeout(30000); 

    // 1. Hardcode your distinct node identity tracking token here
    String myDeviceID = "node_01"; 

    // 2. Build the structured JSON body wrapper sequentially
    // Result: {"deviceId":"node_01","image":"/9j/..."}
    String head = "{\"deviceId\":\"" + myDeviceID + "\",\"image\":\"";
    String tail = "\"}";
    
    String fullHttpBody = head + base64Data + tail;
    size_t totalLength = fullHttpBody.length();

    Serial.println(">>> Connecting to Secure API Gateway...");
    if (client.connect(AWS_API_HOST, 443)) {
        Serial.println(">>> TCP Connected. Sending Standardized HTTP/1.1 Protocol...");

        // Send standard headers matching the absolute calculated string length
        client.print("POST " + String(AWS_API_PATH) + " HTTP/1.1\r\n");
        client.print("Host: " + String(AWS_API_HOST) + "\r\n");
        client.print("x-api-key: " + String(AWS_API_KEY) + "\r\n");
        client.print("Content-Type: application/json\r\n"); 
        client.print("Content-Length: " + String(totalLength) + "\r\n");
        client.print("Connection: close\r\n\r\n");

        Serial.println("Streaming unified JSON body with hardware device ID...");
        
        const char* rawPointer = fullHttpBody.c_str();
        size_t remaining = totalLength;
        size_t chunkSize = 1432; // Your optimized hardware segment packet size
        
        while (remaining > 0) {
            size_t currentChunkSize = (remaining > chunkSize) ? chunkSize : remaining;
            
            client.write((const uint8_t*)rawPointer, currentChunkSize);
            
            rawPointer += currentChunkSize;
            remaining -= currentChunkSize;
            
            Serial.print("#");
            yield(); 
        }
        
        client.flush();
        Serial.println("\n>>> Stream completely finalized. Awaiting Response...");

        // 30-Second Latency Response Catcher
        unsigned long startResponse = millis();
        while (millis() - startResponse < 30000) {
            while (client.available()) {
                String line = client.readStringUntil('\n');
                Serial.println("AWS >> " + line);
                startResponse = millis(); 
            }
            if (!client.connected()) break;
        }
    } else {
        Serial.println(">>> Connection to API Gateway failed.");
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

    // Fixed stable baseline for your SVGA field tests
    config.frame_size = FRAMESIZE_SVGA; 
    config.jpeg_quality = 28;
    config.fb_count = 1; // Keeping it at 1 to save critical SRAM for TLS strings

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        return false;
    }

    // Configure internal hardware registers for harsh sunlight conditions
    sensor_t * s = esp_camera_sensor_get();
    if (s != NULL) {
        s->set_whitebal(s, 1);       // Enable Auto White Balance
        s->set_awb_gain(s, 1);       // Enable Auto White Balance Gain
        s->set_exposure_ctrl(s, 1);  // Enable Auto Exposure Control
        s->set_gain_ctrl(s, 1);      // Enable Auto Gain Control
        
        // Anti-glare optimization parameters
        s->set_brightness(s, -2);    // Drop brightness floor down to minimum (-2)
        s->set_contrast(s, 1);       // Boost contrast to preserve tree foliage definition
        s->set_ae_level(s, -2);      // Target an underexposed metering index (-2 to 2)
    }

    return true;
}

void loop() {
    // Logic resides in setup for deep sleep stability
}