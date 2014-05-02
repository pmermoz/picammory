#
#    PiCammory
#    Copyright (C) 2013-2014 Pascal Mermoz <pascal@mermoz.net>.
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
#

#--- System
import logging
import logging.handlers
log = logging.getLogger('picammory.' + __name__)

import os
import platform
import subprocess
from time import sleep

def reboot():
    "Reboot the computer"
    log.info("Rebooting System")

    os.system('sudo shutdown -r now')

def shutdown():
    "Shutdown the computer"
    log.info("Shutting System Down Now...")

    os.system('sudo shutdown -h now')


def get_cpu_temperature():
    "Return CPU temperature as a character string"
    output = os.popen('vcgencmd measure_temp').readline()
    #return(output.replace("temp=","").replace("'C\n",""))
    return float(output[output.index('=') + 1:output.rindex("'")])

def get_clock_arm():
    "Return Clock ARM frequency in Hz as a character string"
    output = os.popen('vcgencmd measure_clock arm').readline()
    return float(output[output.index('=') + 1:])

def get_computer_name():
     return platform.node()
