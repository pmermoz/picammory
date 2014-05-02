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

import os
import subprocess
import threading
import queue
import ftplib
import configparser

#--- 3rd party

#--- Local
#from config import config
import config

class FtpUploader(object):
    "Singleton handeling upload to storage server in background"

    class __FtpUploader(threading.Thread):

        def __init__(self):
            threading.Thread.__init__(self, name='SmtpDaemon')

            self._command_queue = queue.Queue(maxsize = 50)

            self._terminate = False

            self.daemon = True
            self.start()

        def run(self):
            try:
                log.info("FtpUploaderServer Started")

                while not self._terminate:
                    command = self._command_queue.get()
                    if (self._terminate):
                        break

                    if (command[0] == 'upload_move'):
                        self.upload_move(command[1], command[2], command[3])
                    elif (command[0] == 'merge_convert_and_upload_move'):
                        self.merge_convert_and_upload_move(command[1], command[2], command[3], command[4], command[5])
                    else:
                        log.error("FtpUploaderServer: unknown command '%s'" % (command))

                    self._command_queue.task_done()

            except ftplib.all_errors as e:
                log.exception("FtpUploaderServer Daemon failed: %d (%s)" % (e.errno, e.strerror))
            except EnvironmentError as e:
                log.exception("FtpUploaderServer Daemon failed: %d (%s)" % (e.errno, e.strerror))
            except Exception as e:
                log.exception("FtpUploaderServer Daemon failed (Exception)")
            finally:
                log.info("FtpUploaderServer Daemon Exit.")

        def terminate(self):
            log.info("FtpUploaderServer terminate requested")
            self._terminate = True
            self._command_queue.put(None)

        def queue_command(self, command):
            self._command_queue.put(command)

        def upload_move(self, remotepath, fileName, imageFilepath):
            config_ftp = config.config['ftp']
            try:
                with open(imageFilepath, 'rb') as file:
                    session = ftplib.FTP_TLS(config_ftp['server'], config_ftp['username'], config_ftp['password'])
                    session.prot_p()
                    session.cwd("picammory")
                    if (remotepath is not None):
                        for dir in remotepath:
                            try:
                                session.cwd(dir)
                            except ftplib.error_perm as e:
                                session.mkd(dir)
                                session.cwd(dir)

                    session.storbinary('STOR ' + fileName, file)     # send the file
                    session.quit()

                try:
                    os.remove(imageFilepath)
                except OSError as e:
                    log.info("Error: %s - %s." % (e.filename,e.strerror))

            except Exception as e:
                log.exception("FtpUploaderServer upload_move failed")

        def merge_convert_and_upload_move(self, filepath1, filepath2, remotePath, fileName3, filepath3):
            command = ['MP4Box -quiet -add {} -cat {} {} ; rm {} {}'.format(filepath1, filepath2, filepath3, filepath1, filepath2)]
            subprocess.call(command, shell=True)
            self.upload_move(remotePath, fileName3, filepath3)


    # class variable
    _instance = None

    @classmethod
    def start(cls):
        cls._instance = FtpUploader.__FtpUploader()

    @classmethod
    def is_alive(cls):
        if (cls._instance is not None):
            return cls._instance.is_alive()
        else:
            return False

    @classmethod
    def terminate(cls):
        if (cls._instance is not None):
            cls._instance.terminate()

    @classmethod
    def upload_move(cls, remotePath, fileName, imageFilepath):
        cls._instance.queue_command(('upload_move', remotePath, fileName, imageFilepath))

    @classmethod
    def merge_convert_and_upload_move(cls, filepath1, filepath2, remotePath, fileName3, filepath3):
        cls._instance.queue_command(('merge_convert_and_upload_move', filepath1, filepath2, remotePath, fileName3, filepath3))

