#!/bin/bash
Xvfb :99 -screen 0 1280x800x24 >/dev/null 2>&1 &
export DISPLAY=:99
sleep 2
cd /t/cemu/squashfs-root/usr/bin
exec ./Cemu --enable-gdbstub -g /t/jazz2.wuhb > /t/out/cemu_stdout.txt 2>&1
