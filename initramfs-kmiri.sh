#!/bin/bash

set -exou pipefail

WORK=/tmp/kmiri-initramfs
rm -rf "$WORK"
mkdir -p "$WORK"/{bin,sbin,etc,proc,sys,dev,usr/bin,usr/sbin}

cp /usr/bin/busybox "$WORK/bin/" 2>/dev/null || cp /bin/busybox "$WORK/bin/"

for x in sh mount umount dmesg uname ls cat echo sleep rm test mkdir mv; do
  ln -sf /bin/busybox "$WORK/bin/$x"
done

cat >"$WORK/init" <<'EOS'
#!/bin/sh
set -eu

mount -t devtmpfs devtmpfs /dev
mount -t proc proc /proc
mount -t sysfs sysfs /sys

mkdir -p /host
if ! mount -t hostfs -o / none /host 2>/dev/null; then
  mount -t hostfs none /host -o /
fi

PT_DIR=/host/tmp/kmiri-pt
mkdir -p "$PT_DIR"
rm -f "$PT_DIR"/req.* "$PT_DIR"/resp.*
echo ready > "$PT_DIR/ready"

while true; do
  found=0
  for req in "$PT_DIR"/req.*; do
    [ -e "$req" ] || continue
    found=1
    req_name=${req##*/}
    req_id=${req_name#req.}
    resp_tmp="$PT_DIR/.resp.$req_id.$$"
    resp="$PT_DIR/resp.$req_id"
    cmd="$(cat "$req" 2>/dev/null || true)"

    if [ -n "$cmd" ]; then
      printf '%s' "$cmd" >/proc/kmiri_pt
      cat /proc/kmiri_pt >"$resp_tmp" 2>/dev/null || echo "ERR bridge_read" >"$resp_tmp"
    else
      echo "ERR empty_req" >"$resp_tmp"
    fi

    mv "$resp_tmp" "$resp"
    rm -f "$req"
  done

  [ "$found" -eq 1 ] || sleep 0.01
done

EOS
chmod +x "$WORK/init"

(
  cd "$WORK"
  find . -print0 | cpio --null -ov --format=newc | gzip -9
) >/tmp/initramfs-kmiri.cpio.gz
