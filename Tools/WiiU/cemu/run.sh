#!/bin/bash
# runs inside container: start Xvfb, launch Cemu with the bundle, take screenshots
Xvfb :99 -screen 0 1280x800x24 >/dev/null 2>&1 &
XPID=$!
export DISPLAY=:99
sleep 2
cd /t/cemu/squashfs-root/usr/bin
./Cemu -g /t/jazz2.wuhb > /t/out/cemu_stdout.txt 2>&1 &
CPID=$!
prev=0
for s in ${SHOTS:-20 40 60 90}; do sleep $((s - prev)); prev=$s; import -window root /t/out/shot_$s.png 2>/dev/null; done
kill $CPID; sleep 2; kill -9 $CPID 2>/dev/null; kill $XPID
