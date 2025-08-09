export PICO_SDK_PATH	?= ${HOME}/Project/pico-sdk

# dtrt, cmake
export CLICOLOR		= 0
export VERBOSE		= defined

build/debug/Makefile: CMakeLists.txt
	mkdir -p build/debug
	cd build/debug && cmake ../..

# XXX pull in generated makefile, or something
build/debug/pico-helloworld.uf2:		\
		build/debug/Makefile		\
		include/tusb_config.h		\
		src/main.c			\
		src/usb_descriptors.c
	cd build/debug && make

.PHONY: build-debug
build-debug: build/debug/pico-helloworld.uf2

.PHONY: install-uf2
install-uf2: build/debug/pico-helloworld.uf2
	cp build/debug/pico-helloworld.uf2 /Volumes/RPI-RP2/
