BUILDROOTDIR := ../buildroot
OUTPUTDIR := $(CURDIR)/build
BUILDROOT_REPO := https://github.com/buildroot/buildroot.git
DEFCONFIG_FILE := $(CURDIR)/defconfig

IMAGE := $(OUTPUTDIR)/images/sdcard.img

.PHONY: all setup defconfig menuconfig saveconfig build clean flash

all: build

setup: 
	@if [ ! -d "$(BUILDROOTDIR)/.git" ]; then \
		git clone $(BUILDROOT_REPO) $(BUILDROOTDIR); \
	else \
		echo "Buildroot repository already exists."; \
	fi

# Applies the defconfig to your output directory
defconfig: setup
	$(MAKE) -C $(BUILDROOTDIR) O=$(OUTPUTDIR) BR2_DEFCONFIG=$(DEFCONFIG_FILE) defconfig

# Opens the Buildroot menu to modify your configuration
menuconfig: setup
	$(MAKE) -C $(BUILDROOTDIR) O=$(OUTPUTDIR) menuconfig

# Saves your current configuration back to the defconfig file
saveconfig: setup
	$(MAKE) -C $(BUILDROOTDIR) O=$(OUTPUTDIR) BR2_DEFCONFIG=$(DEFCONFIG_FILE) savedefconfig

# Builds the system using the configuration currently in the output directory
build: setup
	$(MAKE) -C $(BUILDROOTDIR) O=$(OUTPUTDIR) rpi-firmware-dirclean
	$(MAKE) -C $(BUILDROOTDIR) O=$(OUTPUTDIR) rpi-firmware
	
	$(MAKE) -C $(BUILDROOTDIR) O=$(OUTPUTDIR)

clean: 
	$(MAKE) -C $(BUILDROOTDIR) O=$(OUTPUTDIR) clean

flash: build
	@set -eu; \
	if [ ! -f "$(IMAGE)" ]; then \
		echo "ERROR: Image not found: $(IMAGE)"; exit 1; \
	fi; \
	if [ "$$(id -u)" -ne 0 ]; then \
		echo "Run with: sudo make flash"; exit 1; \
	fi; \
	echo "Available disks:"; \
	lsblk -d -p -o NAME,TRAN,RM,SIZE,MODEL; \
	echo ""; \
	printf "Enter SD card device (e.g. /dev/sdb): "; \
	read -r DEV; \
	[ -b "$$DEV" ] || { echo "Invalid device"; exit 1; }; \
	DEV="$$(readlink -f "$$DEV")"; \
	[ "$$(lsblk -dn -o TYPE "$$DEV")" = "disk" ] || \
		{ echo "Select a whole disk, not a partition"; exit 1; }; \
	echo ""; \
	lsblk -p -o NAME,SIZE,MODEL,MOUNTPOINTS "$$DEV"; \
	echo ""; \
	echo "WARNING: ALL DATA ON $$DEV WILL BE ERASED!"; \
	printf 'Type "%s" to confirm: ' "$$DEV"; \
	read -r CONFIRM; \
	[ "$$CONFIRM" = "$$DEV" ] || { echo "Cancelled"; exit 1; }; \
	lsblk -ln -p -o NAME "$$DEV" | tail -n +2 | tac | \
	while read -r PART; do \
		[ -n "$$PART" ] || continue; \
		while findmnt -rn -S "$$PART" >/dev/null 2>&1; do \
			echo "Unmounting $$PART"; \
			umount "$$PART" || { echo "Failed to unmount $$PART"; exit 1; }; \
		done; \
	done; \
	echo "Flashing $(IMAGE) to $$DEV..."; \
	dd if="$(IMAGE)" of="$$DEV" bs=4M status=progress conv=fsync; \
	sync; \
	echo "Flash complete!"
