cmake_minimum_required(VERSION 3.17)

include(pico_sdk_import.cmake)

project(pico-helloworld)

pico_sdk_init()

add_executable(pico-helloworld
	include/tusb_config.h
	src/main.c
	src/usb_descriptors.c
)

#$ vcocalc.py 125.000
# Requested: 125.0 MHz
# Achieved:  125.0 MHz
# REFDIV:    1
# FBDIV:     125 (VCO = 1500.0 MHz)
# PD1:       6
# PD2:       2
target_compile_definitions(pico-helloworld PRIVATE
	PLL_SYS_REFDIV=1
	PLL_SYS_VCO_FREQ_HZ=1500000000
	PLL_SYS_POSTDIV1=6
	PLL_SYS_POSTDIV2=2
	SYS_CLK_HZ=125000000
# Can manually change everything, but must meet specs, and may
# break USB if any numbers are incorrect...
#	PLL_SYS_POSTDIV1=5
#	PLL_SYS_POSTDIV2=3
#	SYS_CLK_HZ=100000000
)

pico_enable_stdio_usb(pico-helloworld 0)
pico_enable_stdio_uart(pico-helloworld 0)

# XXX full path to source or pioasm fails
# XXX and keep generated things with the build
pico_generate_pio_header(
	pico-helloworld
	${CMAKE_CURRENT_LIST_DIR}/src/ws2812.pio
	OUTPUT_DIR ${PROJECT_BINARY_DIR}/generated
)

pico_add_extra_outputs(pico-helloworld)

# pico-sdk/lib/tinyusb/src/portable/raspberrypi/rp2040/dcd_rp2040.c.o
target_include_directories(
	pico-helloworld PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/include)

target_link_libraries(
	pico-helloworld
	pico_stdlib
	pico_unique_id
	tinyusb_device
	tinyusb_board
	hardware_gpio
	hardware_pio
	hardware_pwm
)
