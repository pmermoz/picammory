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
//  motionservice.h
//  
//
//  Created by dev on 4/21/14.
//
//

#ifndef _motionservice_h
#define _motionservice_h

static const int True = 1;
static const int False = 0;

typedef struct {
    unsigned char magic[4];
    unsigned short bitOrder;
    unsigned short headerSize;
    unsigned int payloadSize;
    unsigned char majorVersion;
    unsigned char minorVersion;
    unsigned short imageCount;
    unsigned short imageWidth;
    unsigned short imageHeight;
} CircularBufferHeader;

// Timestamp is Packed. 10 Bytes
// To simplify decoding, instead of the Unix UTC time, we store locatime in a compact time_tm type of structure.
typedef struct {
        unsigned char year;      // years since 1900            (1900-2155)
        unsigned char month;     // months                      1-12
        unsigned char day;       // day of the month            1-31
        unsigned char hour;      // hours since midnight        0-23
        unsigned char minute;    // minutes after the hour      0-59
        unsigned char second;    // seconds after the minute	0-61*
        unsigned short msecond;  // mseconds after the second	0-999
        struct {
            unsigned short isdst:1;    // Daylight Saving Time flag
            unsigned short wday:3;     // days since Monday	0-6
            unsigned short yday:8;     // days since January 1	0-365
            unsigned short unused:4;   // available bits
        } extra;
} Timestamp;

typedef struct {
    Timestamp timestamp;
    char motionDetected :1;             // added v1.2
    char unused :7;
    const unsigned char imageBuffer[];
} TYImage;


//---

void imageBufferWriteBuffer(char* filename);char detect_motion(const unsigned char* buffer1, const unsigned char* buffer2, int trace);

char setup_motion(int inImageWidth, int inImageHeight, unsigned char* inDebugImageBuffer, int trace);

char cleanup_motion();

char detect_motion(const unsigned char* buffer1, const unsigned char* buffer2, int trace);

char* motionMessage();

//---

// The proportion of change to apply on the movingAverageImage
extern unsigned int movingAverageImageCoefficient;
extern unsigned int differenceImageSensitivity;

//--- movingAverageMask

extern char movingAverageMaskUse;
extern unsigned int movingAverageMaskValueUp;
extern unsigned int movingAverageMaskValueDown;
extern unsigned int movingAverageMaskValueThreshold;

extern int movingAverageMaskPixelValueMax;
extern int movingAverageMaskPixelValueMin;

//--- Surface

void setup_debug_buffer();
const short* getSurfaceBuffer();

//--- Detection

const unsigned char* getDetectionBuffer();
extern int detectionSurfaceAgregationSensitivity;

//---

const unsigned char* getDifferenceImage();

// 1 float / pixel, value from 0..255
const float* getMovingAverageImage();

const unsigned char* getMovingAverageMask();


#endif
