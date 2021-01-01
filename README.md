# UNBRICKER
# Unbrick an Arduino MKR1000 using another MKR1000.
Problem: The SAMD21 bootloader is in flash at address 0x0000, and can be blown away using a firmware flashing utility like bossac, 
if an offset of 0x2000 is not specified.
</br>
</br>
https://www.hackster.io/nicola-wrachien posted a helpful tutorial on unbricking the SAMD21, using a uChip as a host.
</br>
</br>
That project was easily converted to use an MKR1000 as a host. And the result is this repository, which includes the Eagle brd file needed to 
produce a PCB, as well as the Arduino sketch that the MKR1000 host can use to flash the bricked MKR1000.
</br>
</br>
To prevent bricking using bossac, use this command line: </br>
bossac -p COM21 -e -w -v -R --offset=0x2000 firmwarefile.ino.bin

