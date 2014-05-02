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
import configparser

#--- 3rd party

#--- Local


config = None

def config_load(config_filename):
    global config

    config = configparser.ConfigParser()
    config.read([config_filename, os.path.expanduser('~/picammory/' + config_filename)])
    if not config:
        raise ValueError('Unable to read configuration file %s' % config_filename)
