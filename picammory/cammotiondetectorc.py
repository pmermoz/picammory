#
#    PiCammory
#    Copyright (C) 2014 Pascal Mermoz <pascal@mermoz.net>.
#
#    This program is free software; you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation; either version 2 of the License, or
#    (at your option) any later version.
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License along
#    with this program; if not, write to the Free Software Foundation, Inc.,
#    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

#--- System
import logging
import logging.handlers
log = logging.getLogger('picammory.' + __name__)

import time
import os
from datetime import datetime
import io
from PIL import Image

#--- Local
from ftpuploader import FtpUploader
from cservice import setup_motion, cleanup_motion, detect_motion, motion_message, write_image_buffer
import config

class CamMotionDetector(object):
    # class variable
    # Sensor 2592x1944
    # Video at (1280, 720)
    # Ration 1.3333
    # 8 * 32 = 128
    _detectionImageWidth = 256
    # 12 * 16 = 96
    _detectionImageHeight = 192

    # Color change below this limit are considers noise and not used to detect movement
    _noiseLevel = 10
    _noiseLevelForHistory = 5

    # Sensitivity - Size of the surface that will trigget detection
    _sensitivity = int((_detectionImageWidth * _detectionImageHeight) / 512)

    #_debugPixelChange = False
    #debugSurface = False
    #_profile_processing = False

    # Debug
    #_counter_debug_dump = 0

    _frame_processed_start_time = 0
    _frame_processed_count = 0
    _frame_processed_fps = 0

    _frame_before_ymi_dump = -1;

    def __init__(self, storagePath, debug=False):

        log.info("Cam Motion Detector: [%d,%d] noiseLevel=%d noiseLevelForHistory=%d sensitivity=%d " % (CamMotionDetector._detectionImageWidth,CamMotionDetector._detectionImageHeight, CamMotionDetector._noiseLevel, CamMotionDetector._noiseLevelForHistory, CamMotionDetector._sensitivity))

        self._storagePath = storagePath

        self._motionDebugPath = storagePath + "Debug/"
        if not os.path.isdir(self._motionDebugPath):
            log.info("Creating photo storage folder %s " % (self._motionDebugPath))
            os.makedirs(self._motionDebugPath)

        self._motionDetectedPrefixFilename = None;

        self._buffer1 = None
        self._buffer2 = None

        self._historicalImageData = [[] for i in range(CamMotionDetector._detectionImageWidth)]
        for x in range(CamMotionDetector._detectionImageWidth):
            for y in range(CamMotionDetector._detectionImageHeight):
                self._historicalImageData[x].append(127)

        #self._debugImageData = bytearray(3 * CamMotionDetector._detectionImageWidth * CamMotionDetector._detectionImageHeight)
        self._debugImageData = bytes(4 * CamMotionDetector._detectionImageWidth * CamMotionDetector._detectionImageHeight)

        setup_motion((CamMotionDetector._detectionImageWidth, CamMotionDetector._detectionImageHeight), self._debugImageData, True)

        CamMotionDetector._frame_processed_count = 0
        CamMotionDetector._frame_processed_start_time = time.time()

    def __enter__(self):
        return self

    def __exit__(self, type, value, traceback):
        cleanup_motion()



    def detect_motion(self, camera, debug_detection):
        motionDetected = False

        timeStart = time.time()

        #---

        if (CamMotionDetector._frame_processed_count >= 10):
            CamMotionDetector._frame_processed_fps = CamMotionDetector._frame_processed_count / (timeStart - CamMotionDetector._frame_processed_start_time)
            CamMotionDetector._frame_processed_count = 0
            CamMotionDetector._frame_processed_start_time = timeStart
        else:
            CamMotionDetector._frame_processed_count += 1

        #---

        # finish to dump previous ymi file before to process next dectection
        if (CamMotionDetector._frame_before_ymi_dump < 0):
            pass
        elif (CamMotionDetector._frame_before_ymi_dump > 0):
            CamMotionDetector._frame_before_ymi_dump -= 1
        else:
            # Dump images used for detection up to detection time

            filename = self._motionDetectedPrefixFilename + ".ymi"
            filepath = self._motionDebugPath + filename
            write_image_buffer(filepath)

            FtpUploader.upload_move(self.remotePath, filename, filepath)

            CamMotionDetector._frame_before_ymi_dump = -1

        #---

        stream = io.BytesIO()
        camera.capture(stream, format='yuv', resize=(CamMotionDetector._detectionImageWidth, CamMotionDetector._detectionImageHeight), use_video_port=True)
        stream.seek(0)

        time1 = time.time() - timeStart
        timeStart2 = time.time()

        if self._buffer1 is None:
            #self._buffer1 = bytearray(stream.getbuffer())
            self._buffer1 = stream.getbuffer()
        else:

            #--- Motion Detection -----------------------------------------------
            # Compare current_image to prior_image to detect motion.

            # Buffer packed 4 byte per pixel, line after line. (RGBA)(RGBA)
            # process on a copy improve performance by 30%. but we cannot modify image buffer anymore
            #self._buffer2 = bytearray(stream.getbuffer())
            self._buffer2 = stream.getbuffer()

            motionDetected = detect_motion(self._buffer1, self._buffer2)
            self._buffer1 = self._buffer2

            time2 = time.time() - timeStart2

            #self._counter_debug_dump += 1

            # Debug when threshold is reached
            #if (motionDetected or self._counter_debug_dump >= 10):
            if (motionDetected and debug_detection):
                #self._counter_debug_dump = 0

                timeStart3 = time.time()

                nowtime = datetime.now()

                camera_name = config.config['camera']['name']

                self._motionDetectedPrefixFilename = "%s-%04d%02d%02d-%02d%02d%02d" % (camera_name, nowtime.year, nowtime.month, nowtime.day, nowtime.hour, nowtime.minute, nowtime.second)
                self.remotePath = ("%04d" % (nowtime.year), "%02d" % (nowtime.month), "%02d/" % (nowtime.day))

                #---
                # Schedule YMI file generation (if not yet scheduled)
                if (CamMotionDetector._frame_before_ymi_dump < 0):
                    CamMotionDetector._frame_before_ymi_dump = 180

                #---

                filename = self._motionDetectedPrefixFilename + ".tiff"
                filepath = self._motionDebugPath + filename
                img = Image.frombuffer("CMYK", (self._detectionImageWidth,self._detectionImageHeight), self._debugImageData, 'raw', "CMYK", 0, 1)
                img.save(filepath)

                FtpUploader.upload_move(self.remotePath, filename, filepath)

                #---

                if (motionDetected):
                    filename = self._motionDetectedPrefixFilename + ".txt"
                else:
                    filename = self._motionDetectedPrefixFilename + "-debug.txt"
                filepath = self._motionDebugPath + filename

                time3 = time.time() - timeStart3

                with open(filepath, "w") as file:
                    file.write("Cam Motion Detector: [%d,%d] noiseLevel=%d noiseLevelForHistory=%d sensitivity=%d\n" % (self._detectionImageWidth,self._detectionImageHeight, CamMotionDetector._noiseLevel, CamMotionDetector._noiseLevelForHistory, CamMotionDetector._sensitivity))
                    file.write("Motion Detection: {:.0f} fps (capture {:.0f} ms | processing {:.0f} ms |  dump {:.0f} ms)\n".format(CamMotionDetector._frame_processed_fps, 1000*time1, 1000*time2, 1000*time3))

                    file.write(motion_message())

                FtpUploader.upload_move(self.remotePath, filename, filepath)

                #---

            #---


        time4 = time.time() - timeStart

        return motionDetected
