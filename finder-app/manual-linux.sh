#!/bin/bash
# Script outline to install and build kernel.

set -e
set -u

OUTDIR=/tmp/aeld
KERNEL_REPO=git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
KERNEL_VERSION=v5.15.163
BUSYBOX_VERSION=1_33_1
FINDER_APP_DIR=$(realpath $(dirname $0))
ARCH=arm64
CROSS_COMPILE=aarch64-none-linux-gnu-
export CROSS_COMPILE

if [ $# -lt 1 ]
then
	echo "Using default directory ${OUTDIR} for output"
else
	OUTDIR=$1
	echo "Using passed directory ${OUTDIR} for output"
fi

mkdir -p ${OUTDIR}

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/linux-stable" ]; then
    #Clone only if the repository does not exist.
	echo "CLONING GIT LINUX STABLE VERSION ${KERNEL_VERSION} IN ${OUTDIR}"
	git clone ${KERNEL_REPO} --depth 1 --single-branch --branch ${KERNEL_VERSION}
fi
if [ ! -e ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ]; then
    cd linux-stable
    echo "Checking out version ${KERNEL_VERSION}"
    git checkout ${KERNEL_VERSION}

    # TODO: Add your kernel build steps here
    echo "==> Building the kernel"
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} mrproper
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} defconfig
    make -j4 ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} all
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} modules
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} dtbs


fi

echo "==> Adding the Image in outdir"
cp ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ${OUTDIR}

echo "Creating the staging directory for the root filesystem"
cd "$OUTDIR"
if [ -d "${OUTDIR}/rootfs" ]
then
	echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
    sudo rm  -rf ${OUTDIR}/rootfs
fi

# TODO: Create necessary base directories
echo "==> Create necessary base directories"

mkdir "${OUTDIR}/rootfs"
cd "${OUTDIR}/rootfs"
mkdir -p bin dev etc home lib lib64 proc sbin sys tmp usr var
mkdir -p usr/bin usr/lib usr/sbin
mkdir -p var/log


echo "==> Configure busybox"

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/busybox" ]
then
git clone git://busybox.net/busybox.git
    cd busybox
    git checkout ${BUSYBOX_VERSION}
    # TODO:  Configure busybox
else
    cd busybox
fi

echo "==> Make and install busybox"

# TODO: Make and install busybox
make distclean
make defconfig
make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE}
make CONFIG_PREFIX="${OUTDIR}/rootfs" ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} install


# TODO: Add library dependencies to rootfs
cd "${OUTDIR}/rootfs"
echo "==> Library dependencies"
${CROSS_COMPILE}readelf -a bin/busybox | grep "program interpreter"
${CROSS_COMPILE}readelf -a bin/busybox | grep "Shared library"

# Library dependencies
#       [Requesting program interpreter: /lib/ld-linux-aarch64.so.1]
cp $(find $(dirname $(which aarch64-none-linux-gnu-gcc))/../ -name ld-linux-aarch64.so.1) lib/
#  0x0000000000000001 (NEEDED)             Shared library: [libm.so.6]
cp $(find $(dirname $(which aarch64-none-linux-gnu-gcc))/../ -name libm.so.6) lib64/
#  0x0000000000000001 (NEEDED)             Shared library: [libresolv.so.2]
cp $(find $(dirname $(which aarch64-none-linux-gnu-gcc))/../ -name libresolv.so.2) lib64/
#  0x0000000000000001 (NEEDED)             Shared library: [libc.so.6]
cp $(find $(dirname $(which aarch64-none-linux-gnu-gcc))/../ -name libc.so.6) lib64/


# TODO: Make device nodes
echo "==> Make device nodes"
sudo mknod -m 666 dev/null c 1 3
sudo mknod -m 600 dev/console c 5 1

# TODO: Clean and build the writer utility
echo "==> Clean and build the writer utility"
cd $FINDER_APP_DIR
#make clean
make

# TODO: Copy the finder related scripts and executables to the /home directory
# on the target rootfs
echo "==> Copy the finder related scripts"
find . -type f -exec cp {} "${OUTDIR}/rootfs/home" \;
# cp * "${OUTDIR}/rootfs/home" 2> /dev/null
cp -r conf/ "${OUTDIR}/rootfs/home"

# TODO: Chown the root directory
echo "==> Chown the root directory"
cd "${OUTDIR}/rootfs"
sudo chown -R root:root *

# TODO: Create initramfs.cpio.gz
echo "==> Create initramfs.cpio.gz"
find . | cpio -H newc -ov --owner root:root > ${OUTDIR}/initramfs.cpio
cd $OUTDIR
gzip -f initramfs.cpio

echo "==> ALL DONE"
