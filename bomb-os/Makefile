BUILDROOTDIR := ../buildroot
OUTPUTDIR := $(CURDIR)/build
BUILDROOT_REPO := https://github.com/buildroot/buildroot.git
DEFCONFIG_FILE := $(CURDIR)/defconfig

.PHONY: all setup defconfig menuconfig saveconfig build clean

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
	$(MAKE) -C $(BUILDROOTDIR) O=$(OUTPUTDIR)

clean: 
	$(MAKE) -C $(BUILDROOTDIR) O=$(OUTPUTDIR) clean