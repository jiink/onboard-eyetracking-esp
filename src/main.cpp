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

void errorLoop()
{
    Serial.printf("Unrecoverable! ");
    delay(500);
}

void captureAndSendPic()
{
    //capture a frame
    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) {
        Serial.println("Frame buffer could not be acquired");
    }

    Serial.printf("Img info: len: %d w/h: %d %d time: %d format: %u\n\n", fb->len, fb->width, fb->height, fb->timestamp.tv_usec, fb->format);

    for (int i = 0; i < fb->len; i++)
    {
        Serial.printf("%02x ", fb->buf[i]);
    }
    Serial.println();

    //return the frame buffer back to be reused
    esp_camera_fb_return(fb);
}

void blink()
{
    digitalWrite(flashPin, HIGH);
    delay(20);
    digitalWrite(flashPin, LOW);
}

void setupCam() {
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
}

void loop()
{
    t++;
    Serial.printf("hello: %d\n", t);
    captureAndSendPic();
    blink();
    delay(1000);
}