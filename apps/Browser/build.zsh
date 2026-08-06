cd /home/cube/WorkSpace/Linux/ARM_Linux_WS/apps/Browser
make clean
make CFLAGS='-Wall -Wextra -Wpedantic -Werror -O2'
make DESTDIR=/home/cube/WorkSpace/Linux/ARM_Linux/target install

cd /home/cube/WorkSpace/Linux/ARM_Linux
make

cd images
./start-qemu-scp.sh --fb




/usr/bin/media-browser \
    /dev/fb0 \
    /dev/input/event0 \
    /root/media \
    /usr/share/fonts/wqy-zenhei/wqy-zenhei.ttc \
    default \
    /dev/input/event1
