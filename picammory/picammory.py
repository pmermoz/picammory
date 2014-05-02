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
log = logging.getLogger('picammory')

import os
import sys
import time
import subprocess

#--- 3rd party
import wiringpi2

#--- Local
from mailservice import MailService
from ledprocessor import LEDProcessor
from ftpuploader import FtpUploader
from camprocessor import CamProcessor
import config

class Picammory(object):

    def __init__(self):
        pass

    def run(self):

        service_list = []

        try:
            log.info("""

    ----------------------------
    ---  Picammory Starting  ---
    ----------------------------
""")

            os.chdir(os.path.expanduser('~/picammory/picammory/'))
            log.info("os.getcwd() = '%s'", os.getcwd())


            #---

            for gpio_pin in (5, 23, 24, 25):
                subprocess.call(['/usr/local/bin/gpio', 'export', str(gpio_pin), 'out'])

            #/usr/local/bin/gpio export 5 out
            #/usr/local/bin/gpio export 23 out
            #/usr/local/bin/gpio export 24 out
            #/usr/local/bin/gpio export 25 out

            wiringpi2.wiringPiSetupSys()

            #---

            config.config_load('picammory.ini')

            #---

            LEDProcessor.start()
            service_list.append(LEDProcessor)

            LEDProcessor.blue(True)

            #---

            MailService.start()
            service_list.append(MailService)
            MailService.send_message('Picammory', 'Picammory Server Starting')

            LEDProcessor.green(True)

            #---

            FtpUploader.start()
            service_list.append(FtpUploader)

            LEDProcessor.red(True)

            #---

            log.info("Start Cam Processor")
            self.cam_processor = CamProcessor(os.path.expanduser('~/picammory_storage/'))
            service_list.append(self.cam_processor)

            LEDProcessor.blue(False)

            #---

            LEDProcessor.green(False)
            LEDProcessor.red(False)

            #--- Wait until one thread stop...
            stopped_service = None
            while(stopped_service is None):
                for service in service_list:
                    if not service.is_alive():
                        log.info("Service %s is dead", service)
                        stopped_service = service
                        break
                time.sleep(1)

        except EnvironmentError as e:
            log.exception("Picammory failed 1")
            MailService.send_message("Picammory Error", "EnvironmentError: SprinklermoryServer failed: %d (%s)" % (e.errno, e.strerror))
        except Exception as e:
            log.exception("Picammory failed 2")
            MailService.send_message("Picammory Error", "Exception: Scheduler failed: %s" % (e))
        finally:
            for service in service_list:
                service.terminate()
            log.info(
'''

    ------------------------
    ---  Picammory Exit  ---
    ------------------------
''')

        sys.exit(1)


    def exited(self):
        log.info("Picammory Exited")



if __name__ == '__main__':

    #--- Configure logging

    logging.captureWarnings(True)
    log.setLevel(logging.INFO)

    log_path = '/var/log/picammory'
    log_file = log_path + '/picammory.log'

    handler = logging.handlers.TimedRotatingFileHandler(filename = log_file, when = 'MIDNIGHT', backupCount=14)
    formatter = logging.Formatter('%(asctime)s %(threadName)s.%(filename)s.%(funcName)s(%(lineno)d) %(levelname)s %(message)s')
    handler.setFormatter(formatter)
    log.addHandler(handler)

    #---

    picammory = Picammory()
    picammory.run()
