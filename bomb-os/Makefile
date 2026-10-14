BUILDROOTDIR := ../buildroot
OUTPUTDIR := $(CURDIR)
BUILDROOT_REPO := https://github.com/buildroot/buildroot.git

.PHONY: all setup config build clean

all: build

setup: 
	@if [ ! -d "$(BUILDROOTDIR)/.git" ]; then
		git clone $(BUILDROOT_REPO) $(BUILDROOTDIR);
	else
		echo "Buildroot repository already exists.";
	fi

config: setup
	$(MAKE) -C $(BUILDROOTDIR) O=$(OUTPUTDIR) menuconfig

build: setup
	$(MAKE) -C $(BUILDROOTDIR) O=$(OUTPUTDIR)

clean: 
	$(MAKE) -C $(BUILDROOTDIR) O=$(OUTPUTDIR) clean