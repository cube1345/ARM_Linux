@echo off
rem ============================================
rem ffmpeg transcode: camera -> H.264 -> RTMP
rem Edit credentials/channel below for each camera.
rem ============================================
set BASE=E:\EmbeddedDevelopment\WorkSpace\ARM_Linux\apps\PhotoAlbum
cd /d %BASE%
ffmpeg -rtsp_transport tcp -i "rtsp://admin:CHANGE_ME@192.168.114.48:554/Streaming/Channels/502" -vf scale=480:270 -an -c:v libx264 -preset veryfast -tune zerolatency -g 15 -keyint_min 15 -pix_fmt yuv420p -f flv rtmp://127.0.0.1:1935/live/cam5
pause