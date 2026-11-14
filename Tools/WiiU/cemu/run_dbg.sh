#!/bin/bash
# like run.sh, but runs xdotool steps from $XSTEPS (";"-separated, "shot:name" takes a screenshot) after Cemu starts
Xvfb :99 -screen 0 1280x800x24 >/dev/null 2>&1 &
XPID=$!
export DISPLAY=:99
sleep 2
cd /t/cemu/squashfs-root/usr/bin
./Cemu ${CEMU_ARGS--g /t/jazz2.wuhb} > /t/out/cemu_stdout.txt 2>&1 &
CPID=$!
IFS=';' read -ra STEPS <<< "$XSTEPS"
for st in "${STEPS[@]}"; do
  case "$st" in
    shot:*) import -window root /t/out/${st#shot:}.png 2>/dev/null ;;
    sleep:*) sleep ${st#sleep:} ;;
    *) xdotool $st ;;
  esac
done
kill $CPID; sleep 2; kill -9 $CPID 2>/dev/null; kill $XPID
