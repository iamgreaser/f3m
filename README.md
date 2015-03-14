F3M: Fast S3M playroutine for whatever, that also happens to work on the Game Boy Advance

Made in 2015 by GreaseMonkey - Public Domain

Test module is "Point of Departure" by Necros (pod.s3m), which you can get here (as it is not PD I've decided to post a link to it instead): http://lite.modarchive.org/module.php?55696

(While nintendo.bin is also not PD, it's a requirement in order to make this actually run on a GBA, and it's pretty small so it's included)

**Building the GBA version:**

You will need gcc and binutils for ARMv4T or higher. I personally use gcc-arm-none-eabi 4.8 2014q1 that is *somewhere* in FreeBSD's ports repository (try devel/gcc-arm-embedded).

For Windows, you'll have to read through `gba_build.sh` in a text editor.

For real OSes, run `./gba_build.sh` in a terminal. It will spit out a file called `gba_f3m.gba`. Use that in your favourite emulator. (It works in Mednafen, VBA-M and Higan as of writing.) Or you could try it out on actual hardware!

Note, this will probably not work straight away, so you will have to edit `gba_build.sh` and make it use the right prefix for your ARM cross compiler. (When building on a Raspberry Pi, you would leave this blank.)

In order to change the module the test player uses, modify `boot.S`, do a different `.incbin` after `s3m_beg`, rebuild, and it should work just nicely.

**Building the Open Sound System version:**

You will need a C compiler that can be accessed by running `cc`.

Run `./oss_build.sh` in a terminal.

This doesn't work on Windows. I made this simple player example for myself, and you can learn from it and adapt it to work for you.

**TODO list:**

* Most of the effects aren't implemented (heck, we don't even have vibrato), this should be fixed at some point
* Allow playback of oneshot samples by the audio engine
* DevKitARM or DevKitPRO or WhateverTheHellPeopleUSE support - I can never remember which one it is.

**TODON'T list:**

* Interpolation - if you want this, make an XM instead. And if you actually *like* XM, get out of my sight you filthy heathen!
* Workarounds for trackers like Modplug which think S3M can handle 16-bit samples and channel volumes and don't do that weird volume jump when you do an Rxy/tremolo effect - ST 3.21 is the one true authority here.
* Adlib channel support - we've got 32 cycles to mix 16 channels at 32768Hz on a GBA, do you really think this is even the slightest bit of a mildly good idea? (Answer: It's terrible. Go sit in the corner.)
* Any of the "optimisations" I've tried which just simply don't work on a GBA:
  - Using a shift instead of a multiply for volume calculation - didn't notice any speed gain, but the audio sure sounded awful!
  - Using a volume lookup table - code compiles to an MLA instruction without it, which is faster than doing LDRH then ADD
  - Precaching more than one sample ahead - despite the fact that gamepak reads are kinda slow, this isn't the bottleneck!
  - While it's possible to copy the module into fast RAM, it doesn't really improve things. It *is* supported, though (it's commented out).

**TOMIGHTDO list:**

* Proper stereo support. Not interested in getting this working on a GBA, however - this would require us to do 16 bytes per sample, which is slow!
* Big-endian support.

**TOYOUDOIT list:**

* Minimal players for other audio frameworks.

