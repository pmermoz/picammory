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
import threading
import queue
from email.mime.multipart import MIMEMultipart
from email.mime.image import MIMEImage
from email.mime.text import MIMEText
import smtplib
import configparser

#--- 3rd party

#--- Local
import config

class MailService(object):
    "Singleton handeling mail in background"

    class __MailService(threading.Thread):

        def __init__(self):
            threading.Thread.__init__(self, name='SmtpDaemon')

            self._command_queue = queue.Queue(maxsize = 50)

            self._terminate = False

            self.daemon = True
            self.start()

        def run(self):
            try:
                log.info("MailServiceServer Started")

                while not self._terminate:
                    command = self._command_queue.get()
                    if (self._terminate):
                        log.info("MailServiceServer terminated")
                        break

                    if (command[0] == 'send_image_stream'):
                        self.send_image_stream(command[1], command[2])
                    elif (command[0] == 'send_message'):
                        self.send_message(command[1], command[2])

                    else:
                        log.error("MailServiceServer: unknown command '%s'" % (command))

                    self._command_queue.task_done()

            except EnvironmentError as e:
                log.exception("MailServiceServer Daemon failed: %d (%s)" % (e.errno, e.strerror))
            except Exception as e:
                log.exception("MailServiceServer Daemon failed (Exception)")
            finally:
                log.info("MailServiceServer Daemon Exit.")

        def terminate(self):
            #log.info("MailServiceServer terminate requested")
            self._terminate = True
            self._command_queue.put(None)

        def queue_command(self, command):
            #log.info("MailServiceServer queue_message %s", command)
            self._command_queue.put(command)

        def create_message(self, filename, image_stream):
            config_email = config.config['email']
            if not config_email:
                raise ValueError("Missing 'email' in configuration file")
            camera_name = config.config['camera']['name']

            root_container = MIMEMultipart(_subtype='related')
            root_container['From'] = config_email['sender']
            root_container['To'] = config_email['recipient']
            root_container['Subject'] = 'Motion detected from {}!'.format(camera_name)
            root_container.preamble = 'This is a multi-part message in MIME format.'
            body_container = MIMEMultipart(_subtype='alternative')
            html_body = MIMEText("""
<html>
  <head>
  </head>
  <body>
    <b>Please see the attachment below!</b><br/>
    <br/>
    <img src="cid:%s"/>
  </body>
</html>""" % (filename), _subtype='html')
            text_body = MIMEText("""Please see the attachment named %s.""" % (filename), _subtype='plain')
            image_attachment = MIMEImage(image_stream.read())
            image_attachment.add_header('Content-Id', '<%s>' % (filename))
            # Mark the image as an attachment for plain-text clients
            image_attachment.add_header('Content-Disposition', 'attachment', filename=filename)
            root_container.attach(body_container)
            body_container.attach(text_body)
            body_container.attach(html_body)
            root_container.attach(image_attachment)
            return root_container

        def send_mime_email(self, message):
            config_smtp = config.config['smtp']
            if not config_smtp:
                raise ValueError("Missing 'smtp' in configuration file")

            server = smtplib.SMTP(config_smtp['server'])
            try:
                server.ehlo()
                if config_smtp['usetls']:
                    server.starttls()
                if config_smtp['username']:
                    server.login(config_smtp['username'], config_smtp['password'])
                server.sendmail(
                                message['From'],
                                message['To'],
                                message.as_string())
            except Exception as e:
                log.exception("send_email failed.")
            finally:
                server.quit()

        def send_email(self, subject, message):
            config_smtp = config.config['smtp']
            if not config_smtp:
                raise ValueError("Missing 'smtp' in configuration file %s" % config_filename)

            config_email = config.config['email']
            if not config_email:
                raise ValueError("Missing 'email' in configuration file %s" % config_filename)

            # Set up the individual bits of the message, and their content
            root_container = MIMEText(message)
            root_container['From'] = config_email['sender']
            root_container['To'] = config_email['recipient']
            root_container['Subject'] = subject

            server = smtplib.SMTP(config_smtp['server'])
            try:
                server.ehlo()
                if config_smtp['usetls']:
                    server.starttls()
                if config_smtp['username']:
                    server.login(config_smtp['username'], config_smtp['password'])
                server.sendmail(
                                config_email['sender'],
                                config_email['recipient'],
                                root_container.as_string())
            except Exception as e:
                log.exception("send_email failed.")
            finally:
                server.quit()

        #---

        def send_message(self, subject, message):
            self.send_email(subject, message)

        def send_image_stream(self, filename, image_stream):
            with open(image_stream, 'rb') as stream:
                message = self.create_message(filename, stream)
                self.send_mime_email(message)

    #------------------------------------------

    # class variable

    _instance = None

    @classmethod
    def start(cls):
        cls._instance = MailService.__MailService()


    @classmethod
    def is_alive(cls):
        #log.info("MailService.is_alive %s" % (cls.__server))
        if (cls._instance is not None):
            return cls._instance.is_alive()
        else:
            return False

    @classmethod
    def terminate(cls):
        if (cls._instance is not None):
            cls._instance.terminate()

    @classmethod
    def send_message(cls, subject, message):
        #log.info("MailService.send_message %s" % (cls.__server))
        cls._instance.queue_command(('send_message', subject, message))

    @classmethod
    def send_image_stream(cls, filename, image_stream):
        #log.info("MailService.send_image_stream %s (%s, %s)" % (cls.__server, filename, image_stream))
        cls._instance.queue_command(('send_image_stream', filename, image_stream))


