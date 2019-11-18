# TI-99/4A Memory Expansion Test 

The purpose of this code is to run an expansion memory test from a TI-99/4A console that has nothing but a FlashRom 99 cartridge to test with. 

These are destructive memory tests... pre-loaded interrupt routines or RAM Disk content will be **erased**.

## Supported Cards

* Basic 32K memory expansions ( internal or external )
* Foundation 128K (or 512K modded) 
* Myarc 128K or 512K 
* SAMS Memory (upto 1024K)

Other cards may fallback to basic 32K.

## Build

Set your TI tms9900gcc bin folder into your path. 
Set environment variable for path to Tursi's libti99 gcc library.
Edit or comment out the part in the Makefile where it copies the rom into a place for classic99. 

run: make

It will produce a 32kexptest.bin 8K cartridge rom image. 

## Cool bits...

This build is an example of using GCC to run code that doesn't actually require the 32k expansion ram for the TI-99/4A. It leaves interrupts off, sets the gcc stack to work backwards from the top of the TI scratchpad space (>8400). It sets the data segment to begin write after the gcc workspace (>8320). And all of the constant and code goes into the 8k cartridge rom space (>6000). 

## License

Public Domain - Have fun, or see License.txt



