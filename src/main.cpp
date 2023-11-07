#include <Arduino.h>
#include <esp_camera.h>

#define PWDN_GPIO_NUM 32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 0
#define SIOD_GPIO_NUM 26
#define SIOC_GPIO_NUM 27
#define Y9_GPIO_NUM 35
#define Y8_GPIO_NUM 34
#define Y7_GPIO_NUM 39
#define Y6_GPIO_NUM 36
#define Y5_GPIO_NUM 21
#define Y4_GPIO_NUM 19
#define Y3_GPIO_NUM 18
#define Y2_GPIO_NUM 5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM 23
#define PCLK_GPIO_NUM 22
#define LED_PIN 33   // Status led
#define LED_ON LOW   // - Pin is inverted.
#define LED_OFF HIGH //
#define LAMP_PIN 4   // LED FloodLamp.

#define THRESHOLD_BUFFER_W 96
#define THRESHOLD_BUFFER_H 96
#define THRESHOLD_BUFFER_LEN ((THRESHOLD_BUFFER_W * THRESHOLD_BUFFER_H) / 8)

camera_config_t camConfig = {
    .pin_pwdn = PWDN_GPIO_NUM,
    .pin_reset = RESET_GPIO_NUM,
    .pin_xclk = XCLK_GPIO_NUM,
    .pin_sscb_sda = SIOD_GPIO_NUM,
    .pin_sscb_scl = SIOC_GPIO_NUM,
    .pin_d7 = Y9_GPIO_NUM,
    .pin_d6 = Y8_GPIO_NUM,
    .pin_d5 = Y7_GPIO_NUM,
    .pin_d4 = Y6_GPIO_NUM,
    .pin_d3 = Y5_GPIO_NUM,
    .pin_d2 = Y4_GPIO_NUM,
    .pin_d1 = Y3_GPIO_NUM,
    .pin_d0 = Y2_GPIO_NUM,
    .pin_vsync = VSYNC_GPIO_NUM,
    .pin_href = HREF_GPIO_NUM,
    .pin_pclk = PCLK_GPIO_NUM,

    .xclk_freq_hz = 20000000,
    .ledc_timer = LEDC_TIMER_0,
    .ledc_channel = LEDC_CHANNEL_0,
    .pixel_format = PIXFORMAT_GRAYSCALE,
    .frame_size = FRAMESIZE_96X96,
    .jpeg_quality = 15,
    .fb_count = 2,
    .fb_location = CAMERA_FB_IN_PSRAM,
    .grab_mode = CAMERA_GRAB_LATEST};

int flashPin = 4;
int t = 0;
bool continuousTracking = false;
float thresholdProportion = 0.1f; // 0.0 to 1.0
// For storing the thesholded image.
// 1 bit per pixel. Each byte is 8 pixels wide.
uint8_t thresholdBuffer[THRESHOLD_BUFFER_LEN];

void errorLoop()
{
    Serial.printf("Unrecoverable! ");
    delay(500);
}

void printBuffer(uint8_t *buf, size_t len)
{
    Serial.printf("\n\nSending %d bytes of image data\n", len);
    for (int i = 0; i < len; i++)
    {
        Serial.printf("%02x ", buf[i]);
    }
    Serial.println();
}

camera_fb_t *capturePic()
{
    // capture a frame
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb)
    {
        Serial.println("THERE IS NO FRAME BUFFER?");
    }

    //Serial.printf("Img info: len: %d w/h: %d %d time: %d format: %u\n\n", fb->len, fb->width, fb->height, fb->timestamp.tv_usec, fb->format);
    // printBuffer(fb->buf, fb->len);
    // esp_camera_fb_return(fb);

    return fb;
}

void blink()
{
    analogWrite(flashPin, 1);
    delay(10);
    analogWrite(flashPin, 0);
    delay(10);
    analogWrite(flashPin, 1);
    delay(300);
    analogWrite(flashPin, 0);
}

void trackEye()
{
    //Serial.println("BEGIN!");
    // capturePic();
    camera_fb_t *camFrameBuffer = capturePic();
    uint8_t lowest = 255;
    uint8_t highest = 0;
    for (int i = 0; i < camFrameBuffer->len; i++)
    {
        uint8_t pixel = camFrameBuffer->buf[i];
        if (pixel < lowest)
        {
            lowest = pixel;
        }
        if (pixel > highest)
        {
            highest = pixel;
        }
    }
    uint8_t threshold = (uint8_t)(thresholdProportion * (highest - lowest) + lowest);
    if (!continuousTracking)
    {
        Serial.printf("Lowest: %u Highest: %u Threshold: %u\n", lowest, highest, threshold);
    }
    // Threshold image and send it
    for (int i = 0; i < camFrameBuffer->len; i++)
    {
        uint8_t pixel = camFrameBuffer->buf[i];

        bool white = pixel > threshold ? 1 : 0;
        // Set the bit in the respective byte
        int byteIndex = i / 8;
        int bitIndex = i % 8;
        if (white)
        {
            thresholdBuffer[byteIndex] |= (1 << (7 - bitIndex));
        }
        else
        {
            thresholdBuffer[byteIndex] &= ~(1 << (7 - bitIndex));
        }
    }
    // Find average location of black pixels
    int xSum = 0;
    int ySum = 0;
    int numBlackPixels = 0;
    for (int y = 0; y < THRESHOLD_BUFFER_H; y++)
    {
        for (int x = 0; x < THRESHOLD_BUFFER_W; x++)
        {
            int byteIndex = (y * THRESHOLD_BUFFER_W + x) / 8;
            int bitIndex = (y * THRESHOLD_BUFFER_W + x) % 8;
            bool black = (thresholdBuffer[byteIndex] & (1 << (7 - bitIndex))) == 0;
            if (black)
            {
                xSum += x;
                ySum += y;
                numBlackPixels++;
            }
        }
    }
    int xAvg = xSum / numBlackPixels;
    int yAvg = ySum / numBlackPixels;
    Serial.print("X_Location:");
    Serial.print(xAvg);
    Serial.print(",");
    Serial.print("Y_Location:");
    Serial.println(yAvg);

    // Serial.printf("Sending 1 byte per pix\n");
    // printBuffer(camFrameBuffer->buf, camFrameBuffer->len);

    // Serial.printf("Sending 1 bit per pix %ux%u img\n", THRESHOLD_BUFFER_W, THRESHOLD_BUFFER_H);
    // printBuffer(thresholdBuffer, THRESHOLD_BUFFER_LEN);

    //Serial.println("DONE!");
    // Done with the camera image, so return the frame buffer back to be reused
    esp_camera_fb_return(camFrameBuffer);
}

void setupCam()
{
    esp_err_t err = esp_camera_init(&camConfig);
    if (err != ESP_OK)
    {
        delay(100); // need a delay here or the next serial o/p gets missed
        Serial.printf("\r\n\r\nCRITICAL FAILURE: Camera sensor failed to initialize.\r\n\r\n");
        errorLoop();
    }
    Serial.println("Camera init succeeded");
}

void setup()
{
    pinMode(flashPin, OUTPUT);
    Serial.begin(115200);
    setupCam();
    analogWrite(flashPin, 1);
}

void loop()
{
    t++;
    if (Serial.available() > 0)
    {
        char receivedChar = Serial.read();
        Serial.println(">>>> GOT A CHAR!");
        switch (receivedChar)
        {
        case 'p':
            analogWrite(flashPin, 6);
            trackEye();
            analogWrite(flashPin, 0);
            break;
        case 'g': // g for GO
            continuousTracking = true;
            break;
        case 's': // s for STOP
            continuousTracking = false;
            break;
        }
    }
    if (continuousTracking)
    {
        trackEye();
    }
    else
    {
        Serial.printf("hello: %d\n", t);
        blink();
    }
    //blink();
    //delay(10);
}