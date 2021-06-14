# Adalight-FastLED-Plus
[![Build Status](https://github.com/d8ahazard/Adalight-FastLED-Plus/workflows/build/badge.svg?branch=master)](https://github.com/d8ahazard/Adalight-FastLED-Plus/actions?query=workflow%3Abuild)

![Adalight-Rainbow](http://i.imgur.com/sHygxq9.jpg)

## Synopsis

This is a modified version of dmadison's Adalight-FastLED sketch, modified to add a few additional features beyond the scope of his project and the 
original adalight protocol, but should be 100% backwards-compatible with other software that relies on the vanilla Adalight protocol.

This project modifies the Adalight protocol to use [FastLED](https://github.com/FastLED/FastLED) ([fastled.io](http://fastled.io)). This expands Adalight to, in theory, work with *[any supported FastLED strip](https://github.com/FastLED/FastLED/wiki/Chipset-reference)* including WS2812B (aka Adafruit NeoPixels).

In addition to ambilight setups, the protocol can be used to stream any color data over serial from a computer to supported LED strips.

For this sketch to work, you'll need to have a copy of the FastLED library. You can download FastLED [from GitHub](https://github.com/FastLED/FastLED) or through the libraries manager of the Arduino IDE. This program was writen using FastLED 3 - note that earlier versions of FastLED are untested and may not work properly.

For more information on my own ambilight setup, see [the project page](https://www.partsnotincluded.com/diy-ambilight-ws2812b/) on [PartsNotIncluded.com](http://www.partsnotincluded.com/).

## What's the "Plus" For?

I've added a few small features for convenience/completeness:

A secondary command protocol, currently only used to request device state and set brightness...but extensible to really...anything.
The option to store user-defined brightness in EEPROM, allowing changes to persist between power off/on.
Gamma correction...more or less the same values used by WLED.
A "maximum brightness" value, used to restrict the max input from the user to avoid power issues.


## Future Ideas

I don't want to make this too bloated - but I'm considering the possibility of adding a few more features, just haven't decided when or if I will
for sure or not:

Auto-Brightness-Limiter: Similar to WLED, a simple option that can restrict the overall brightness of the LEDs if the total appx. used voltage
exceeds a user-defined limit.

Setter for LED Count, type: Because the command protocol is pretty extensible, adding options to set other params like LED count, type, debugging,
or even Gamma Correction is quite trivial.

Adjustable gamma correction: Just another parameter with a quick function to calculate the Gamma Table being used.

Startup animation: Because why not? :P 


## Basic Setup

Open the LEDstream_FastLED file in the Arduino IDE and customize the settings at the top for your setup. You will need to change:

- Number of LEDs
- LED data pin
- LED type

If you are using a 4-wire LED chipset like APA102 or SK9822, you will need to uncomment the `PIN_CLOCK` line and set that as well.

Upload to your Arduino and use a corresponding PC application to stream color data. You can get the Processing files from the [main Adalight repository](https://github.com/adafruit/Adalight), though I would recommend using Patrick Siegler's fork of Lightpacks's Prismatik, which you can find [here](https://github.com/psieg/Lightpack/releases).

## Additional Settings/Features

There are additional settings to allow for adjusting:

- Max brightness
- Starting brightness
- LED color order
- Serial speed
- Serial timeout length
- Store brightness to EEPROM
- Gamma correction

There are also optional settings to clear the LEDs on reset or flush the incoming serial buffer after every latch. This latter option is enabled by default to help with flickering when using WS2812B LEDs.

## Set Brightness

You can *also* now set the brightness of the LEDs after boot, without having to modify config files. To do so, send a serial packet to the device,
but use the following byte sequence:

Bytes 0-2 (Command Magic) ('Adb'), etc.
Byte 3 (hi) - "B"
Byte 4 (lo) - "R"
Byte 5 (brightness) - 0-255

You should ensure that you are not trying to send color data at the same time, or issues will arise.

## Fetch State

Similar to setting brightness, you can query the device state by sending the following command::

Bytes 0-2 (Command Magic) ('Adb'), etc.
Byte 3 (hi) - "S"
Byte 4 (lo) - "T"
Byte 5 (unused) - 0

This will cause the device to return a state message, formatted like so:

AdbN=<led_count>;B=<brightness>

Or as an example:

AdbN=80;B=160

The first three characters will always be the "Command Magic" value of "Adb", or whatever value is defined in the sketch.
Next will be N= and then the defined LED count.
Next is a ";" as a separator, and finally "B=", and the current user-defined brightness.


## Debug Settings

The code includes two debugging options:
- DEBUG_LED
- DEBUG_FPS

`DEBUG_LED` will turn on the Arduino's built-in LED on a successful header match, and off when the LEDs latch. If your LEDs aren't working, this will help confirm that the Arduino is receiving properly formatted serial data.

`DEBUG_FPS`, similarly, will toggle a given pin when the LEDs latch. This is useful for measuring framerate with external hardware, like a logic analyzer.

To enable either of these settings, uncomment their respective `#define` lines.

## Issues and LED-types

I've only tested the code with the WS2812B strips I have on hand, but so far it performs flawlessly. If you find an issue with the code or can confirm that it works with another chipset, please let me know!

## Credits and Contributions

Thanks to Adafruit for the initial code and for developing the Adalight protocol. The base for the original FastLED modifications is [this gist](https://gist.github.com/jamesabruce/09d79a56d270ed37870c) by [James Bruce](https://github.com/jamesabruce) and [Dave Madison](https://github.com/dmadison). Thanks James, Dave!

Pull requests to improve this software are always welcome!

## License

Adalight is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as
published by the Free Software Foundation, either version 3 of
the License, or (at your option) any later version.

Adalight is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with Adalight.  If not, see <http://www.gnu.org/licenses/>.
