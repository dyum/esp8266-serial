#!/bin/bash
read -p "Enter to continue"
python ~/src/esptool/esptool.py --port /dev/ttyUSB0 write_flash 0x00000 ../firmware/0x00000.bin
read -p "Enter to continue"
python ~/src/esptool/esptool.py --port /dev/ttyUSB0 write_flash 0x40000 ../firmware/0x40000.bin
