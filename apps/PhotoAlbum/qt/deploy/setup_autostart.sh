#!/bin/sh
# i.MX6ULL 板卡运维：photo-album 开机自启 + 原神 cover 开屏 + 禁用正点原子 psplash
# 用法：scp 后到板卡执行  sh /home/root/setup_autostart.sh
set -u

# 1. 自启脚本（rcS 遍历 /etc/rcS.d；本系统不调用 /etc/rc.local）
#    命名 S20：紧跟 S04udev 之后启动（最早可用点），大幅压缩 logo→开屏的黑屏窗口
cat > /etc/rcS.d/S20photoalbum.sh <<'SCRIPT'
#!/bin/sh
pkill -9 psplash 2>/dev/null
sleep 1
for p in /proc/[0-9]*; do
  [ "$p" = "/proc/$$" ] && continue
  ls $p/fd 2>/dev/null | grep -q /dev/fb0 && kill -9 ${p##*/} 2>/dev/null
done
# 清屏，抹掉残留 logo / 进度条
dd if=/dev/zero of=/dev/fb0 bs=4096 count=300 2>/dev/null
cd /home/root
. ./qt_env.sh >/dev/null 2>&1
./photo-album /root/photos >/home/root/run.log 2>&1 &
exit 0
SCRIPT
chmod +x /etc/rcS.d/S20photoalbum.sh
rm -f /etc/rcS.d/S99photoalbum.sh 2>/dev/null

# 2. 禁用 psplash（reset 后不显示正点原子 logo，直接进原神开屏）
#    注意：rc 脚本用 for i in /etc/rcS.d/S* 遍历 —— mv 成 .bak 仍会被执行！
#    必须删除软链 / 或改成不以 S 开头的名字
rm -f /etc/rcS.d/S00psplash.sh /etc/rcS.d/S00psplash.sh.bak 2>/dev/null
[ -e /etc/init.d/psplash.sh ] && echo "psplash 已禁用（init.d/psplash.sh 本体保留，可 ln 恢复）"

echo "开机自启已配置：photo-album（含 cover 开屏动画）+ psplash 禁用"
echo "注意：部署文件须在 /home/root（photo-album/qt_env.sh/opening.gif）"