# RPi-AdagioConnect

This is a kernel module developed for the Raspberry Pi 2, to drive the sound board from a Crestron Adagio AAS-2 Audio Server.

## Overview
I had a Crestron Adagio AAS-2 with a broken motherboard, as the sound board was seperate I wondered if it could be hooked up to a Raspberry Pi. The Adagio AAS-2 sound board is based on a Wolfson WM8770 codec, providing a set of 4 stereo outputs, and 1 stereo in. This codec requires I2S for sound, and SPI as a control channel. It also requires a master clock signal, the frequency of which is dependent on the sampling frequency of the I2S signal, and there is also a master reset line for the codec.

Currently, this module is output only. While the board provides an ADC, it currently is not connected in my setup, so hasn't been included in the driver.

### Compatability
I believe this should be compatible with a Raspberry Pi 3, but I don't think this will work for the original Raspberry Pi, however I have neither so can't make any further comment.

### Hookup
See Adagio Sound Board Pinout.ods for the Adagio sound board pinout.

The board was hooked up as specified below, this includes both GPIO and header pin numbering:


| Function           |  RPi GPIO  | RPi Header |   Adagio board    |
| ------------------:| ----------:| ----------:| -----------------:|
| I2S PCM Clock      |     18     |     12     |     CN22 (6)      | 
| I2S PCM FS         |     19     |     35     |     CN22 (2)      |
| I2S PCM DIN        |     20     |     38     |        N/C        |
| I2S PCM DOUT       |     21     |     40     | CN22 (8,10,12,14) |
| SPI MOSI           |     10     |     19     |     CN22 (24)     |
| SPI SCL            |     11     |     23     |     CN22 (25)     |
| SPI CE0            |      8     |     24     |     CN22 (23)     |
| Codec Reset        |      5     |     29     |     CN22 (26)     |
| Board Amp Mute     |     22     |     15     |     CN23 (14)     |

