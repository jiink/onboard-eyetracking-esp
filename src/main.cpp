#include <Arduino.h>
#include <esp_camera.h>
#include <ArduinoOSCWiFi.h>
#include <Wire.h>
#include "WiFi.h"
#include "AsyncUDP.h"

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

#define WIFI_ENABLE 1

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
#define LED_PIN 33    // Status led
#define LED_ON LOW    // - Pin is inverted.
#define LED_OFF HIGH  //
#define LAMP_PIN 4    // LED FloodLamp.

#define THRESHOLD_BUFFER_W 96
#define THRESHOLD_BUFFER_H 96
#define THRESHOLD_BUFFER_LEN ((THRESHOLD_BUFFER_W * THRESHOLD_BUFFER_H) / 8)
#define MAX_BLOBS 50

typedef struct eyeCal
{
    int minX;
    int minY;
    int maxX;
    int maxY;
} eyeCal;

typedef struct vec2f
{
    float x;
    float y;
} vec2f;

typedef struct vec2i
{
    int x;
    int y;
} vec2i;

typedef struct blob
{
    int minX;
    int minY;
    int maxX;
    int maxY;
} blob;

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
bool sendPhotos = false;
eyeCal calibration = {0, 0, THRESHOLD_BUFFER_W, THRESHOLD_BUFFER_H};
bool calibrating = false;
blob blobs[MAX_BLOBS];
unsigned int numBlobs = 0;
float thresholdProportion = 0.1f; // 0.0 to 1.0
uint8_t lowestPixVal = 255;
uint8_t highestPixVal = 0;
uint8_t prevLowestPixVal = 0;
uint8_t prevHighestPixVal = 255;
const int rollingAvgPupilPosWindowSize = 5;
vec2i rollingAvgPupilPosBuf[rollingAvgPupilPosWindowSize];
unsigned int rollingAvgPupilPosBufIndex = 0;
// For storing the thesholded image.
// 1 bit per pixel. Each byte is 8 pixels wide.
uint8_t thresholdBuffer[THRESHOLD_BUFFER_LEN];

const char * ssid = "WIFI NAME";
const char * password = "PASSWORD";
IPAddress broadcastIp(192, 255, 255, 255);
AsyncUDP udp;
OscWiFiClient client;

int rectDistSq(int minX, int minY, int maxX, int maxY, int px, int py)
{
    int dx = MAX(minX - px, MAX(px - maxX, 0));
    int dy = MAX(minY - py, MAX(py - maxY, 0));
    return dx * dx + dy * dy;
}

void errorLoop()
{
    Serial.printf("Unrecoverable! ");
    analogWrite(flashPin, 5);
    delay(500);
    analogWrite(flashPin, 0);
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

    // Serial.printf("Img info: len: %d w/h: %d %d time: %d format: %u\n\n", fb->len, fb->width, fb->height, fb->timestamp.tv_usec, fb->format);
    //  printBuffer(fb->buf, fb->len);
    //  esp_camera_fb_return(fb);

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

void clearBlobs()
{
    numBlobs = 0;
    for (int i = 0; i < MAX_BLOBS; i++)
    {
        blobs[i].minX = 0;
        blobs[i].minY = 0;
        blobs[i].maxX = 0;
        blobs[i].maxY = 0;
    }
}

void newBlob(int x, int y)
{
    if (numBlobs >= MAX_BLOBS)
    {
        printf("Error: too many blobs\n");
        return;
    }
    blobs[numBlobs].minX = x;
    blobs[numBlobs].minY = y;
    blobs[numBlobs].maxX = x;
    blobs[numBlobs].maxY = y;
    numBlobs++;
}

bool blobIsNear(blob *b, int x, int y)
{
    int centerx = (b->minX + b->maxX) / 2;
    int centery = (b->minY + b->maxY) / 2;
    int distance = rectDistSq(b->minX, b->minY, b->maxX, b->maxY, x, y);
    int maxBlobDistance = 5;
    if (distance < maxBlobDistance * maxBlobDistance)
    {
        return true;
    }
    return false;
}

void addToBlob(blob *b, int x, int y)
{
    b->minX = MIN(b->minX, x);
    b->minY = MIN(b->minY, y);
    b->maxX = MAX(b->maxX, x);
    b->maxY = MAX(b->maxY, y);
}

void printAllBlobs()
{
    for (int i = 0; i < numBlobs; i++)
    {
        blob *b = &blobs[i];
        Serial.printf("Blob %d: (%d, %d) to (%d, %d)\n", i, b->minX, b->minY, b->maxX, b->maxY);
    }
}

vec2i getBlobPosClosestToPoint(int px, int py)
{
    int closestBlobIndex = -1;
    int closestBlobDistance = 1000000;
    for (int i = 0; i < numBlobs; i++)
    {
        blob *b = &blobs[i];
        // Do not consider ones that are too small
        int blobSize = (b->maxX - b->minX) * (b->maxY - b->minY);
        int minBlobSize = 10; // in number of pixels in the rectangle
        if (blobSize <= minBlobSize)
        {
            continue;
        }
        int centerx = (b->minX + b->maxX) / 2;
        int centery = (b->minY + b->maxY) / 2;
        int distanceSq = (centerx - px) * (centerx - px) + (centery - py) * (centery - py);
        if (distanceSq < closestBlobDistance)
        {
            closestBlobDistance = distanceSq;
            closestBlobIndex = i;
        }
    }
    if (closestBlobIndex == -1)
    {
        // printf("Error: no blobs found\n");
        return (vec2i){THRESHOLD_BUFFER_W / 2, THRESHOLD_BUFFER_H / 2};
    }
    blob *b = &blobs[closestBlobIndex];
    int centerx = (b->minX + b->maxX) / 2;
    int centery = (b->minY + b->maxY) / 2;
    return (vec2i){centerx, centery};
}

vec2i findPupil(uint8_t *buf, int len)
{
    float thresholdProportion = 0.1f;
    lowestPixVal = 255;
    highestPixVal = 0;
    uint8_t threshold = (uint8_t)(thresholdProportion * (prevHighestPixVal - prevLowestPixVal) + prevLowestPixVal);
    int numBlackPixels = 0;
    numBlobs = 0;
    for (int i = 0; i < len; i++)
    {
        uint8_t pixel = buf[i];
        if (pixel < lowestPixVal)
        {
            lowestPixVal = pixel;
        }
        if (pixel > highestPixVal)
        {
            highestPixVal = pixel;
        }
        bool black = pixel <= threshold ? 1 : 0;
        if (black) // this black pixel is potentially part of the pupil.
        {
            int x = i % THRESHOLD_BUFFER_W;
            int y = i / THRESHOLD_BUFFER_W;
            bool foundBlob = false;
            for (int j = 0; j < numBlobs; j++)
            {
                blob *b = &blobs[j];
                if (blobIsNear(b, x, y))
                {
                    foundBlob = true;
                    addToBlob(b, x, y);
                    break;
                }
            }
            if (!foundBlob)
            {
                newBlob(x, y);
            }
            numBlackPixels++;
        }
    }
    // printf("%d black pixels found\n", numBlackPixels);
    // printf("Lowest: %d, highest: %d, threshold: %d\n", prevLowestPixVal, prevHighestPixVal, threshold);
    prevLowestPixVal = lowestPixVal;
    prevHighestPixVal = highestPixVal;

    return getBlobPosClosestToPoint(THRESHOLD_BUFFER_W / 2, THRESHOLD_BUFFER_H / 2);
}

float normalize(int number, int min, int max)
{
    if (min == max)
    {
        return 0.0;
    }

    // Calculate the normalized value
    float normalized = (2.0 * (number - min) / (max - min)) - 1.0;

    // Ensure the result is within the valid range [-1, 1]
    if (normalized < -1.0)
    {
        return -1.0;
    }
    else if (normalized > 1.0)
    {
        return 1.0;
    }
    else
    {
        return normalized;
    }
}

vec2i rollingAvgUpdate(vec2i newPupilPos)
{
    if (!(newPupilPos.x == 0 || newPupilPos.y == 0)) // it keeps going in the top left corner just ignore it
    {
        rollingAvgPupilPosBuf[rollingAvgPupilPosBufIndex] = newPupilPos;
        rollingAvgPupilPosBufIndex = (rollingAvgPupilPosBufIndex + 1) % rollingAvgPupilPosWindowSize;
    }
    vec2i sum = {0, 0};
    for (int i = 0; i < rollingAvgPupilPosWindowSize; i++)
    {
        sum.x += rollingAvgPupilPosBuf[i].x;
        sum.y += rollingAvgPupilPosBuf[i].y;
    }
    vec2i avg = {sum.x / rollingAvgPupilPosWindowSize, sum.y / rollingAvgPupilPosWindowSize};
    return avg;
}

vec2f normalizePupilCoords(vec2i pixelCoords, eyeCal calibration)
{
    vec2f result;
    result.x = normalize(pixelCoords.x, calibration.minX, calibration.maxX);
    result.y = normalize(pixelCoords.y, calibration.minY, calibration.maxY);
    return result;
}

// For sending to VRC over OSC
float normToEyeDeg(float norm)
{
    return norm * 16.0f;
}

void trackEye()
{
    // Serial.println("BEGIN!");
    //  capturePic();
    int timeBefore = millis();
    int timeBeforeCapturePic = millis();
    camera_fb_t *camFrameBuffer = capturePic();
    int timeAfterCapturePic = millis();
    int timeBeforeFindPupil = millis();
    vec2i pupilPosPix = rollingAvgUpdate(findPupil(camFrameBuffer->buf, camFrameBuffer->len));
    int timeAfterFindPupil = millis();
    vec2f pupilPosNorm = normalizePupilCoords(pupilPosPix, calibration);
    int timeAfter = millis();
    if (calibrating)
    {
        calibration.minX = MIN(calibration.minX, pupilPosPix.x);
        calibration.maxX = MAX(calibration.maxX, pupilPosPix.x);
        calibration.minY = MIN(calibration.minY, pupilPosPix.y);
        calibration.maxY = MAX(calibration.maxY, pupilPosPix.y);
    }
    Serial.printf("X:%.02f,Y:%.02f,ms:%d %d %d\n", pupilPosNorm.x, pupilPosNorm.y, timeAfter - timeBefore, timeAfterCapturePic - timeBeforeCapturePic, timeAfterFindPupil - timeBeforeFindPupil);
    if (WIFI_ENABLE)
    {
        client.send("192.168.50.219", 9000, "/tracking/eye/LeftRightPitchYaw", normToEyeDeg(pupilPosNorm.y), normToEyeDeg(pupilPosNorm.x), normToEyeDeg(pupilPosNorm.y), normToEyeDeg(pupilPosNorm.x));
    }
    if (!continuousTracking)
    {
        Serial.printf("Lowest: %u Highest: %u\n", prevLowestPixVal, prevHighestPixVal);
        Serial.printf("NumBlobs: %d\n", numBlobs);
        printAllBlobs();
        Serial.printf("Time to capture pic: %d\n", timeAfterCapturePic - timeBeforeCapturePic);
        Serial.printf("Time to find pupil: %d\n", timeAfterFindPupil - timeBeforeFindPupil);
    }
    if (sendPhotos)
    {
        Serial.printf("Sending 1 byte per pix\n");
        printBuffer(camFrameBuffer->buf, camFrameBuffer->len);
        Serial.printf("Sending 1 bit per pix %ux%u img\n", THRESHOLD_BUFFER_W, THRESHOLD_BUFFER_H);
        printBuffer(thresholdBuffer, THRESHOLD_BUFFER_LEN);
    }

    // Serial.println("DONE!");
    //  Done with the camera image, so return the frame buffer back to be reused
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
    if (WIFI_ENABLE) 
    {
        WiFi.mode(WIFI_STA);
        Serial.print("Connecting to WiFi ..");

        WiFi.begin(ssid, password);
        while (WiFi.status() != WL_CONNECTED)
        {
            delay(10);
        }
        if (WiFi.waitForConnectResult() != WL_CONNECTED)
        {
            Serial.println("WiFi Failed");
            errorLoop();
        }
    }
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
        case 'f': // f for FRAME
            trackEye();
            break;
        case 'g': // g for GO
            continuousTracking = true;
            analogWrite(flashPin, 1);
            break;
        case 's': // s for STOP
            continuousTracking = false;
            break;
        case 'c': // c for CALIBRATE
            if (calibrating)
            {
                calibrating = false;
                Serial.printf("CAL DONE\nMIN/MAX_X:%d-%d\nMIN/MAX_Y:%d-%d\n", calibration.minX, calibration.maxX, calibration.minY, calibration.maxY);
            }
            else
            {
                calibrating = true;
                calibration.minX = 47;
                calibration.minY = 47;
                calibration.maxX = 48;
                calibration.maxY = 48;
                Serial.println("CAL STARTED");
            }
            break;
        case 'r': // r for RESET CALIBRATION
            calibration.minX = 0;
            calibration.minY = 0;
            calibration.maxX = THRESHOLD_BUFFER_W;
            calibration.maxY = THRESHOLD_BUFFER_H;
            break;
        case 'p': // p for PHOTO
            sendPhotos = true;
            trackEye();
            sendPhotos = false;
            break;
        default:
            break;
        }
    }
    if (continuousTracking)
    {
        trackEye();
    }
    else
    {
        Serial.printf("hi: %d ", t);
        blink();
    }
    // blink();
    // delay(10);
}