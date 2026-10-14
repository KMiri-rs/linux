#!/bin/bash

set -exou pipefail

WORK=/tmp/rust-initramfs
rm -rf "$WORK"
mkdir -p "$WORK"/{bin,sbin,etc,proc,sys,dev,usr/bin,usr/sbin}

cp /usr/bin/busybox "$WORK/bin/" 2>/dev/null || cp /bin/busybox "$WORK/bin/"

# Link most tools.
for x in sh mount umount dmesg insmod uname ls cat echo sleep; do
  ln -sf /bin/busybox "$WORK/bin/$x"
done

# Put the rust module in the kernel.
cp samples/rust/rust_minimal.ko "$WORK/"

cat >"$WORK/init" <<'EOF'
#!/bin/sh
mount -t devtmpfs devtmpfs /dev
mount -t proc proc /proc
mount -t sysfs sysfs /sys

echo "=== booted into test initramfs ==="
uname -a

echo "=== insmod rust_minimal.ko ==="
insmod /rust_minimal.ko test_parameter=123 test_bool_parameter=1 || echo "insmod failed: $?"

echo "=== dmesg (rust) ==="
dmesg | grep -i rust || true

exec sh
EOF
chmod +x "$WORK/init"

# Pack initramfs.cpio.gz
(
  cd "$WORK"
  find . -print0 | cpio --null -ov --format=newc | gzip -9
) >/tmp/initramfs-rust.cpio.gz
