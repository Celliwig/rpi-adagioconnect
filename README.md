# RPi-AdagioConnect

This is a kernel module developed for the Raspberry Pi 2, to drive the sound board from a Crestron Adagio AAS-2 Audio Server.

## Overview
   I had a Crestron Adagio AAS-2 with a broken motherboard, as the sound board was seperate I wondered if it could be hooked up
to a Raspberry Pi. The Adagio AAS-2 sound board is based on a Wolfson WM8770 codec, providing a set of 4 stereo outputs, and
1 stereo in. This codec requires I2S for sound, and SPI as a control channel. It also requires a master clock signal, the
frequency of which is dependent on the sampling frequency of the I2S signal, and there is also a master reset line for the codec.

Currently, this module is output only. While the board provides an ADC, it currently is not connected in my setup, so hasn't been
included in the driver.

### Compatability
   I believe this should be compatible with a Raspberry Pi 3, but I don't think this will work for the original Raspberry Pi,
however I have neither so can't make any further comment.

### Hookup
   The board was hooked up as specified below, this includes both GPIO and header pin numbering:

Function                                GPIO            Header
I2S PCM Clock                           18              12
I2S PCM FS                              19              35
I2S PCM DIN                             20              38
I2S PCM DOUT                            21              40
Codec Reset                             5               29
Board Amp Mute                          22              15
