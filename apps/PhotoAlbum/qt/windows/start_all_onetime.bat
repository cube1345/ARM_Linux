@echo off
rem ============================================
rem One window, 3 background processes.
rem Logs are written to files; view with: type <file>
rem ============================================
set BASE=E:\EmbeddedDevelopment\WorkSpace\ARM_Linux\apps\PhotoAlbum
set MTXDIR=%BASE%\mediamtx_v1.21.0_windows_amd64

start /b "mediamtx"    cmd /c "cd /d %MTXDIR% && mediamtx.exe > %BASE%\mediamtx.log 2>&1"
start /b "transcode"   cmd /c "cd /d %BASE% && ffmpeg -rtsp_transport tcp -i \"rtsp://admin:CHANGE_ME@192.168.114.48:554/Streaming/Channels/502\" -vf scale=480:270 -an -c:v libx264 -preset veryfast -tune zerolatency -g 15 -keyint_min 15 -pix_fmt yuv420p -f flv rtmp://127.0.0.1:1935/live/cam5 > %BASE%\transcode.log 2>&1"
start /b "streamsrv"   cmd /c "cd /d %BASE% && python stream_server.py 8000 > %BASE%\streamsrv.log 2>&1"

echo.
echo All 3 started in background. Logs:
echo   mediamtx   : %BASE%\mediamtx.log
echo   transcode  : %BASE%\transcode.log
echo   streamsrv  : %BASE%\streamsrv.log
echo.
echo View a log : type "%%BASE%%\mediamtx.log"
echo Stop all   : taskkill /IM mediamtx.exe /F ^& taskkill /IM ffmpeg.exe /F ^& taskkill /IM python.exe /F
echo Check ports: netstat -ano ^| findstr "8888 1935 8554 8000"
pause