#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

#define BUFFER_W 96
#define BUFFER_H 96
#define MAX_BLOBS 100

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

blob blobs[MAX_BLOBS];
unsigned int numBlobs = 0;

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

int rectDistSq(int minX, int minY, int maxX, int maxY, int px, int py)
{
    int dx = MAX(minX - px, MAX(px - maxX, 0));
    int dy = MAX(minY - py, MAX(py - maxY, 0));
    return dx * dx + dy * dy;
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
        printf("Blob %d: (%d, %d) to (%d, %d)\n", i, b->minX, b->minY, b->maxX, b->maxY);
    }
}

#include <stdio.h>

void printAllBlobsToFile(const char* filename, vec2i pupil)
{
    FILE *file = fopen(filename, "w");
    if (file == NULL) {
        perror("Error opening file");
        return;
    }

    for (int i = 0; i < numBlobs; i++)
    {
        blob *b = &blobs[i];
        fprintf(file, "Blob %d: (%d, %d) to (%d, %d)\n", i, b->minX, b->minY, b->maxX, b->maxY);
    }
    fprintf(file, "Pupil: (%d, %d)", pupil.x, pupil.y);

    fclose(file);
}


vec2i getBlobClosestToPoint(int px, int py)
{
    int closestBlobIndex = -1;
    int closestBlobDistance = 1000000;
    for (int i = 0; i < numBlobs; i++)
    {
        blob *b = &blobs[i];
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
        printf("Error: no blobs found\n");
        return (vec2i){0, 0};
    }
    blob *b = &blobs[closestBlobIndex];
    int centerx = (b->minX + b->maxX) / 2;
    int centery = (b->minY + b->maxY) / 2;
    return (vec2i){centerx, centery};
}

vec2i findPupil(uint8_t *buf, int len)
{
    float thresholdProportion = 0.1f;
    static uint8_t lowest = 255;
    static uint8_t highest = 0;
    uint8_t threshold = (uint8_t)(thresholdProportion * (highest - lowest) + lowest);
    int numBlackPixels = 0;
    numBlobs = 0;
    for (int i = 0; i < len; i++)
    {
        uint8_t pixel = buf[i];
        if (pixel < lowest)
        {
            lowest = pixel;
        }
        if (pixel > highest)
        {
            highest = pixel;
        }
        bool black = pixel <= threshold ? 1 : 0;
        if (black) // this black pixel is potentially part of the pupil.
        {
            int x = i % BUFFER_W;
            int y = i / BUFFER_W;
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
    printf("%d black pixels found\n", numBlackPixels);
    printf("Lowest: %d, highest: %d, threshold: %d\n", lowest, highest, threshold);

    return getBlobClosestToPoint(BUFFER_W / 2, BUFFER_H / 2);
}

int main()
{
    printf("Hello World\n");
    FILE *file = fopen("buffer.bin", "rb");
    if (file == NULL)
    {
        printf("Failed to open file\n");
        return 1;
    }
    uint8_t buffer[BUFFER_W * BUFFER_H];
    size_t numBytesRead = fread(buffer, sizeof(buffer[0]), BUFFER_W * BUFFER_H, file);
    if (numBytesRead != BUFFER_W * BUFFER_H)
    {
        printf("Failed to read file\n");
        return 1;
    }

    findPupil(buffer, BUFFER_W * BUFFER_H); // let the threshold do its thing
    vec2i result = findPupil(buffer, BUFFER_W * BUFFER_H);
    printf("Pupil is at %d, %d\n", result.x, result.y);
    printf("%d blobs found\n", numBlobs);
    
    printAllBlobs();
    printAllBlobsToFile("blobs.txt", result);

    fclose(file);
    return 0;
}
