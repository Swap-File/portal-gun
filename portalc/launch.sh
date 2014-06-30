#!/bin/bash
/usr/bin/minimu9-ahrs | /home/pi/ahrs-visualizer-master/ahrs-visualizer &
sleep 4
/usr/bin/raspivid -w 400 -h 260 -s -i record -fps 20 -g 10 -n -pf baseline -ex auto -t 0 -o - | /usr/bin/gst-launch-1.0 -q fdsrc ! h264parse ! rtph264pay config-interval=1 pt=96 ! udpsink host=192.168.1.255 port=9000 &
sleep 4
/usr/bin/gst-launch-0.10 -q alsasrc device=hw:0 ! audio/x-raw-int,rate=48000, channels=1, endianness=1234, width=16, depth=16, signed=true ! udpsink host=192.168.1.255 port=5000 &
sleep 4