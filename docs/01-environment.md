# 1. Build environment

Everything builds inside **WSL2 on Windows 11**, distro **Ubuntu 22.04**.
22.04 is deliberate: it is the newest LTS that still packages `python2`,
which the 2020 Magic Lantern build scripts call.

## Packages

```bash
sudo apt install -y build-essential git mercurial python3 python3-docutils python3-pip python2 perl \
  zip unzip wget curl ca-certificates file bzip2 gcc-arm-none-eabi binutils-arm-none-eabi \
  libnewlib-arm-none-eabi gdb-multiarch pkg-config libgtk-3-dev libglib2.0-dev libpixman-1-dev \
  zlib1g-dev flex bison mtools
```

`mtools` is only needed to put files into QEMU card images.

## The ARM cross-compiler that matches Magic Lantern

Ubuntu ships `arm-none-eabi-gcc` 10.3. The official 7D builds of Magic
Lantern were made with **GCC 5.4.1 (ARM 5-2016-q3)**, and that is the one
used here (see [02-magic-lantern.md](02-magic-lantern.md) for why it
matters). Its Linux binaries are 32-bit, so 32-bit libraries are needed:

```bash
sudo dpkg --add-architecture i386 && sudo apt update
sudo apt install -y libc6:i386 libncurses5:i386 libstdc++6:i386
cd ~ && wget https://developer.arm.com/-/media/Files/downloads/gnu-rm/5_4-2016q3/gcc-arm-none-eabi-5_4-2016q3-20160926-linux.tar.bz2
tar xjf gcc-arm-none-eabi-5_4-2016q3-20160926-linux.tar.bz2
```

Magic Lantern's `Makefile.user.default` already looks for it at
`~/gcc-arm-none-eabi-5_4-2016q3`.

## Python 2 bits

Module READMEs are turned into strings by Python 2 scripts that need
`docutils`, and one of them is invoked as plain `python`:

```bash
wget https://bootstrap.pypa.io/pip/2.7/get-pip.py && python2 get-pip.py --user
python2 -m pip install --user "docutils<0.19"
mkdir -p ~/.local/bin && ln -sf /usr/bin/python2 ~/.local/bin/python   # user-local only
```

## The CF card from WSL

Windows sees the card reader as a drive (here `D:`). Mount it **read-only**
unless you really mean to write:

```bash
sudo mkdir -p /mnt/d && sudo mount -t drvfs D: /mnt/d -o ro
```

Removable drives are not auto-mounted, and the mount is gone after WSL
restarts.
