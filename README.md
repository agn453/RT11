# RT11
CP/M program to transfer files to/from RT-11 RX01 formatted 8-inch floppies

Source-code is written in a variant of C to be compiled using
BDS-C v1.46 (later versions may work too).

I use this to copy files from RT-11 formatted Single Density and
Single Sided RX01 eight-inch floppy disks on my CompuPro/Godbout
S-100 bus Z80 system with a DISK-1 floppy controller.

I think the original source came on one of the BDS-C User Group disks
and submitted into the public domain by William C. Colley, III in
March 1981.

There are two prebuilt binaries provided.

For use with the DISK-1 BIOS (sector numbering 1..26) [src/RT11-D1.COM](https://raw.githubusercontent.com/agn453/RT11/master/src/RT11-D1.COM),
and for other systems where the BIOS routines expect sector numbering 0..25
[src/RT11-ND1.COM](https://raw.githubusercontent.com/agn453/RT11/master/src/RT11-ND1.COM).

A CP/M format library file containing all files is
[rt11-12.lbr](https://raw.githubusercontent.com/agn453/RT11/master/rt11-12.lbr).

Tony Nicholson 17-May-2023
