//
//    PiCammory
//    Copyright (C) 2014 Pascal Mermoz <pascal@mermoz.net>.
//
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License along
//    with this program; if not, write to the Free Software Foundation, Inc.,
//    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
//
//  motionservice.c
//  
//
//  Created by dev on 4/21/14.
//
//

// gettimeofday
#include <sys/time.h>
// localtime
#include <time.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "motionservice.h"

static int imageWidth;
static int imageHeight;
static int imageSize;

//------------------------
//--- YMI
//------------------------

static const int YMI_MAJOR_VERSION = 2;
static const int YMI_MINOR_VERSION = 0;

//------------------------
//--- Difference image
//------------------------

// Luminance sensitivity
// Color change below this limit are considers noise and not used to detect movement
unsigned int differenceImageSensitivity = 20;

static unsigned char* differenceImageDebug;

const unsigned char* getDifferenceImage() {
    return differenceImageDebug;
}

//------------------------
//--- Moving Average image
//------------------------

unsigned int movingAverageImageCoefficient = 10;

static float* movingAverageImage;

const float* getMovingAverageImage() {
    return movingAverageImage;
}

//------------------------
//--- Moving Average Mask
//------------------------

char movingAverageMaskUse = 1;

unsigned int movingAverageMaskValueUp = 5;
unsigned int movingAverageMaskValueDown = 1;
unsigned int movingAverageMaskValueThreshold = 80;

// Luminance sensitivity
static const int movingAverageMaskSensitivity = 10;

static unsigned char* movingAverageMask;

const unsigned char* getMovingAverageMask() {
    return movingAverageMask;
}

int movingAverageMaskPixelValueMax = 0;
int movingAverageMaskPixelValueMin = 255;

//------------------------
//--- Surface
//------------------------

typedef struct {
    int alias;
    int rootAlias;
    // Surface size
    int size;
    int upperLeftX;
    int upperLeftY;
    int bottomRightX;
    int bottomRightY;

    int detectedSurface;
} SurfaceInfo;

// Sensitivity - Size of the surface that will trigget detection
static const int surfaceSizeSensitivity = 50;

// surface index are stored inside a short (surface_buffer0, surface_buffer1), 256 is a little to small, and 65k is way to big...
#define MAX_SURFACE ((1<<16)-1)
static int surfaceCount = 0;
static int percentOfUsableSurface = 0;
static SurfaceInfo surfaceInfoList[MAX_SURFACE+1];

static int surfaceIndexBufferDebugSize = 0;
static short* surfaceIndexBufferDebug = NULL;

//------------------------
//--- Detection
//------------------------

static unsigned char* detectionImage;
int detectionSurfaceAgregationSensitivity = 2;
int detectionCountBeforeReporting = 4;

int detectionCount = 0;
//typedef struct {
//    int centerX;
//    int centerY;
//} DetectionSurface;
//DetectionSurface* detectionSurfaceList = NULL;

//------------------------------------------------------------------------------

static int pixelChangedCount = 0;

static unsigned long currentImagePixelValueAvg = 0;
static unsigned long currentImagePixelValueAverageNoise = 0.;
static int currentImagePixelValueChangesMax = 0;
static int currentImagePixelValueChangesMin = 255;


static unsigned char* motionDetectionImageDebug = NULL;

//------------------------------------------------------------------------------

static int _messageLength = 0;
static int _messageSize = 0;
static char* _message = NULL;

static void messageClear() {
    _messageLength = 0;
}

static void messageAddString(char* inString) {
    int stringLength = strlen(inString);
    int newMessageLength = _messageLength + stringLength;

    if (newMessageLength >= _messageSize) {
        _messageSize = 2 * (newMessageLength + 1);
        _message = realloc(_message, _messageSize);
        strcpy(&_message[_messageLength], inString);
    }
    else {
        strcpy(&_message[_messageLength], inString);
    }
    _messageLength = newMessageLength;
}

static char* message() {
    return _message;
}

//------------------------------------------------------------------------------

// 120 images = 40s @ 3fps = 5.6MB
// 360 images = 2mn @ 3fps = 17MB
// 540 images = 3mn @ 3fps = 25MB
static const int _imageCircularBufferMaxSlotCount = 540;

typedef struct {

    int firstUsedIndex;
    int firstFreeIndex;

    int slotSize;
    char* buffer;

} CircularBuffer;

static CircularBuffer _circularBuffer;

//---

inline static unsigned char* imageBufferAddressOfIndex(int index) {
    return (unsigned char*) (_circularBuffer.buffer + index * _circularBuffer.slotSize);
}

static void imageBufferInitializeCircularBuffer() {

    memset(&_circularBuffer, 0, sizeof(_circularBuffer));

    _circularBuffer.firstUsedIndex = -1;
    _circularBuffer.slotSize = sizeof(Timestamp) + imageSize;

    _circularBuffer.buffer = malloc(_imageCircularBufferMaxSlotCount * _circularBuffer.slotSize);
}

static void imageBufferAddImage(const unsigned char* buffer, char motionDetected) {
    unsigned char* p = imageBufferAddressOfIndex(_circularBuffer.firstFreeIndex);

    if (!_circularBuffer.buffer) {
        return;
    }

    TYImage tyImage;
    memset(&tyImage, 0, sizeof(TYImage));

    // Add a timestamp before the frame
    {
        struct timeval tv;
        if (gettimeofday(&tv, (struct timezone *)NULL) == 0) {

            struct tm* timeinfo = localtime(&tv.tv_sec);

            tyImage.timestamp.year    = timeinfo->tm_year;
            tyImage.timestamp.month   = timeinfo->tm_mon + 1;
            tyImage.timestamp.day     = timeinfo->tm_mday;
            tyImage.timestamp.hour    = timeinfo->tm_hour;
            tyImage.timestamp.minute  = timeinfo->tm_min;
            tyImage.timestamp.second  = timeinfo->tm_sec;
            tyImage.timestamp.msecond = tv.tv_usec / 1000;
            tyImage.timestamp.extra.isdst = timeinfo->tm_isdst;
            if (timeinfo->tm_wday == 0) {
                tyImage.timestamp.extra.wday = 6;
            }
            else {
                tyImage.timestamp.extra.wday = timeinfo->tm_wday - 1;
            }
            tyImage.timestamp.extra.yday = timeinfo->tm_yday;
            tyImage.timestamp.extra.unused = 0;
        }
    }

    tyImage.motionDetected = motionDetected ? 1 : 0;

    // We need to remove the one char allocated for imageBuffer
    memcpy(p, &tyImage, sizeof(TYImage)-1);

    p += sizeof(TYImage)-1;

    memcpy(p, buffer, imageSize);

    char full = _circularBuffer.firstUsedIndex == _circularBuffer.firstFreeIndex;
    if (_circularBuffer.firstUsedIndex == -1) {
        _circularBuffer.firstUsedIndex = _circularBuffer.firstFreeIndex;
    }
    _circularBuffer.firstFreeIndex++;
    if (_circularBuffer.firstFreeIndex >= _imageCircularBufferMaxSlotCount) {
        _circularBuffer.firstFreeIndex = 0;
    }
    if (full) {
        _circularBuffer.firstUsedIndex = _circularBuffer.firstFreeIndex;
    }
}

void imageBufferWriteBuffer(char* filename) {
    if (!_circularBuffer.buffer) {
        return;
    }

    FILE* fp = fopen(filename, "w");

    CircularBufferHeader header;
    memset(&header, 0, sizeof(header));
    strncpy((char*)header.magic, "ymi#", 4);
    header.bitOrder = 0xF01E;
    header.headerSize = sizeof(CircularBufferHeader);
    header.majorVersion = YMI_MAJOR_VERSION;
    header.minorVersion = YMI_MINOR_VERSION;

    header.imageWidth = imageWidth;
    header.imageHeight = imageHeight;

    if (_circularBuffer.firstUsedIndex == -1) {
        // Empty... NOP

        header.imageCount = 0;
        header.payloadSize = 0;
        fwrite(&header, sizeof(header), 1, fp);
    }
    else if (_circularBuffer.firstUsedIndex < _circularBuffer.firstFreeIndex) {
        // [1234    ]
        // [  1234  ]
        // One continue block
        header.imageCount = _circularBuffer.firstFreeIndex - _circularBuffer.firstUsedIndex;
        header.payloadSize = _circularBuffer.slotSize * header.imageCount;
        fwrite(&header, sizeof(header), 1, fp);

        fwrite(imageBufferAddressOfIndex(_circularBuffer.firstUsedIndex), _circularBuffer.slotSize, header.imageCount, fp);
    }
    else if (_circularBuffer.firstFreeIndex == 0) {
        // [    1234]
        // One continue block at the end
        header.imageCount = _imageCircularBufferMaxSlotCount - _circularBuffer.firstUsedIndex;
        header.payloadSize = _circularBuffer.slotSize * header.imageCount;
        fwrite(&header, sizeof(header), 1, fp);

        fwrite(imageBufferAddressOfIndex(_circularBuffer.firstUsedIndex), _circularBuffer.slotSize, header.imageCount, fp);
    }
    else {
        // [34    12]
        // Two block, from firstUsedIndex to end of buffer, and start of buffer to firstFreeIndex
        int imageCountPart1 = _imageCircularBufferMaxSlotCount - _circularBuffer.firstUsedIndex;
        int imageCountPart2 =  _circularBuffer.firstFreeIndex;

        header.imageCount = imageCountPart1 + imageCountPart2;
        header.payloadSize = _circularBuffer.slotSize * header.imageCount;
        fwrite(&header, sizeof(header), 1, fp);

        fwrite(imageBufferAddressOfIndex(_circularBuffer.firstUsedIndex), _circularBuffer.slotSize, imageCountPart1, fp);

        fwrite(_circularBuffer.buffer, _circularBuffer.slotSize, imageCountPart2, fp);

        _circularBuffer.firstUsedIndex = -1;
    }

    fclose(fp);
}

//------------------------------------------------------------------------------
//---
//------------------------------------------------------------------------------

// inDebugImageBuffer should be 4*inImageWidth*inImageHeight bytes (CMYK like format)
char setup_motion(int inImageWidth, int inImageHeight, unsigned char* inMotionDetectionImageDebug, int trace) {
    imageWidth = inImageWidth;
    imageHeight = inImageHeight;
    motionDetectionImageDebug = inMotionDetectionImageDebug;

    imageSize = imageWidth * imageHeight;

    movingAverageMask = malloc(imageSize);
    memset(movingAverageMask, 127, imageSize);

    // Float from 0..255
    movingAverageImage = NULL;

    if (trace) {
        imageBufferInitializeCircularBuffer();
    }

    // Debug
    surfaceIndexBufferDebug = NULL;
    differenceImageDebug = NULL;

    //--- Detection

    detectionCount = 0;
//    detectionSurfaceList = malloc(detectionCountBeforeReporting * sizeof(DetectionSurface));

    //---

    return 0;
}

void setup_debug_buffer() {
    surfaceIndexBufferDebugSize = imageSize * sizeof(short);
    surfaceIndexBufferDebug = malloc(surfaceIndexBufferDebugSize);

    differenceImageDebug = malloc(imageSize);

    detectionImage = malloc(imageSize);

}
const short* getSurfaceBuffer() {
    return surfaceIndexBufferDebug;
}

const unsigned char* getDetectionBuffer() {
    return detectionImage;
}

//-----------------------------------------------

char cleanup_motion() {

    if (detectionImage) {
        free(detectionImage);
        detectionImage = NULL;
    }

    if (surfaceIndexBufferDebug) {
        free(surfaceIndexBufferDebug);
        surfaceIndexBufferDebug = NULL;
    }

    if (differenceImageDebug) {
        free(differenceImageDebug);
        differenceImageDebug = NULL;
    }
    if (movingAverageImage) {
        free(movingAverageImage);
        movingAverageImage = NULL;
    }

    motionDetectionImageDebug = NULL;

    free(movingAverageMask);
    movingAverageMask = NULL;

    return 0;
}

static int compareSurface(const void * p1, const void * p2)
{
    const SurfaceInfo* surfaceInfo1 = p1;
    const SurfaceInfo* surfaceInfo2 = p2;

    // <0	The element pointed by p1 goes before the element pointed by p2
    // 0	The element pointed by p1 is equivalent to the element pointed by p2
    // >0	The element pointed by p1 goes after the element pointed by p2
    return surfaceInfo2->size - surfaceInfo1->size;
}

// Luminence image
char detect_motion(const unsigned char* buffer0, const unsigned char* buffer1, int trace) {

    char motionDetected = False;
    pixelChangedCount = 0;

    movingAverageMaskPixelValueMax = 0;
    movingAverageMaskPixelValueMin = 255;

    currentImagePixelValueAvg = 0;
    currentImagePixelValueAverageNoise = 0.;
    currentImagePixelValueChangesMax = 0;
    currentImagePixelValueChangesMin = 255;
    int pixelChangedAtNoiseLevelForHistory = 0;

    //------------------------
    //--- Diference
    //------------------------

    if (differenceImageDebug) {
        memset(differenceImageDebug, 0, imageSize);
    }

    //------------------------
    //--- Historical motion
    //------------------------

    if (!movingAverageImage) {
        // Float from 0..255
        movingAverageImage = malloc(imageSize * sizeof(float));
        for(int p = 0; p < imageSize; ++p) {
            movingAverageImage[p] = buffer0[p];
        }
    }

    //------------------------
    //--- Surface Detection
    //------------------------
    // Value:
    //      0           No change
    //      1...65k     Surface index
    short surface_buffer0[imageWidth];
    short surface_buffer1[imageWidth];
    memset(surface_buffer0, 0, sizeof(surface_buffer0));
    //memset(surface_buffer1, 0, sizeof(surface_buffer1)); // TODO remove line

    if (surfaceIndexBufferDebug) {
        memset(surfaceIndexBufferDebug, 0, surfaceIndexBufferDebugSize);
    }

    surfaceCount = 0;
    // just to be safe...
    memset(surfaceInfoList, 0, sizeof(surfaceInfoList));

    //------------------------
    //---
    //------------------------

    int p = 0;
    int p3 = 0;
    for(int y = 0; y < imageHeight; ++y) {
        for(int x = 0; x < imageWidth; ++x, ++p, p3 += 4) {

            //--- MovingAverage

            movingAverageImage[p] += (buffer0[p] - movingAverageImage[p]) / movingAverageImageCoefficient;

            //---

            unsigned char movingAverageImagePixelValue = roundf(movingAverageImage[p]);
            unsigned char currentImagePixelValue    = buffer1[p];
            unsigned char currentImagePixelValueChanges = abs(currentImagePixelValue - movingAverageImagePixelValue);

            //--- DifferenceImage

            if (differenceImageDebug) {
                differenceImageDebug[p] = currentImagePixelValueChanges;
            }

            //---

            currentImagePixelValueAvg += currentImagePixelValue;
            if (currentImagePixelValueChanges < currentImagePixelValueChangesMin) {
                currentImagePixelValueChangesMin = currentImagePixelValueChanges;
            }
            if (currentImagePixelValueChanges > currentImagePixelValueChangesMax) {
                currentImagePixelValueChangesMax = currentImagePixelValueChanges;
            }
            currentImagePixelValueAverageNoise += currentImagePixelValueChanges;


            char changedPixelBasedOnLowNoise = False;
            char changedPixelBasedOnNoise = False;
            char changedPixel = False;

            //------------------------
            //--- Remove pixel based on historical motion noise
            //------------------------

            if (movingAverageMaskUse) {
                int movingAverageMaskPixelValue = movingAverageMask[p];
                if (currentImagePixelValueChanges > movingAverageMaskSensitivity || currentImagePixelValueChanges > differenceImageSensitivity) {
                    //log.info("[%d,%d] change hist=%d" % (x, y, self.historicalImageData[x][y]))
                    changedPixelBasedOnLowNoise = True;
                    pixelChangedAtNoiseLevelForHistory += 1;
                    if (movingAverageMaskPixelValue < 255) {
                        movingAverageMaskPixelValue += movingAverageMaskValueUp;
                        if (movingAverageMaskPixelValue > 255) {
                            movingAverageMaskPixelValue = 255;
                        }
                        movingAverageMask[p] = movingAverageMaskPixelValue;
                    }
                    if (currentImagePixelValueChanges >= differenceImageSensitivity) {
                        changedPixelBasedOnNoise = True;
                        if (movingAverageMaskPixelValue < movingAverageMaskValueThreshold) {
                            pixelChangedCount += 1;
                            changedPixel = True;
                        }
                    }
                } else {
                    if (movingAverageMaskPixelValue > 0) {
                        movingAverageMaskPixelValue -= movingAverageMaskValueDown;
                        movingAverageMask[p] = movingAverageMaskPixelValue;
                    }
                }

                if (movingAverageMaskPixelValueMin > movingAverageMaskPixelValue) {
                    movingAverageMaskPixelValueMin = movingAverageMaskPixelValue;
                } else if (movingAverageMaskPixelValueMax < movingAverageMaskPixelValue) {
                    movingAverageMaskPixelValueMax = movingAverageMaskPixelValue;
                }
            }
            else {

                if (currentImagePixelValueChanges >= differenceImageSensitivity) {
                    pixelChangedCount += 1;
                    changedPixel = True;
                }
            }

            //------------------------
            //--- Surface Detection
            //------------------------
            if (changedPixel) {
                // Surface Index goes from 1..n, 0 is for empty.

                int topSurfaceIndex = surface_buffer0[x];
                //--- If a surface on the top exist, them get the root alias (themself, or the associate main surface)
                if (topSurfaceIndex != 0) {
                    topSurfaceIndex = surfaceInfoList[topSurfaceIndex].rootAlias;
                }
                int leftSurfaceIndex;
                if (x > 0) {
                    leftSurfaceIndex = surface_buffer1[x-1];
                } else {
                    leftSurfaceIndex = 0;
                }
                if (leftSurfaceIndex != 0) {
                    leftSurfaceIndex = surfaceInfoList[leftSurfaceIndex].rootAlias;
                }
                int surfaceIndex = 0;
                if (topSurfaceIndex == 0) {
                    if (leftSurfaceIndex == 0) {
                        if (surfaceCount >= MAX_SURFACE) {
                            printf("*** MAX_SURFACE reached ***\n");
                            // Trash it...
                            // Surface 0 will include the som of trashed surface size
                            //surfaceIndex = 0;
                            continue;
                        }
                        else {
                            // surfaceIndex == surfaceCount
                            surfaceIndex = ++surfaceCount;
                        }
                        surfaceInfoList[surfaceIndex].alias = surfaceIndex;
                        surfaceInfoList[surfaceIndex].rootAlias = surfaceIndex;
                        surfaceInfoList[surfaceIndex].size = 0;
                        surfaceInfoList[surfaceIndex].upperLeftX = x;
                        surfaceInfoList[surfaceIndex].upperLeftY = y;
                        surfaceInfoList[surfaceIndex].bottomRightX = x;
                        surfaceInfoList[surfaceIndex].bottomRightY = y;
                    } else {
                        surfaceIndex = leftSurfaceIndex;
                    }
                } else {
                    if (leftSurfaceIndex == 0) {
                        surfaceIndex = topSurfaceIndex;
                    } else {
                        if (topSurfaceIndex == leftSurfaceIndex) {
                            surfaceIndex = topSurfaceIndex; // or leftSurfaceIndex, same value
                        } else {
                            //--- Merge to surface
                            int surfaceIndexToMerge = 0;
                            if (topSurfaceIndex < leftSurfaceIndex) {
                                surfaceIndex = topSurfaceIndex;
                                surfaceIndexToMerge = leftSurfaceIndex;
                            } else {
                                surfaceIndex = leftSurfaceIndex;
                                surfaceIndexToMerge = topSurfaceIndex;
                            }
                            if (surfaceIndexToMerge > 0) {
                                surfaceInfoList[surfaceIndex].size += surfaceInfoList[surfaceIndexToMerge].size;
                                surfaceInfoList[surfaceIndexToMerge].size = 0;
                                //surfaceInfoList[surfaceIndexToMerge].alias = surfaceIndex;
                                surfaceInfoList[surfaceIndexToMerge].rootAlias = surfaceIndex;

                                if (surfaceInfoList[surfaceIndex].upperLeftX > surfaceInfoList[surfaceIndexToMerge].upperLeftX) {
                                    surfaceInfoList[surfaceIndex].upperLeftX = surfaceInfoList[surfaceIndexToMerge].upperLeftX;
                                }
                                if (surfaceInfoList[surfaceIndex].upperLeftY > surfaceInfoList[surfaceIndexToMerge].upperLeftY) {
                                    surfaceInfoList[surfaceIndex].upperLeftY = surfaceInfoList[surfaceIndexToMerge].upperLeftY;
                                }
                                if (surfaceInfoList[surfaceIndex].bottomRightX < surfaceInfoList[surfaceIndexToMerge].bottomRightX) {
                                    surfaceInfoList[surfaceIndex].bottomRightX = surfaceInfoList[surfaceIndexToMerge].bottomRightX;
                                }
                                if (surfaceInfoList[surfaceIndex].bottomRightY < surfaceInfoList[surfaceIndexToMerge].bottomRightY) {
                                    surfaceInfoList[surfaceIndex].bottomRightY = surfaceInfoList[surfaceIndexToMerge].bottomRightY;
                                }
                            }
                        }
                    }
                }

                surface_buffer1[x] = surfaceIndex;

                if (surfaceIndexBufferDebug) {
                    surfaceIndexBufferDebug[p] = surfaceIndex;
                }

                surfaceInfoList[surfaceIndex].size++;

                if (surfaceInfoList[surfaceIndex].upperLeftX > x) {
                    surfaceInfoList[surfaceIndex].upperLeftX = x;
                }
                else if (surfaceInfoList[surfaceIndex].bottomRightX < x) {
                    surfaceInfoList[surfaceIndex].bottomRightX = x;
                }
                if (surfaceInfoList[surfaceIndex].upperLeftY > y) {
                    surfaceInfoList[surfaceIndex].upperLeftY = y;
                }
                else if (surfaceInfoList[surfaceIndex].bottomRightY < y) {
                    surfaceInfoList[surfaceIndex].bottomRightY = y;
                }
            }
            else {
                surface_buffer1[x] = 0;
            }

            //------------------------
            //--- Keep image debug context
            //------------------------

            if (motionDetectionImageDebug != NULL) {
                motionDetectionImageDebug[p3+0] = movingAverageMask[p];
                motionDetectionImageDebug[p3+1] = movingAverageImagePixelValue;
                motionDetectionImageDebug[p3+2] = currentImagePixelValue;
                if (changedPixel) {
                    motionDetectionImageDebug[p3+3] = 255;
                }
                else if (changedPixelBasedOnNoise) {
                    motionDetectionImageDebug[p3+3] = 127;
                }
                else if (changedPixelBasedOnLowNoise) {
                    motionDetectionImageDebug[p3+3] = 63;
                }
                else {
                    motionDetectionImageDebug[p3+3] = 0;
                }
            }
        }

        //------------------------
        //--- Surface Detection
        //------------------------
        memcpy(surface_buffer0, surface_buffer1, sizeof(surface_buffer0));

        //------------------------
    }

    if (pixelChangedCount > imageSize*0.9) {
        // Camera Shake!
        surfaceCount = 0;
    }

    //------------------------
    //--- Surface Detection
    //------------------------

    //int biggerSurfaceSize = 0;
    //for (int surfaceIndex = 1; surfaceIndex <= surfaceCount; ++surfaceIndex) {
    //    if (surfaceInfoList[surfaceIndex].size > biggerSurfaceSize) {
    //        biggerSurfaceSize = surfaceInfoList[surfaceIndex].size;
    //    }
    //}

    if (surfaceCount > 0) {

//        printf("----------------------------------\n");
//        printf("Surfaces: (%d)\n", __LINE__);
//        for(int surfaceIndex = 0; surfaceIndex <= surfaceCount; ++surfaceIndex) {
//            printf("    - #%3d alias=%3d rootAlias=%3d size = %4d\n", surfaceIndex,
//             surfaceInfoList[surfaceIndex].alias,
//             surfaceInfoList[surfaceIndex].rootAlias,
//             surfaceInfoList[surfaceIndex].size);
//        }


        // just to be safe, but should be already 0
        if (surfaceInfoList[0].alias) {
            surfaceInfoList[0].alias = 0;
            surfaceInfoList[0].rootAlias = 0;
        }
        //--- Needed for Debug propose only
        if (surfaceIndexBufferDebug) {

            for(int surfaceIndex = 0; surfaceIndex <= surfaceCount; ++surfaceIndex) {
                //surfaceInfoList[surfaceIndex].rootAlias = surfaceInfoList[surfaceIndex].alias;
                if (surfaceInfoList[surfaceIndex].rootAlias != surfaceIndex) {
                    int rootAlias = surfaceInfoList[surfaceIndex].rootAlias;
                    while(rootAlias != surfaceInfoList[rootAlias].rootAlias) {
                        rootAlias = surfaceInfoList[rootAlias].rootAlias;
                    }
                    surfaceInfoList[surfaceIndex].rootAlias = rootAlias;
                }
            }

            // Alias contain the buffer indice
            // rootAlias contain the lower indice for the surface

//            printf("Surfaces: (%d)\n", __LINE__);
//            for(int surfaceIndex = 0; surfaceIndex <= surfaceCount; ++surfaceIndex) {
//                printf("    - #%3d alias=%3d rootAlias=%3d size = %4d\n", surfaceIndex,
//                 surfaceInfoList[surfaceIndex].alias,
//                 surfaceInfoList[surfaceIndex].rootAlias,
//                 surfaceInfoList[surfaceIndex].size);
//            }

            // process alias from buffer before qsort
            for(int p = 0; p < imageSize; ++p) {
                surfaceIndexBufferDebug[p] = surfaceInfoList[surfaceIndexBufferDebug[p]].rootAlias;
            }


//            printf("Surfaces: (%d)\n", __LINE__);
//            for(int surfaceIndex = 0; surfaceIndex <= surfaceCount; ++surfaceIndex) {
//                printf("    - #%3d alias=%3d rootAlias=%3d size = %4d\n", surfaceIndex,
//                 surfaceInfoList[surfaceIndex].alias,
//                 surfaceInfoList[surfaceIndex].rootAlias,
//                 surfaceInfoList[surfaceIndex].size);
//            }

            for(int p = 0; p < imageSize; ++p) {
                int surfaceId = surfaceIndexBufferDebug[p];
                if (surfaceId > 0) {
                    if (surfaceInfoList[surfaceId].size <= 0) {
                        printf("<!> Surface #%d Size = %d\n", surfaceIndexBufferDebug[p], surfaceInfoList[surfaceId].size);
                    }
                }
            }

        }

        //FIXME We lost connection between index in surface_buffer and surfaceInfoList...
        // We need to sort a table of correcpondance instead, and then use alias to pack and order surfaceIndex

        qsort (&surfaceInfoList[1], surfaceCount, sizeof(SurfaceInfo), compareSurface);

//        printf("Surfaces: (%d)\n", __LINE__);
//        for(int surfaceIndex = 0; surfaceIndex <= surfaceCount; ++surfaceIndex) {
//            printf("    - #%3d alias=%3d rootAlias=%3d size = %4d\n", surfaceIndex,
//             surfaceInfoList[surfaceIndex].alias,
//             surfaceInfoList[surfaceIndex].rootAlias,
//             surfaceInfoList[surfaceIndex].size);
//        }

        // When wind are present or camera shake, a lot of small surface appear creating false positive.
        // We compute the ratio of "usable" surface (surface bigger than sensitivity) compare the smaller "noise" surface
        // If we have less than 10% of usable surface, then no detection.
        // We may miss real detection when windy, but avoid a lot of false positive
        int surfaceSizeBeforeSensitivity = 0;
        int surfaceSizeAfterSensitivity = 0;
        int unfliteredSurfaceCount = surfaceCount;

        // Cleanup Surface and computer stats
        for(int surfaceIndex = 1; surfaceIndex <= unfliteredSurfaceCount; ++surfaceIndex) {

            // Surface are already sorted from larger to smaller, just need to stop the size = 0...
            if (surfaceInfoList[surfaceIndex].size <= 0) {
                surfaceCount = surfaceIndex - 1;
                break;
            }
            else if (surfaceInfoList[surfaceIndex].size >= surfaceSizeSensitivity) {
                surfaceSizeAfterSensitivity += surfaceInfoList[surfaceIndex].size;
            }
            else {
                surfaceSizeBeforeSensitivity += surfaceInfoList[surfaceIndex].size;
            }
        }

//        printf("Surfaces: (%d)\n", __LINE__);
//        for(int surfaceIndex = 0; surfaceIndex <= surfaceCount; ++surfaceIndex) {
//            printf("    - #%3d alias=%3d rootAlias=%3d size = %4d\n", surfaceIndex,
//             surfaceInfoList[surfaceIndex].alias,
//             surfaceInfoList[surfaceIndex].rootAlias,
//             surfaceInfoList[surfaceIndex].size);
//        }



        int totalSurface = surfaceSizeAfterSensitivity + surfaceSizeBeforeSensitivity;
        if (totalSurface > 0) {
            percentOfUsableSurface = (100 * surfaceSizeAfterSensitivity) / totalSurface;
        }
        else {
            percentOfUsableSurface = 0;
        }

        //--- Needed for Debug propose only
        if (surfaceIndexBufferDebug) {
            int fromImageIndiceToInfoList[unfliteredSurfaceCount+1];
            memset(fromImageIndiceToInfoList, 0, (unfliteredSurfaceCount+1)*sizeof(int));
            //fromImageIndiceToInfoList[0] = 0;

            for(int surfaceIndex = 1; surfaceIndex <= surfaceCount; ++surfaceIndex) {
                fromImageIndiceToInfoList[surfaceInfoList[surfaceIndex].alias] = surfaceIndex;
                surfaceInfoList[surfaceIndex].alias = surfaceIndex;
                surfaceInfoList[surfaceIndex].rootAlias = surfaceIndex;
            }
            for(int surfaceIndex = surfaceCount+1; surfaceIndex <= unfliteredSurfaceCount; ++surfaceIndex) {
                int rootSurfaceIndex = fromImageIndiceToInfoList[surfaceInfoList[surfaceIndex].rootAlias];
                fromImageIndiceToInfoList[surfaceInfoList[surfaceIndex].alias] = rootSurfaceIndex;
                surfaceInfoList[surfaceIndex].alias = rootSurfaceIndex;
                surfaceInfoList[surfaceIndex].rootAlias = rootSurfaceIndex;
            }

//            printf("fromImageIndiceToInfoList:\n");
//            for(int surfaceIndex = 1; surfaceIndex <= unfliteredSurfaceCount; ++surfaceIndex) {
//                printf("  - %d -> %d\n", surfaceIndex, fromImageIndiceToInfoList[surfaceIndex]);
//            }

            for(int p = 0; p < imageSize; ++p) {
                surfaceIndexBufferDebug[p] = fromImageIndiceToInfoList[surfaceIndexBufferDebug[p]];
            }

//            printf("Surfaces: (%d)\n", __LINE__);
//            for(int surfaceIndex = 0; surfaceIndex <= surfaceCount; ++surfaceIndex) {
//                printf("    - #%3d alias=%3d size = %4d [%d,%d]-[%d,%d]\n", surfaceIndex,
//                 surfaceInfoList[surfaceIndex].alias,
//                 surfaceInfoList[surfaceIndex].size,
//                 surfaceInfoList[surfaceIndex].upperLeftX,
//                 surfaceInfoList[surfaceIndex].upperLeftY,
//                 surfaceInfoList[surfaceIndex].bottomRightX,
//                 surfaceInfoList[surfaceIndex].bottomRightY
//                 );
//            }
        }
    }
    else {
        percentOfUsableSurface = 0;
    }


    //------------------------
    //--- Final Detection
    //------------------------

    currentImagePixelValueAverageNoise = currentImagePixelValueAverageNoise / imageSize;
    currentImagePixelValueAvg = currentImagePixelValueAvg / imageSize;

//    if (pixelChangedAtNoiseLevelForHistory > (imageWidth*imageHeight)/2) {
//        //log.info("Too many pixel changed, probably just camera shacking, skip... %d" % (pixelChangedAtNoiseLevelForHistory));
//        printf("Too many pixel changed, probably just camera shacking, skip... %d\n", pixelChangedAtNoiseLevelForHistory);
//    } else
//    {
//        // Save an image if pixels changed
//        if (/*biggerSurfaceSize > sensitivity &&*/ percentOfUsableSurface > 90) {
//            motionDetected = True;
//        }
//    }

    if (detectionImage) {
        memset(detectionImage, 0, imageSize);
    }

    if (surfaceCount > 0) {

        // Take the bigger surface and find near small one to attach

//        if (surfaceInfoList[1].size > surfaceSizeSensitivity) {
//            surfaceInfoList[1].detectedSurface = 1;
//        }

        int changed = True;
        int sixBiggerSurfaceCount = 6;
        if (surfaceCount < 6) {
            sixBiggerSurfaceCount = surfaceCount;
        }
        for(int i = 0; i < 2 && changed; ++i) {
            changed = False;
            for(int surfaceIndex1 = 1; surfaceIndex1 <= sixBiggerSurfaceCount; ++surfaceIndex1) {
                if (surfaceInfoList[surfaceIndex1].size == 0) {
                    continue;
                }

                int upperLeftX1   = surfaceInfoList[surfaceIndex1].upperLeftX;
                int upperLeftY1   = surfaceInfoList[surfaceIndex1].upperLeftY;
                int bottomRightX1 = surfaceInfoList[surfaceIndex1].bottomRightX;
                int bottomRightY1 = surfaceInfoList[surfaceIndex1].bottomRightY;
                //if (surfaceInfoList[surfaceIndex1].detectedSurface == 0) {
                //    surfaceInfoList[surfaceIndex1].detectedSurface = surfaceIndex1;
                //}

                for(int surfaceIndex2 = surfaceIndex1+1; surfaceIndex2 <= sixBiggerSurfaceCount; ++surfaceIndex2) {
                //for(int surfaceIndex2 = surfaceIndex1+1; surfaceIndex2 <= surfaceCount; ++surfaceIndex2) {
                    if (surfaceInfoList[surfaceIndex2].size == 0) {
                        continue;
                    }

                    int upperLeftX2   = surfaceInfoList[surfaceIndex2].upperLeftX;
                    int upperLeftY2   = surfaceInfoList[surfaceIndex2].upperLeftY;
                    int bottomRightX2 = surfaceInfoList[surfaceIndex2].bottomRightX;
                    int bottomRightY2 = surfaceInfoList[surfaceIndex2].bottomRightY;

                    //--- See if both surface are fare away

                    // 2 is on the left of 1
                    if (bottomRightX2 < upperLeftX1) {
                        if ((upperLeftX1 - bottomRightX2) > detectionSurfaceAgregationSensitivity) {
                            continue;
                        }
                    }
                    // 1 is on the left of 2
                    if (bottomRightX1 < upperLeftX2) {
                        if ((upperLeftX2 - bottomRightX1) > detectionSurfaceAgregationSensitivity) {
                            continue;
                        }
                    }
                    // 2 is on the top of 1
                    if (bottomRightY2 < upperLeftY1) {
                        if ((upperLeftY1 - bottomRightY2) > detectionSurfaceAgregationSensitivity) {
                            continue;
                        }
                    }
                    // 1 is on the top of 2
                    if (bottomRightY1 < upperLeftY2) {
                        if ((upperLeftY2 - bottomRightY1) > detectionSurfaceAgregationSensitivity) {
                            continue;
                        }
                    }
                    // 1 and 2 are near Enghouth

                    changed = True;


                    // less than 3 pixels surface does not increase root surface bbox (to avoid noise agregation)
                    if (surfaceInfoList[surfaceIndex2].size > 2) {
                        surfaceInfoList[surfaceIndex1].size += surfaceInfoList[surfaceIndex2].size;

                        if (surfaceInfoList[surfaceIndex1].upperLeftX > surfaceInfoList[surfaceIndex2].upperLeftX) {
                            surfaceInfoList[surfaceIndex1].upperLeftX = surfaceInfoList[surfaceIndex2].upperLeftX;
                        }
                        if (surfaceInfoList[surfaceIndex1].bottomRightX < surfaceInfoList[surfaceIndex2].bottomRightX) {
                            surfaceInfoList[surfaceIndex1].bottomRightX = surfaceInfoList[surfaceIndex2].bottomRightX;
                        }
                        if (surfaceInfoList[surfaceIndex1].upperLeftY > surfaceInfoList[surfaceIndex2].upperLeftY) {
                            surfaceInfoList[surfaceIndex1].upperLeftY = surfaceInfoList[surfaceIndex2].upperLeftY;
                        }
                        if (surfaceInfoList[surfaceIndex1].bottomRightY < surfaceInfoList[surfaceIndex2].bottomRightY) {
                            surfaceInfoList[surfaceIndex1].bottomRightY = surfaceInfoList[surfaceIndex2].bottomRightY;
                        }
                    }
                    surfaceInfoList[surfaceIndex2].size = 0;
                    
                    surfaceInfoList[surfaceIndex2].detectedSurface = surfaceIndex1;
                }
            }
            
        }

        for(int surfaceIndex = 1; surfaceIndex < surfaceCount; ++surfaceIndex) {
            if (surfaceInfoList[surfaceIndex].detectedSurface > 0) {
                int rootSurfaceIndex = surfaceIndex;
                while(surfaceInfoList[rootSurfaceIndex].detectedSurface != 0) {
                    rootSurfaceIndex = surfaceInfoList[rootSurfaceIndex].detectedSurface;
                }
                surfaceInfoList[surfaceIndex].detectedSurface = rootSurfaceIndex;
            }
        }

        for(int surfaceIndex = 1; surfaceIndex <= surfaceCount; ++surfaceIndex) {
            if (surfaceInfoList[surfaceIndex].size > surfaceSizeSensitivity) {
                motionDetected = True;
                break;
            }
        }

        if (motionDetected && detectionImage && surfaceIndexBufferDebug) {
            int p = 0;
            int p4 = 0;
            for(int y = 0; y < imageHeight; ++y) {
                for(int x = 0; x < imageWidth; ++x, ++p, p4 += 4) {
                    int surfaceIndex = surfaceIndexBufferDebug[p];
                    if (surfaceInfoList[surfaceIndex].detectedSurface > 0) {
                        surfaceIndex = surfaceInfoList[surfaceIndex].detectedSurface;
                    }
                    if (surfaceInfoList[surfaceIndex].size > surfaceSizeSensitivity) {
                        detectionImage[p] = 255;
                    }
                }
            }
        }
    }
    else {
        motionDetected = False;
    }

    //--------------------------------------------------
    // The problem here is leaf&wind can generate surface detection for few frame (at least 3)
    //--------------------------------------------------

//    if (surfaceInfoList[1].size > 10000) {
//        motionDetected = motionDetected;
//    }


//    //--------------------------------------------------
//    //--- Try to detect motion of center bigger surface
//    //--------------------------------------------------
//
//    if (motionDetected) {
//
//        DetectionSurface* detectionSurface = &detectionSurfaceList[detectionCount];
//        detectionCount++;
//
//        //--- Look for motion, means the detected surface move for at least its own size during the last detectionCount detection
//
//        detectionSurface->centerX = (surfaceInfoList[1].bottomRightX + surfaceInfoList[1].upperLeftX) /2;
//        detectionSurface->centerY = (surfaceInfoList[1].bottomRightY + surfaceInfoList[1].upperLeftY) /2;
//
//        if (detectionCount < detectionCountBeforeReporting) {
//            motionDetected = False;
//        }
//        else {
//            int distance = 0;
//            for(int i = 1; i < detectionCount; ++i) {
//                int dx = detectionSurfaceList[i-1].centerX - detectionSurfaceList[i].centerX;
//                int dy = detectionSurfaceList[i-1].centerY - detectionSurfaceList[i].centerY;
//
//                distance += sqrt(dx * dx + dy * dy);
//            }
//            if (distance > surfaceInfoList[1].size) {
//                motionDetected = True;
//            }
//            else {
//                motionDetected = False;
//                for(int i = 1; i < detectionCount; ++i) {
//                    detectionSurfaceList[i-1] = detectionSurfaceList[i];
//                }
//            }
//            detectionCount--;
//        }
//    }
//    else {
//        if (detectionCount > 0) {
//            for(int i = 1; i < detectionCount; ++i) {
//                detectionSurfaceList[i-1] = detectionSurfaceList[i];
//            }
//            detectionCount--;
//        }
//    }


    //--------------------------------------------------
    //--- If 4 motion surface then upgrade to motion detection
    //--------------------------------------------------

    if (motionDetected) {
        detectionCount++;
        if (detectionCount >= 4) {
            motionDetected = True;
        }
        else {
            motionDetected = False;
        }
    }
    else {
        if (detectionCount > 0) {
            detectionCount -= 2;
        }
    }

    //------------------------
    //--- Archive
    //------------------------

    imageBufferAddImage(buffer1, motionDetected);

    //------------------------

    return motionDetected;
}


char* motionMessage() {
    char buffer[500];

    messageClear();
    snprintf(buffer, 500, "Cam Motion Detector: [%d,%d] noiseLevel=%d noiseLevelForHistory=%d sensitivity=%d\n", imageWidth, imageHeight, differenceImageSensitivity, movingAverageMaskSensitivity, surfaceSizeSensitivity);
    messageAddString(buffer);
    snprintf(buffer, 500, "Historical [%d, %d], changedPixels %d Noise [%d < %d < %d] PixelValueAvg %ld\n", movingAverageMaskPixelValueMin, movingAverageMaskPixelValueMax, pixelChangedCount, (int)currentImagePixelValueChangesMin, (int)currentImagePixelValueAverageNoise, (int)currentImagePixelValueChangesMax, currentImagePixelValueAvg);
    messageAddString(buffer);
    //printf("Motion capture %.0f ms | processing %.0f ms | debug %.0f ms\n", 1000*time1, 1000*time2, 1000*time3);
    //printf("readbuffer %.0f ms | history %.0f ms | surface %.0f ms\n", 1000*timeStart2_readbufferTotal, 1000*timeStart2_historyTotal, 1000*timeStart2_surfaceTotal);
    snprintf(buffer, 500, "Surface (Total:%d, Usable:%d%%):\n", surfaceCount, percentOfUsableSurface);
    messageAddString(buffer);
    for(int surfaceIndex = 1;
        surfaceIndex <= surfaceCount && surfaceInfoList[surfaceIndex].size > 2;
        surfaceIndex++) {

        snprintf(buffer, 500, "\t- %d TL(%d,%d) BR(%d,%d)\n", surfaceInfoList[surfaceIndex].size,
            surfaceInfoList[surfaceIndex].upperLeftX,
            surfaceInfoList[surfaceIndex].upperLeftY,
            surfaceInfoList[surfaceIndex].bottomRightX,
            surfaceInfoList[surfaceIndex].bottomRightY
        );
        messageAddString(buffer);
    }
    return message();
}
