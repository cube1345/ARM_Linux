# ============================================
# run_all.ps1 - one window, background processes
#   mediamtx + ffmpeg transcode (configurable channels)
#   + stream_server + snapshot generator
# Run:  powershell -ExecutionPolicy Bypass -File run_all.ps1
# Edit $channels to the cameras that exist (NVR concurrency limit!).
# ============================================
$ErrorActionPreference = "Stop"

# cleanup leftover processes (stale mediamtx would keep old config on 8888/1935)
Get-Process -Name ffmpeg -ErrorAction SilentlyContinue | Stop-Process -Force
Get-Process -Name mediamtx -ErrorAction SilentlyContinue | Stop-Process -Force
Get-Process -Name python -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 2

$base = "E:\EmbeddedDevelopment\WorkSpace\ARM_Linux\apps\PhotoAlbum"
$mtx  = "$base\mediamtx_v1.21.0_windows_amd64"
$snap = "$base\windows\snapshot.py"

# camera password: set $env:CAMPASS before running, or edit this line locally
$camPass = if ($env:CAMPASS) { $env:CAMPASS } else { "CHANGE_ME" }
$camHost = "192.168.114.48"
# channels that actually have cameras (respect NVR concurrent connection limit)
$channels = @(1, 2, 3, 4, 5, 6, 8)

# 1) mediamtx
$p1 = Start-Process -FilePath "$mtx\mediamtx.exe" -WorkingDirectory $mtx `
     -NoNewWindow -RedirectStandardOutput "$base\mediamtx.log" `
     -RedirectStandardError "$base\mediamtx.err" -PassThru
$pids = @($p1.Id)

# 2) ffmpeg transcode per channel -> RTMP (staggered)
foreach ($i in $channels) {
    $camUrl = "rtsp://admin:$camPass@$camHost:554/Streaming/Channels/${i}02"
    $proc = Start-Process -FilePath "ffmpeg" -WorkingDirectory $base `
        -ArgumentList @("-rtsp_transport","tcp","-i",$camUrl,
                        "-vf","scale=480:270","-af","volume=40,acompressor=threshold=0.1:ratio=8:attack=10:release=100:makeup=6",
                        "-c:a","aac","-b:a","64k","-ar","32000","-ac","1",
                        "-c:v","libx264","-preset","veryfast","-tune","zerolatency",
                        "-g","15","-keyint_min","15","-pix_fmt","yuv420p",
                        "-f","flv","rtmp://127.0.0.1:1935/live/cam$i") `
        -NoNewWindow -RedirectStandardOutput "$base\transcode_cam$i.log" `
        -RedirectStandardError "$base\transcode_cam$i.err" -PassThru
    $pids += $proc.Id
    Start-Sleep -Seconds 3
    if ($proc.HasExited) {
        Write-Host "cam$i : FAILED (exited) - check transcode_cam$i.err"
    } else {
        Write-Host "cam$i : running"
    }
}

# 3) stream_server (snap + recordings list/delete), root = $base
$p3 = Start-Process -FilePath "python" -WorkingDirectory $base `
     -ArgumentList @("stream_server.py","8000") `
     -NoNewWindow -RedirectStandardOutput "$base\streamsrv.log" `
     -RedirectStandardError "$base\streamsrv.err" -PassThru
$pids += $p3.Id

# 4) snapshot generator
$p4 = Start-Process -FilePath "python" -WorkingDirectory $base `
     -ArgumentList @($snap) `
     -NoNewWindow -RedirectStandardOutput "$base\snap.log" `
     -RedirectStandardError "$base\snap.err" -PassThru
$pids += $p4.Id

# 5) grid MJPEG server (九宫格多路实时, port 8010)
$p5 = Start-Process -FilePath "python" -WorkingDirectory $base `
     -ArgumentList @("$base\windows\grid_mjpeg.py","8010") `
     -NoNewWindow -RedirectStandardOutput "$base\grid_mjpeg.log" `
     -RedirectStandardError "$base\grid_mjpeg.err" -PassThru
$pids += $p5.Id

Write-Host ""
Write-Host "Started. PIDs: $($pids -join ' ')"
Write-Host "Logs: $base\*.log"
Write-Host "Check: netstat -ano | findstr 8888 1935 8554 8000 8010"
Write-Host ""
Write-Host "Press Enter to stop all processes..."
Read-Host

foreach ($id in $pids) { Stop-Process -Id $id -Force -ErrorAction SilentlyContinue }
Write-Host "Stopped."