#!/bin/sh

BUILDROOT_OUTPUT=${BUILDROOT_OUTPUT:-/home/cube/WorkSpace/Linux/ARM_Linux}
IMAGE_DIR=${IMAGE_DIR:-${BUILDROOT_OUTPUT}/images}
IMAGE_PATH=${IMAGE_DIR}/rootfs.ext4
QEMU_BIN=${QEMU_BIN:-/usr/bin/qemu-system-aarch64}
mode=serial
ssh_port=2222
vnc_display=0
snapshot=false

usage()
{
    cat >&2 <<'EOF'
Usage: start-qemu.sh [--fb|--vnc|--serial-only] [options]

Options:
  --ssh-port PORT       host SSH port (default: 2222)
  --vnc-display N       VNC display number (default: 0)
  --readonly, --snapshot
                        temporary QEMU snapshot overlay
  --                    pass remaining arguments to QEMU
EOF
    exit 2
}

is_decimal()
{
    case "$1" in
    ''|*[!0-9]*) return 1 ;;
    *) return 0 ;;
    esac
}

while [ "$1" ]; do
    case "$1" in
    --fb|-fb|fb) mode=fb; shift ;;
    --vnc|-vnc|vnc) mode=vnc; shift ;;
    --serial-only|serial-only) mode=serial; shift ;;
    --readonly|--snapshot) snapshot=true; shift ;;
    --ssh-port)
        [ "$#" -ge 2 ] || usage
        ssh_port=$2
        is_decimal "$ssh_port" || usage
        shift 2
        ;;
    --vnc-display)
        [ "$#" -ge 2 ] || usage
        vnc_display=$2
        is_decimal "$vnc_display" || usage
        shift 2
        ;;
    --help|-h) usage ;;
    --) shift; break ;;
    *) echo "qemu: unknown option: $1" >&2; usage ;;
    esac
done

if [ ! -r "$IMAGE_PATH" ]; then
    echo "qemu: rootfs image not found: $IMAGE_PATH" >&2
    exit 1
fi
if ! command -v "$QEMU_BIN" >/dev/null 2>&1; then
    echo "qemu: executable not found: $QEMU_BIN" >&2
    exit 1
fi
if nc -z 127.0.0.1 "$ssh_port" >/dev/null 2>&1; then
    echo "qemu: SSH port $ssh_port is already in use" >&2
    echo "qemu: use --ssh-port PORT or stop its owner" >&2
    exit 1
fi
existing_qemu=$(pgrep -af 'qemu-system-aarch64.*rootfs\.ext4' || true)
if [ -n "$existing_qemu" ]; then
    echo "qemu: another QEMU instance is using $IMAGE_PATH" >&2
    echo "$existing_qemu" >&2
    exit 1
fi

if $snapshot; then
    snapshot_arg=-snapshot
    echo "qemu: readonly snapshot mode enabled; guest writes are temporary" >&2
else
    snapshot_arg=
fi

run_qemu()
{
    case "$mode" in
    fb)
        exec "$QEMU_BIN" -M virt -cpu cortex-a53 -smp 1 \
            -kernel "$IMAGE_DIR/Image" \
            -append "rootwait root=/dev/vda console=ttyAMA0" \
            -netdev "user,id=eth0,hostfwd=tcp::${ssh_port}-:22" \
            -device virtio-net-device,netdev=eth0 \
            -drive "file=${IMAGE_PATH},if=none,format=raw,id=hd0" \
            -device virtio-blk-device,drive=hd0 \
            -device virtio-gpu-pci -device virtio-keyboard-pci \
            -device virtio-tablet-pci -device qemu-xhci \
            -audiodev pa,id=audio0 -device usb-audio,audiodev=audio0 \
            -display gtk,grab-on-hover=on -serial stdio -monitor none \
            ${snapshot_arg} "$@"
        ;;
    vnc)
        exec "$QEMU_BIN" -M virt -cpu cortex-a53 -smp 1 \
            -kernel "$IMAGE_DIR/Image" \
            -append "rootwait root=/dev/vda console=ttyAMA0" \
            -netdev "user,id=eth0,hostfwd=tcp::${ssh_port}-:22" \
            -device virtio-net-device,netdev=eth0 \
            -drive "file=${IMAGE_PATH},if=none,format=raw,id=hd0" \
            -device virtio-blk-device,drive=hd0 \
            -device virtio-gpu-pci -device virtio-keyboard-pci \
            -device virtio-tablet-pci -device qemu-xhci \
            -audiodev pa,id=audio0 -device usb-audio,audiodev=audio0 \
            -display none -vnc ":${vnc_display}" \
            -serial stdio -monitor none ${snapshot_arg} "$@"
        ;;
    serial)
        exec "$QEMU_BIN" -M virt -cpu cortex-a53 -smp 1 \
            -kernel "$IMAGE_DIR/Image" \
            -append "rootwait root=/dev/vda console=ttyAMA0" \
            -netdev "user,id=eth0,hostfwd=tcp::${ssh_port}-:22" \
            -device virtio-net-device,netdev=eth0 \
            -drive "file=${IMAGE_PATH},if=none,format=raw,id=hd0" \
            -device virtio-blk-device,drive=hd0 -nographic \
            ${snapshot_arg} "$@"
        ;;
    esac
}

run_qemu "$@"
