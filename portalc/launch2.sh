
/usr/bin/gst-launch-0.10 -q udpsrc buffer-size=1 port=5000 ! audio/x-raw-int, rate=48000,  channels=1, endianness=1234, width=16, depth=16, signed=true ! alsasink  sync=false &
sleep 5
/usr/bin/gst-launch-1.0 -q udpsrc port=9000 caps='application/x-rtp, media=(string)video, clock-rate=(int)90000, encoding-name=(string)H264' ! rtph264depay ! h264parse ! omxh264dec ! eglglessink sync=false &
sleep 5