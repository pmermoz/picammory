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
import io
from datetime import datetime
import threading

#--- 3rd party
import picamera

#--- Local
#from ledprocessor import LEDProcessor
from ftpuploader import FtpUploader
from mailservice import MailService
import config

USE_C_ACCELERATION = True

if USE_C_ACCELERATION:
    # C optimised
    from cammotiondetectorc import CamMotionDetector
else:
    # Pure Python
    from cammotiondetector import CamMotionDetector

class CamProcessorServer(threading.Thread):

    def __init__(self, storagePath, debug=False):
        threading.Thread.__init__(self, name='CamDaemon')

        self._storage_path = storagePath
        self._periodic_path = storagePath + "Periodic/"

        if not os.path.isdir(self._storage_path):
            log.info("Creating photo storage folder %s " % (self._storage_path))
            os.makedirs(self._storage_path)
        if not os.path.isdir(self._periodic_path):
            log.info("Creating photo storage folder %s " % (self._periodic_path))
            os.makedirs(self._periodic_path)

        self._terminate = False

        self.daemon = True
        self.start()

    def run(self):
        try:
            log.info("CamProcessorServer Started")

            camera_name = config.config['camera']['name']

            with CamMotionDetector(self._storage_path) as camMotionDetector:

                # Take a shot at boot, them every hour on the hour
                log.info("Start After Boot Picture")
#                LEDProcessor.green(True)
                with picamera.PiCamera() as camera:
                    nowtime = datetime.now()
                    imageFilename = "%s-%04d%02d%02d-%02d%02d%02d-Boot.jpg" % (camera_name, nowtime.year, nowtime.month, nowtime.day, nowtime.hour, nowtime.minute, nowtime.second)
                    imageFilepath = self._periodic_path + imageFilename
                    camera.resolution = (2592, 1944)
#                    camera.hflip = True
#                    camera.vflip = True
                    #camera.exif_tags['EXIF.UserComment'] = b'Garden'
                    camera.start_preview()
                    # Camera warm-up time
                    time.sleep(2)
#                    LEDProcessor.green(False)
                    camera.capture(imageFilepath)
#                    LEDProcessor.green(True)

                    remotePath = ("%04d" % (nowtime.year), "%02d" % (nowtime.month), "%02d" % (nowtime.day))
                    FtpUploader.upload_move(remotePath, imageFilename, imageFilepath)
#                LEDProcessor.green(False)

#                LEDProcessor.red(False)
#                LEDProcessor.blue(False)

                self.lastPictureTime = datetime.utcnow().hour;

                while not self._terminate:

                    log.info("Start Motion detection")

                    with picamera.PiCamera() as camera:

                        #camera.led = False
#                        LEDProcessor.camera(False)

                        camera.resolution = (1280, 720)
#                        camera.hflip = True
#                        camera.vflip = True
                        camera.framerate = 6
                        #camera.video_stabilization = True
                        stream = picamera.PiCameraCircularIO(camera, seconds=5)

                        camera.start_recording(stream, format='h264')

                        try:
                            while not self._terminate:
                                camera.wait_recording(0.2)
                                if camMotionDetector.detect_motion(camera, True):
                                    self.process_motion(camera, stream, camMotionDetector)

                                # 'datetime.utcnow().hour' take about 25 usec
                                # 'time.gmtime()[3]' take about 26 usec
                                utc_now_hour = datetime.utcnow().hour;
                                if (utc_now_hour != self.lastPictureTime):
                                    self.lastPictureTime = utc_now_hour
                                    break
                        except Exception as e:
                            log.exception("Motion detection")
                        finally:
                            camera.stop_recording()

                    if (self._terminate):
                        log.info("CamProcessorServer terminated %d" % (self._terminate))
                        break

                    # Take a shot at boot, them every hour on the hour
                    log.info("Start Periodic Picture")
#                    LEDProcessor.green(True)
                    with picamera.PiCamera() as camera:
                        nowtime = datetime.now()
                        log.info('Periodic Picture')

                        imageFilename = "%s-%04d%02d%02d-%02d0000.jpg" % (camera_name, nowtime.year, nowtime.month, nowtime.day, nowtime.hour)
                        imageFilepath = self._periodic_path + imageFilename
                        camera.resolution = (2592, 1944)
#                        camera.hflip = True
#                        camera.vflip = True
                        #camera.exif_tags['EXIF.UserComment'] = b'Garden'
                        camera.start_preview()
                        # Camera warm-up time
                        time.sleep(1)
#                        LEDProcessor.green(False)
                        camera.capture(imageFilepath)
#                        LEDProcessor.green(True)

                        remotePath = ('Periodic', "%04d" % (nowtime.year), "%02d" % (nowtime.month), "%02d" % (nowtime.day))
                        FtpUploader.upload_move(remotePath, imageFilename, imageFilepath)
#                    LEDProcessor.green(False)


        except EnvironmentError as e:
            log.exception("CamProcessorServer Daemon failed: %d (%s)" % (e.errno, e.strerror))
        except Exception as e:
            log.exception("CamProcessorServer Daemon failed (Exception)")
        finally:
            log.info("CamProcessorServer Daemon Exit.")


    def terminate(self):
        log.info("CamProcessorServer terminate requested")
        self._terminate = True

    def process_motion(self, camera, stream, camMotionDetector):
        # As soon as we detect motion, split the recording to
        # record the frames "after" motion
        detectionTime = datetime.now()
        nowtime = detectionTime
        # To be safe, limit to 60s video
        startDetectionTime = time.time()
        endDetectionTime = startDetectionTime + 60
        camera_name = config.config['camera']['name']

        preFilename = "%s-%04d%02d%02d-%02d%02d%02d" % (camera_name, detectionTime.year, detectionTime.month, detectionTime.day, detectionTime.hour, detectionTime.minute, detectionTime.second)
        beforeFilename = preFilename + "-1.h264"
        afterFilename = preFilename + "-2.h264"
        mergeFilename = preFilename + ".mp4"

        beforeFilepath = self._storage_path + beforeFilename
        afterFilepath = self._storage_path + afterFilename
        mergeFilepath = self._storage_path + mergeFilename

        #log.info( "%s - Saved After Video to %s" % (progname, afterFilename))
        camera.split_recording(afterFilepath)

        #---

        # Always record at least 5s (or 15 frames @ 3fps) of video per event
        # Need to call detect_motion to keep feeding the yim file
        for i in range(15):
            camMotionDetector.detect_motion(camera, False)
            camera.wait_recording(0.2)

        # Wait until motion is no longer detected (+5s or 15 frames @ 3fps), then split
        # recording back to the in-memory circular buffer
        no_motion = 0
        while(no_motion < 15 and time.time() < endDetectionTime):
            if (camMotionDetector.detect_motion(camera, False)):
                no_motion = 0
            else:
                no_motion += 1
            camera.wait_recording(0.2)


        #---

        # Write the entire content of the circular buffer to disk. No need to
        # lock the stream here as we're definitely not writing to it
        # simultaneously
        #print( "%s - Saved Before Video to %s" % (filename))
        with io.open(beforeFilepath, 'wb') as output:
            for frame in stream.frames:
                if frame.header:
                    stream.seek(frame.position)
                    break
            while True:
                buf = stream.read1()
                if not buf:
                    break
                output.write(buf)
        # Wipe the circular stream once we're done
        stream.seek(0)
        stream.truncate()

        log.info('Motion stopped! no_motion=%d, elapseTime=%fs' % (no_motion, (time.time() - startDetectionTime)))
        camera.split_recording(stream)

#        LEDProcessor.red(True)

        remotePath = ("%04d" % (nowtime.year), "%02d" % (nowtime.month), "%02d" % (nowtime.day))
        FtpUploader.merge_convert_and_upload_move(beforeFilepath, afterFilepath, remotePath, mergeFilename, mergeFilepath)

        log.info('Motion detected!')
        #MailService.send_message("Motion detected!", "")

#        LEDProcessor.red(False)


class CamProcessor(object):
    "Singleton handeling camera in background"

    # class variable
    __server = None

    def __init__(self, storagePath, debug=False):
        #log.info("Cam Motion Detector: [%d,%d]" % (CamMotionDetector.detectionImageWidth,CamMotionDetector.detectionImageHeight))
        CamProcessor.__server = CamProcessorServer(storagePath, debug)

    @classmethod
    def is_alive(cls):
        if (cls.__server):
            return cls.__server.is_alive()
        else:
            return False

    @classmethod
    def terminate(cls):
        if (cls.__server):
            cls.__server.terminate()
