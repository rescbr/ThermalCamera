#!/bin/sh
export WAYLAND_DISPLAY=wayland-1
export XDG_RUNTIME_DIR=/run/0-runtime-dir
export SWAYSOCK=/run/0-runtime-dir/sway-ipc.0.sock
/storage/thermalcamera/thermalcamera --scale 2 "$@" &
APP_PID=$!
i=0
while [ $i -lt 20 ]; do
  sleep 1
  if swaymsg -t get_tree | grep -q thermalcamera; then
    swaymsg "[app_id=thermalcamera] focus" >/dev/null 2>&1
    swaymsg "[app_id=thermalcamera] fullscreen enable" >/dev/null 2>&1
    break
  fi
  i=$((i+1))
done
wait $APP_PID
swaymsg "[app_id=emulationstation] focus" >/dev/null 2>&1
