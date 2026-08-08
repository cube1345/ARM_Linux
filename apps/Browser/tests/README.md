# Browser smoke tests

`media_smoke.sh` builds a host-side decoder test and checks generated
4/8/24/32-bit BMP, RLE4/RLE8 BMP, PNG, JPEG, GIF, WAV, MP3 and MP4 fixtures.
It also verifies an empty directory and rejects a corrupt PNG.
`input_reconnect_smoke.sh` uses host pipes and a FIFO to verify that one
disconnected operation does not stop other input and that an offline explicit
device path is reopened. It also checks tslib pointercal affine mapping and the
uncalibrated EVIOCGABS linear fallback.
`video_decoder_smoke.sh` verifies decoder operation registration, software
selection, optional RKMPP probing and `BROWSER_VIDEO_DECODER` parsing.
`plugin_manager_smoke.sh` builds real compatible, incompatible and
initialization-failing shared objects. It verifies ABI filtering,
image/audio/page/application/display registration, runtime extensions,
failure-time handle retention and shutdown before unload.

Run it from the workspace after the Buildroot target contains an MP3 and MP4:

```sh
cd /home/cube/WorkSpace/Linux/ARM_Linux_WS
apps/Browser/tests/media_smoke.sh
apps/Browser/tests/input_reconnect_smoke.sh
apps/Browser/tests/video_decoder_smoke.sh
apps/Browser/tests/plugin_manager_smoke.sh
```

On a running QEMU or target board, `input_auto_smoke.sh` verifies evdev
auto-discovery and graceful SIGTERM cleanup:

```sh
apps/Browser/tests/input_auto_smoke.sh
```
