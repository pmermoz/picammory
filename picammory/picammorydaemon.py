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

import sys
import atexit
import os
import subprocess

#--- 3rd party

#--- Local
from system import daemon

class PicammoryDeamon(daemon.Daemon):
    """Demonize Picammory
For debugging, Picammory can be started like:

    $ python3 picammory.py

But for the onsite automatic start, PicammoryDeamon is used to isolate Picammory.
    $ service picammory start
or
    $ python3 picammorydeamon.py start
    """

    def run(self):
        try:

            log.info(
'''

    ---------------------------------------
    ---  Picammory Deamon Starting  ---
    ---------------------------------------
''')

            #--- Build the C libray... just in case...

            command = ['cd {} ; python3 setup.py build_ext --inplace'.format(os.path.expanduser('~/picammory/picammory/'))]
            subprocess.call(command, shell=True)

            #--- Picammory environment is load after the demonization
            from picammory import Picammory

            picammory = Picammory()
            picammory.run()

        except EnvironmentError as e:
            log.exception("Picammory Deamon failed %d (%s)" % (e.errno, e.strerror))
        except Exception as e:
            log.exception("Picammory Deamon failed")
        finally:
            log.info(
'''

    ---  Picammory Deamon Exit  ---
    -----------------------------------
''')

            sys.exit(1)


    def sigterm(self, signum = None, frame = None):
        log.info('Signal handler called with signal %s', signum)
        super(PicammoryDeamon, self).sigterm(signum, frame)

    def exit(self):
        log.info("Picammory Deamon Exited")
        super(PicammoryDeamon, self).exit()


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

    if len(sys.argv) == 2:

        a_daemon = PicammoryDeamon("/var/log/picammory/picammory.pid")

        arg = sys.argv[1].lower()

        if arg in ('start'):
            log.info('start')
            a_daemon.start()

        elif arg in ('stop'):
            log.info('stop')
            a_daemon.stop()

        elif arg in ('restart'):
            log.info('restart')
            a_daemon.restart()

        elif arg in ('status'):
            if a_daemon.status():
                print('Picammory currently running!')
            else:
                print('Picammory not running!')

        else:
            print("Unknown command")
            sys.exit(2)

        sys.exit(0)

    else:
        print('python', sys.argv[0], 'start|stop|restart|status')
        sys.exit(2)

