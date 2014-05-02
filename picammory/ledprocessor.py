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

#--- 3rd party
#try:
#    import RPi.GPIO as GPIO
#except RuntimeError:
#    print("Error importing RPi.GPIO!  This is probably because you need superuser privileges.  You can achieve this by using 'sudo' to run your script")
#import pigpio
import wiringpi2

#--- Local
import config

class LEDProcessor(object):

    _led_red_pin = 0
    _led_green_pin = 0
    _led_blue_pin = 0
    _led_camera_pin = 0

    @classmethod
    def start(cls):
        config_gpio = config.config['gpio']
        cls._led_red_pin = int(config_gpio['ledRedPin'])
        cls._led_green_pin = int(config_gpio['ledGreenPin'])
        cls._led_blue_pin = int(config_gpio['ledBluePin'])
        cls._led_camera_pin = int(config_gpio['ledCameraPin'])

        print("LED: Red:{} Green:{} Blue:{} Camera:{}".format(cls._led_red_pin, cls._led_green_pin, cls._led_blue_pin, cls._led_camera_pin))

        #GPIO.setmode(GPIO.BCM)

        #GPIO.setwarnings(False)
        #GPIO.setup(cls._led_red_pin,GPIO.OUT, initial=GPIO.LOW)
        #GPIO.setup(cls._led_green_pin,GPIO.OUT, initial=GPIO.LOW)
        #GPIO.setup(cls._led_blue_pin,GPIO.OUT, initial=GPIO.LOW)
        #GPIO.setwarnings(True)

        #pigpio.set_mode(cls._led_red_pin, pigpio.OUTPUT)
        #pigpio.set_mode(cls._led_green_pin, pigpio.OUTPUT)
        #pigpio.set_mode(cls._led_blue_pin, pigpio.OUTPUT)
        #pigpio.set_mode(cls._led_camera_pin, pigpio.OUTPUT)

        #pigpio.write(cls._led_red_pin, pigpio.LOW)
        #pigpio.write(cls._led_green_pin, pigpio.LOW)
        #pigpio.write(cls._led_blue_pin, pigpio.LOW)
        #pigpio.write(cls._led_camera_pin, pigpio.LOW)

        wiringpi2.pinMode(cls._led_red_pin, wiringpi2.GPIO.OUTPUT)
        wiringpi2.pinMode(cls._led_green_pin, wiringpi2.GPIO.OUTPUT)
        wiringpi2.pinMode(cls._led_blue_pin, wiringpi2.GPIO.OUTPUT)
        wiringpi2.pinMode(cls._led_camera_pin, wiringpi2.GPIO.OUTPUT)

        wiringpi2.digitalWrite(cls._led_red_pin, wiringpi2.GPIO.LOW)
        wiringpi2.digitalWrite(cls._led_green_pin, wiringpi2.GPIO.LOW)
        wiringpi2.digitalWrite(cls._led_blue_pin, wiringpi2.GPIO.LOW)
        wiringpi2.digitalWrite(cls._led_camera_pin, wiringpi2.GPIO.LOW)

    @classmethod
    def is_alive(cls):
        return True

    @classmethod
    def terminate(cls):
        log.info("LEDProcessor terminate requested")
        wiringpi2.digitalWrite(cls._led_red_pin, wiringpi2.GPIO.LOW)
        wiringpi2.digitalWrite(cls._led_green_pin, wiringpi2.GPIO.LOW)
        wiringpi2.digitalWrite(cls._led_blue_pin, wiringpi2.GPIO.LOW)
        wiringpi2.digitalWrite(cls._led_camera_pin, wiringpi2.GPIO.LOW)


    @classmethod
    def red(cls, state):
        if (state):
            #GPIO.output(cls._led_red_pin, GPIO.HIGH)
            #pigpio.write(cls._led_red_pin, pigpio.HIGH)
            wiringpi2.digitalWrite(cls._led_red_pin, wiringpi2.GPIO.HIGH)
        else:
            #GPIO.output(cls._led_red_pin, GPIO.LOW)
            #pigpio.write(cls._led_red_pin, pigpio.LOW)
            wiringpi2.digitalWrite(cls._led_red_pin, wiringpi2.GPIO.LOW)

    @classmethod
    def green(cls, state):
        if (state):
            #GPIO.output(cls._led_green_pin, GPIO.HIGH)
            #pigpio.write(cls._led_green_pin, pigpio.HIGH)
            wiringpi2.digitalWrite(cls._led_green_pin, wiringpi2.GPIO.HIGH)
        else:
            #GPIO.output(cls._led_green_pin, GPIO.LOW)
            #pigpio.write(cls._led_green_pin, pigpio.LOW)
            wiringpi2.digitalWrite(cls._led_green_pin, wiringpi2.GPIO.LOW)

    @classmethod
    def blue(cls, state):
        if (state):
            #GPIO.output(cls._led_blue_pin, GPIO.HIGH)
            #pigpio.write(cls._led_blue_pin, pigpio.HIGH)
            wiringpi2.digitalWrite(cls._led_blue_pin, wiringpi2.GPIO.HIGH)
        else:
            #GPIO.output(cls._led_blue_pin, GPIO.LOW)
            #pigpio.write(cls._led_blue_pin, pigpio.LOW)
            wiringpi2.digitalWrite(cls._led_blue_pin, wiringpi2.GPIO.LOW)

    @classmethod
    def camera(cls, state):
        if (state):
            #GPIO.output(cls._led_camera_pin, GPIO.HIGH)
            #pigpio.write(cls._led_camera_pin, pigpio.HIGH)
            wiringpi2.digitalWrite(cls._led_camera_pin, wiringpi2.GPIO.HIGH)
        else:
            #GPIO.output(cls._led_camera_pin, GPIO.LOW)
            #pigpio.write(cls._led_camera_pin, pigpio.LOW)
            wiringpi2.digitalWrite(cls._led_camera_pin, wiringpi2.GPIO.LOW)
