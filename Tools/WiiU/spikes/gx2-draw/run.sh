#!/bin/bash
Xvfb :99 -screen 0 1280x800x24 >/dev/null 2>&1 &
export DISPLAY=:99
sleep 2
cd /t/cemu/squashfs-root/usr/bin
./Cemu -g /s/build/gx2draw.wuhb > /s/cemu_stdout.txt 2>&1 &
CPID=$!
sleep 10; import -window root /s/shot_a.png
sleep 1; import -window root /s/shot_b.png
kill $CPID; sleep 2; kill -9 $CPID 2>/dev/null
