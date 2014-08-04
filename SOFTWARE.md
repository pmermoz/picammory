# Picammory Software


The Garden's Camera based on Raspberry Pi (with motion detection)


## Setting Up the Raspberry Pi

Copying an image to the SD card in Mac OS X (mostly graphical interface)
http://elinux.org/RPi_Easy_SD_Card_Setup

Download the image from a mirror or torrent
http://www.raspberrypi.org/downloads
    
Raw Images Raspbian, Release Date	2014-01-07

Extract the image by double clicking on the download file

Connect the SD card reader with the SD card inside;

From the Apple () menu, choose About This Mac, then click on More info...; if you are using Mac OS X 10.8.x Mountain Lion then click on System report.

Click on USB (or Card Reader if using an in-built SD card reader) then search for your SD card in the upper right section of the window; click it, then search for BSD name in the lower right section: must be something like diskn where n is a number (for example, disk4). Note this number

Unmount the partition so that you will be allowed to overwrite the disk by opening Disk Utility and unmounting it (do not eject it, or you have to reconnect it). Note: On Mac OS X 10.8.x Mountain Lion, "Verify Disk" (before unmounting) will display the BSD name as "/dev/disk1s1" (or similar), allowing you to skip the previous two steps.

From the Terminal run:
``` bash
sudo dd if=path_of_your_image.img of=/dev/diskn bs=1m
```
Remember to replace n with the number that you noted before!

	<!> Warning: giving a wrong number for '/dev/diskn' can damage your computer,
		be extremely careful with 'dd' command.

Wait a LONG time!

Insert it in the Raspberry Pi, and have fun

## Raspbian Initial Settings

``` bash
$ ssh pi@<ip address>
  password: raspberry
$ sudo raspi-config
```
Modify the following item:

* 1 ***Expand Filesystem***

* 2 Change User Password

* 4 Internationalisation Options

	* I1 Change Locale
	    * Remove en_GB.UTF-8 UTF-8
	    * Add en_US.UTF-8 UTF-8
    
	    * Default: en_US.UTF-8
    
    * I2 Change Timezone
    
	    * US
	    * Pacific Ocean


* ***5 Enable Camera***

* Advanced Options

	* A2 Hostname

		* PiCammory
    
Note: only 1 and 5 are required.

Reboot to apply changes
        
``` bash
$ sudo shutdown -r now
```

Optionally, after rebooting, check the time and timezone

``` bash
$ date
```
        
## Update Raspbian

``` bash
$ sudo apt-get update
$ sudo apt-get upgrade
```

## Install Netatalk & Bonjour
Netatalk is a freely-available Open Source AFP fileserver.

``` bash
$ sudo apt-get install netatalk
```
Register ASP (File sharing with Mac)
``` bash
$ sudo nano /etc/avahi/services/afpd.service

<?xml version="1.0" standalone='no'?>
<!DOCTYPE service-group SYSTEM "avahi-service.dtd">
<service-group>
    <name replace-wildcards="yes">%h</name>
    <service>
        <type>_afpovertcp._tcp</type>
        <port>548</port>
    </service>
</service-group>
```
Register SSH (Remote Shell)
``` bash
$ sudo nano /etc/avahi/services/ssh.service

<?xml version="1.0" standalone='no'?>
<!DOCTYPE service-group SYSTEM "avahi-service.dtd">
<service-group>
    <name replace-wildcards="yes">%h</name>
    <service>
        <type>_ssh._tcp</type>
        <port>22</port>
    </service>
</service-group>

$ sudo /etc/init.d/avahi-daemon restart
```
Test of Bonjour: Browse for mDNS/DNS-SD services using the Avahi daemon
``` bash
$ sudo apt-get install avahi-utils

$ avahi-browse -a
```
Install library needed for Python
``` bash
$ sudo apt-get install libavahi-compat-libdnssd1
```

## Install Python3 Goodies

``` bash
$ sudo apt-get install python3-dev
$ sudo apt-get install python3-setuptools
$ sudo apt-get install python3-pip
```
## Install WiringPi
GPIO Interface library for the Raspberry Pi

``` bash
$ git clone git://git.drogon.net/wiringPi
$ cd wiringPi
$ ./build
$ gpio -v
$ gpio readall
```

Optional:

``` bash
$ /usr/local/bin/gpio export 5 out
$ /usr/local/bin/gpio export 23 out
$ /usr/local/bin/gpio export 24 out
$ /usr/local/bin/gpio export 25 out
$ ls -l /sys/devices/virtual/gpio
total 0
drwxrwx--- 3 root gpio 0 Apr 27 08:41 gpio18
drwxrwx--- 3 root gpio 0 Apr 27 08:41 gpio23
drwxrwx--- 3 root gpio 0 Apr 27 08:41 gpio24
drwxrwx--- 3 root gpio 0 Apr 27 08:41 gpio5
drwxrwx--- 3 root gpio 0 Dec 31  1969 gpiochip0
```

## Install WiringPi2-Python
Python-wrapped version of Gordon Henderson's WiringPi version 2.

``` bash
$ git clone https://github.com/Gadgetoid/WiringPi2-Python.git
$ cd WiringPi2-Python
$ sudo python3 setup.py install
```

## Install PiCamera

``` bash
$ sudo apt-get install python3-picamera
```



If you get an error during Picammory execution, like:

``` bash
    File "/usr/lib/python3/dist-packages/picamera/camera.py", line 1470, in _get_frame
    return self._encoders.values()[0].frame
TypeError: 'dict_values' object does not support indexing
```

you need to change the line 1470 in /usr/lib/python3/dist-packages/picamera/camera.py from
``` Python3
return self._encoders.values()[0].frame
```
to
``` Python3
return (self._encoders.values())[0].frame
```

this is fixed in development version but not yet released.

## Install Python3 PIL
Python Imaging Library

``` bash
$ sudo pip-3.2 install Pillow
```

## Install GPAC
Multimedia Framework for MPEG-4, VRML, X3D, SVG, LASeR ...
http://gpac.wp.mines-telecom.fr

``` bash
$ sudo apt-get install gpac
```

## Install Picammory


### Download Picammory sources

for the last released version:
``` bash
$ git clone https://github.com/pmermoz/picammory.git ~/
```

or, for the current development version:
``` bash
$ git clone https://github.com/pmermoz/picammory.git -b develop ~/
```

### Build the C source code
Note: This step is optional, Picammory will rebuild the C code at startup.
``` bash
$ cd ~/picammory; python3 setup.py build_ext --inplace; cd ..
```

### Create the log files folder

``` bash
$ sudo mkdir /var/log/picammory
$ sudo chown pi /var/log/picammory
```

### Edit the configuration file
``` bash
$ nano ~/picammory/picammory.ini
```

* **[camera]**
	* **name**
	
		Name for your camera, this name is used to prefix filenames generated by the camera.

* **[smtp]**


	Picammory send mail to inform of system status and detection events.
	* **username**
	
		User name for your email account
	* **password**
	
		Password for your email account
	* **usetls**
	
		YES to use encrypted communication with the email server
	* **server**
		
		Mail server Address and port (like 'smtp.mailserver.com:587')

* **[email]**
	* **recipient**
	
	
		Email address (or list of) for the recipient of alert
	* **sender**
	
	
		Email address to identify this program (like 'GardenCam <mycamera@mydomain.com>')

* **[ftp]**

	Picammory upload videos and pictures to an FTP server
	* **username**
	
		FTP user name
	* **password**
	
		FTP password
	* **server**
	
		FTP server Address

* **[gpio]**
	
	* **ledRedPin**
	
		GPIO Pin number for the Red LED (23)
	* **ledGreenPin**
	
		GPIO Pin number for the Green LED (24)
	* **ledBluePin**
	
		GPIO Pin number for the Blue LED (25)
	* **ledCameraPin**
	
		GPIO Pin number for the Pi Camera LED (5)


## Install the Automatic start at boot

Copy the boot script for automatic start at boot time:

``` bash
$ sudo cp ~/picammory/etc/init.d/picammory /etc/init.d/
```

Make The Startup Script Executable

``` bash
$ sudo chmod 755 /etc/init.d/picammory
```

Make The Script Run At Startup

``` bash
$ sudo update-rc.d picammory defaults
```

## Install the Automatic restart
Restartd is a daemon for checking running and not running processes. It execute a script or a program if the process is not running.

Restart can take up to 20s.


```bash
$ sudo apt-get install restartd
$ sudo nano /etc/restartd.conf

picammory ".*picammorydaemon.py*" "service picammory restart"
```

## Finally, Reboot

Reboot the system
``` bash
$ sudo shutdown -r now
```
