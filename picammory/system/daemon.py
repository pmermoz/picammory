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

"""Generic linux daemon base class for python 3.x."""
# Code based from 'http://www.jejik.com/files/examples/daemon3x.py'

import sys
import os
import time
import atexit
import signal

class Daemon(object):
	"""A generic daemon class.

	Usage: subclass the daemon class and override the run() method."""

	def __init__(self, pidfile): self.pidfile = pidfile
	
	def daemonize(self):
		"""Deamonize class. UNIX double fork mechanism."""

		try: 
			pid = os.fork() 
			if pid > 0:
				# exit first parent
				sys.exit(0) 
		except OSError as err: 
			sys.stderr.write('fork #1 failed: {0}\n'.format(err))
			sys.exit(1)
	
		# decouple from parent environment
		os.chdir('/')
		# Stop listening for signals that the parent process receives.
		# This is done by getting a new process id.
		# setpgrp() is an alternative to setsid().
		# setsid puts the process in a new parent group and detaches its controlling terminal.
		os.setsid() 
		os.umask(0) 
	
		# do second fork
		try: 
			pid = os.fork() 
			if pid > 0:
				# exit from second parent
				sys.exit(0) 
		except OSError as err: 
			sys.stderr.write('fork #2 failed: {0}\n'.format(err))
			sys.exit(1) 
	
		# redirect standard file descriptors
		sys.stdout.flush()
		sys.stderr.flush()
		si = open(os.devnull, 'r')
		so = open(os.devnull, 'a+')
		se = open(os.devnull, 'a+')

		os.dup2(si.fileno(), sys.stdin.fileno())
		os.dup2(so.fileno(), sys.stdout.fileno())
		os.dup2(se.fileno(), sys.stderr.fileno())
	
		# write pidfile
		atexit.register(self.delpid)

		pid = str(os.getpid())
		with open(self.pidfile,'w+') as f:
			f.write(pid + '\n')

		signal.signal(signal.SIGTERM, self.sigterm)
		atexit.register(self.exit)
	
	def delpid(self):
		os.remove(self.pidfile)

	def start(self):
		"""Start the daemon."""

		# Check for a pidfile to see if the daemon already runs
		try:
			with open(self.pidfile,'r') as pf:
				pid = int(pf.read().strip())
		except IOError:
			pid = None
		except ValueError:
			pid = None

		if pid:
			sys.stderr.write("pidfile {0} already exist. Daemon already running?\n".format(self.pidfile))
			sys.exit(1)
		
		# Start the daemon
		self.daemonize()
		self.run()

	def stop(self):
		"""Stop the daemon."""

		# Get the pid from the pidfile
		try:
			with open(self.pidfile,'r') as pf:
				pid = int(pf.read().strip())
		except IOError:
			pid = None
	
		if not pid:
			sys.stderr.write("pidfile {0} does not exist. Daemon not running?\n".format(self.pidfile))
			return # not an error in a restart

		# Try killing the daemon process	
		try:
			for i in range(0,10):
				os.kill(pid, signal.SIGTERM)
				time.sleep(0.1)
			os.kill(pid, signal.SIGHUP)

		except OSError as err:
			e = str(err.args)
			if e.find("No such process") > 0:
				if os.path.exists(self.pidfile):
					os.remove(self.pidfile)
			else:
				print (str(err.args))
				sys.exit(1)

	def restart(self):
		"""Restart the daemon."""
		self.stop()
		self.start()

	#-----------------

	def status(self):
		"""
		Check if the daemon is currently running.
		Requires procfs, so it will only work on POSIX compliant OS'.
		"""
		# Get the pid from the pidfile
		try:
			with open(self.pidfile,'r') as pf:
				pid = int(pf.read().strip())
		except IOError:
			pid = None

		if not pid:
			return False

		try:
			return os.path.exists("/proc/{0}".format(pid))
		except OSError:
			return False


	def sigterm(self, signum, frame):
		""" sigterm method
		These actions will be done after SIGTERM.
		"""
		#self.logger.warn("Caught signal %s. Stopping daemon." % signum)
		os.remove(self.pidfile)
		sys.exit(0)

	def exit(self):
		""" exit method
		Cleanup pid file at exit.
		"""
		#self.logger.warn("Stopping daemon.")
		os.remove(self.pidfile)
		sys.exit(0)

	#-----------------

	def run(self):
		"""You should override this method when you subclass Daemon.
		
		It will be called after the process has been daemonized by 
		start() or restart()."""
