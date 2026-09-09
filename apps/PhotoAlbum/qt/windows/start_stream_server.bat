@echo off
rem ============================================
rem stream_server: snap + recordings list/delete for board
rem MUST run from PhotoAlbum root (stream_server.py lives there)
rem ============================================
set BASE=E:\EmbeddedDevelopment\WorkSpace\ARM_Linux\apps\PhotoAlbum
cd /d %BASE%
python stream_server.py 8000
pause