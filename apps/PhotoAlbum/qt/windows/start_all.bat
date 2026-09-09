@echo off
rem ============================================
rem PhotoAlbum monitor stream: start all 3 processes
rem   1) mediamtx        HLS/RTSP/RTMP + recording
rem   2) ffmpeg transcode camera HEVC->H.264 -> RTMP
rem   3) stream_server   recording list/delete for board
rem Dependencies: mediamtx.exe in MTXDIR; ffmpeg/python in PATH
rem ============================================
set BASE=E:\EmbeddedDevelopment\WorkSpace\ARM_Linux\apps\PhotoAlbum
set MTXDIR=%BASE%\mediamtx_v1.21.0_windows_amd64

start "mediamtx"      cmd /k "cd /d %MTXDIR% && mediamtx.exe"
start "transcode"     cmd /k "%BASE%\windows\transcode_chan5.bat"
start "stream_server" cmd /k "%BASE%\windows\start_stream_server.bat"

echo.
echo Started: mediamtx / transcode / stream_server
echo Close each window or press Ctrl+C to stop.
pause