BUILDROOT_DIR := $(CURDIR)/../buildroot
OUTPUT_DIR    := $(CURDIR)/build
DEFCONFIG     := $(BUILDROOT_DIR)/configs/raspberrypi4_64_defconfig

BUILDROOT_REPO := https://github.com/buildroot/buildroot.git
IMAGE          := $(OUTPUT_DIR)/images/sdcard.img

PROGRAM_NAME := bomb_os
PROGRAM_SRC  := $(CURDIR)/src/main.c
PROGRAM_BIN  := $(CURDIR)/rootfs_overlay/bin/bomb_os
CROSS_COMPILE := $(OUTPUT_DIR)/host/bin/aarch64-linux-

.PHONY: all setup configure build clean flash program

all: build

setup:
	@if [ ! -d "$(BUILDROOT_DIR)/.git" ]; then \
		echo "Cloning Buildroot..."; \
		git clone $(BUILDROOT_REPO) $(BUILDROOT_DIR); \
	else \
		echo "Buildroot already exists."; \
		git -C $(BUILDROOT_DIR) pull --ff-only; \
	fi

program:
	mkdir -p $(dir $(PROGRAM_BIN))
	$(CROSS_COMPILE)gcc $(PROGRAM_SRC) -o $(PROGRAM_BIN) \
		$$( $(OUTPUT_DIR)/host/bin/pkg-config --cflags --libs sdl2 ) -lm
	chmod +x $(PROGRAM_BIN)

configure: setup
	$(MAKE) -C $(BUILDROOT_DIR) O=$(OUTPUT_DIR) BR2_DEFCONFIG=$(DEFCONFIG) defconfig

	$(BUILDROOT_DIR)/utils/config --file $(OUTPUT_DIR)/.config --set-str BR2_ROOTFS_OVERLAY "$(CURDIR)/rootfs_overlay"

	$(BUILDROOT_DIR)/utils/config --file $(OUTPUT_DIR)/.config --enable BR2_PACKAGE_SDL2
	$(BUILDROOT_DIR)/utils/config --file $(OUTPUT_DIR)/.config --enable BR2_PACKAGE_SDL2_OPENGLES
	$(BUILDROOT_DIR)/utils/config --file $(OUTPUT_DIR)/.config --enable BR2_PACKAGE_SDL2_KMSDRM

	$(BUILDROOT_DIR)/utils/config --file $(OUTPUT_DIR)/.config --enable BR2_PACKAGE_LIBDRM
	$(BUILDROOT_DIR)/utils/config --file $(OUTPUT_DIR)/.config --enable BR2_PACKAGE_MESA3D
	$(BUILDROOT_DIR)/utils/config --file $(OUTPUT_DIR)/.config --enable BR2_PACKAGE_MESA3D_OPENGL_ES
	$(BUILDROOT_DIR)/utils/config --file $(OUTPUT_DIR)/.config --enable BR2_PACKAGE_MESA3D_GALLIUM_DRIVER_V3D
	$(BUILDROOT_DIR)/utils/config --file $(OUTPUT_DIR)/.config --enable BR2_PACKAGE_MESA3D_GALLIUM_DRIVER_VC4

	$(MAKE) -C $(BUILDROOT_DIR) O=$(OUTPUT_DIR) olddefconfig

build: configure
	$(MAKE) -C $(BUILDROOT_DIR) O=$(OUTPUT_DIR) rpi-firmware
	$(MAKE) -C $(BUILDROOT_DIR) O=$(OUTPUT_DIR) sdl2
	$(MAKE) program
	$(MAKE) -C $(BUILDROOT_DIR) O=$(OUTPUT_DIR)

clean:
	rm -rf $(PROGRAM_BIN)
	$(MAKE) -C $(BUILDROOT_DIR) O=$(OUTPUT_DIR) clean

flash: build
	@set -eu; \
	\
	if [ ! -f "$(IMAGE)" ]; then \
		echo "ERROR: Image not found: $(IMAGE)"; \
		exit 1; \
	fi; \
	\
	if [ "$$(id -u)" -ne 0 ]; then \
		echo "ERROR: Run with: sudo make flash"; \
		exit 1; \
	fi; \
	\
	echo "Available disks:"; \
	lsblk -d -p -o NAME,TRAN,RM,SIZE,MODEL; \
	echo ""; \
	\
	printf "Enter SD card device (e.g. /dev/sdb): "; \
	read -r DEV; \
	[ -b "$$DEV" ] || { echo "ERROR: Invalid device"; exit 1; }; \
	DEV="$$(readlink -f "$$DEV")"; \
	\
	[ "$$(lsblk -dn -o TYPE "$$DEV")" = "disk" ] || \
		{ echo "ERROR: Select a whole disk, not a partition"; exit 1; }; \
	\
	echo ""; \
	lsblk -p -o NAME,SIZE,MODEL,MOUNTPOINTS "$$DEV"; \
	echo ""; \
	\
	echo "WARNING: ALL DATA ON $$DEV WILL BE ERASED!"; \
	printf 'Type "%s" to confirm: ' "$$DEV"; \
	read -r CONFIRM; \
	[ "$$CONFIRM" = "$$DEV" ] || { echo "Cancelled"; exit 1; }; \
	\
	for PART in $$(lsblk -ln -p -o NAME "$$DEV" | tac | sed '$$d'); do \
		while findmnt -rn -S "$$PART" >/dev/null 2>&1; do \
			echo "Unmounting $$PART"; \
			umount "$$PART"; \
		done; \
	done; \
	\
	echo "Flashing $(IMAGE) to $$DEV..."; \
	dd if="$(IMAGE)" of="$$DEV" bs=4M status=progress conv=fsync; \
	sync; \
	echo "Flash complete!"
