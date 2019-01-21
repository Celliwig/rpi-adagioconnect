# RPi-AdagioConnect

This is a kernel module developed for the Raspberry Pi 2, to drive the sound board from a Crestron Adagio AAS-2 Audio Server.

## Overview
I had a Crestron Adagio AAS-2 with a broken motherboard, as the sound board was seperate I wondered if it could be hooked up to a Raspberry Pi. The Adagio AAS-2 sound board is based on a Wolfson WM8770 codec, this provides 4 stereo outputs, and 1 stereo in. This codec requires I2S for sound, and SPI as a control channel. It also requires a master clock signal, the frequency of which is dependent on the sampling frequency of the I2S signal, and there is also a master reset line for the codec.

Currently this driver is output only. While the board provides an ADC, it currently is not connected in my setup, so hasn't been included in the driver.

### Sound Board
The required I2S and SPI lines were traced from the codec to the IDC headers without problem. However the board also includes a preamp and this has muting. Muting is controlled by an optocoupler (U10), the LED anode of which is connected through a resistor (R72) to CN23 (14). However, the cathode of the diode is connected to U11, and I could not find a means of controlling U11 so that the cathode would be pulled low (not helped by the fact I'm not sure what that component actually is!). R71 is connected to both U11, and CN22 (22), however it didn't seem to have any affect on the circuit applying 0V, or 3.3V to that pin. In the end I removed U11, and hooked the optocoupler led cathode directly to ground.

The board requires +/- 12V, the 5V line routed to the board in the original configuration is not connected. All required voltages are actually derived with onboard regulators. 

### Hookup
See Adagio Sound Board Pinout.ods for the Adagio sound board pinout. Now for the usual caveat. THIS HAS BEEN DERIVED FROM MY OWN EXPEREINCES, I HAVE SEEN VARIATIONS OF THE BOARD WHICH ARE AT LEAST PHYSICALLY DIFFERENT (COULD BE ELECTRONICALLY THE SAME, HAVEN'T HAD A CHANCE TO CHECK YET), WHICH CARRY THE SAME BOARD NUMBER (WHICH IS WEIRD). SO CHECK THIS YOURSELF, AT LEAST THE VOLTAGE IN +/- 12V. DON'T BLAME ME IF YOU FRY THE SOUND BOARD, THE PI, YOURSELF, THE WORLD (YOU GET THE IDEA). 

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

### Compatability
I believe this should be compatible with a Raspberry Pi 3, but I don't think this will work for the original Raspberry Pi, however I have neither so can't make any further comment.
