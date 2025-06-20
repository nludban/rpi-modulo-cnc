//! Blinks the LED on a Pico board
//!
//! This will blink an LED attached to GP25, which is the pin the Pico uses for the on-board LED.
#![no_std]
#![no_main]

use rp2040_hal::entry;
use defmt::*;
use defmt_rtt as _;
use embedded_hal::digital::OutputPin;
use panic_probe as _;

use rp2040_hal as hal;	// cargo add rp2040_hal
use hal::{
    clocks::{init_clocks_and_plls},
    pac,
    watchdog::Watchdog,
};

use usb_device::{class_prelude::*, prelude::*};
use usbd_serial::SerialPort;


//---------------------------------------------------------------------

// 200 Hz
// 4 x 1 (sof, type, dlen, 
// 4 (ts) + 4 x 2 (dp) + i/o?

//---------------------------------------------------------------------

/*
struct CommandBlock {
    magic_sof: u8;
    msg_type: u8;
    data_len: u8;
    checksum: u8;
    //payload []
};

struct MotionBlock {
    timestamp: u32;
    axis: i16[6];
};

enum MotionCommand {
}
*/

//---------------------------------------------------------------------

use core::mem::MaybeUninit;

mod app_pins {
    use crate::hal::{gpio};

    pub type Led = gpio::Pin<gpio::bank0::Gpio25,
                             gpio::FunctionSio<gpio::SioOutput>,
                             gpio::PullDown>;
}


struct Globals {
    i: i32,
}

impl Globals {
    fn preinit(& mut self) {
        self.i = 42;
    }
}


#[allow(static_mut_refs)]
static mut G: MaybeUninit<Globals> = MaybeUninit::uninit();

fn new_globals() -> &'static mut Globals {
    unsafe {
        #[allow(static_mut_refs)]
        let p = G.as_mut_ptr();
        //(*p).preinit();
        //return p; //G.as_mut_ptr();
        return p.as_mut().unwrap();
    }
}

//---------------------------------------------------------------------

struct AppPins {
    led: app_pins::Led,
}

impl AppPins {

    pub fn new(
        io_bank0: hal::pac::IO_BANK0,
        pads_bank0: hal::pac::PADS_BANK0,
        sio_bank0: hal::sio::SioGpioBank0,
        resets: &mut hal::pac::RESETS,
    ) -> Self {
        let pins = hal::gpio::Pins::new(
            io_bank0,
            pads_bank0,
            sio_bank0,
            resets,
        );

        AppPins {
            led: pins.gpio25.into_push_pull_output(),
        }
    }
}

//---------------------------------------------------------------------

#[entry]
fn main() -> ! {
    info!("Program start");
    let mut pac = pac::Peripherals::take().unwrap();
    let _core = pac::CorePeripherals::take().unwrap();
    let mut watchdog = Watchdog::new(pac.WATCHDOG);

    let external_xtal_freq_hz = 12_000_000u32;  // Pico crystal.
    let clocks = init_clocks_and_plls(
        external_xtal_freq_hz,
        pac.XOSC,
        pac.CLOCKS,
        pac.PLL_SYS,
        pac.PLL_USB,
        &mut pac.RESETS,
        &mut watchdog,
    )
    .ok()
    .unwrap();

    let timer = hal::Timer::new(pac.TIMER, &mut pac.RESETS, &clocks);

    let sio = hal::Sio::new(pac.SIO);
    let mut app_pins = AppPins::new(
        pac.IO_BANK0,
        pac.PADS_BANK0,
        sio.gpio_bank0,
        &mut pac.RESETS,
    );

    let _g = new_globals();

    let usb_bus = UsbBusAllocator::new(hal::usb::UsbBus::new(
        pac.USBCTRL_REGS,
        pac.USBCTRL_DPRAM,
        clocks.usb_clock,
        true,
        &mut pac.RESETS,
    ));

    let mut serial = SerialPort::new(&usb_bus);

    let mut usb_dev = UsbDeviceBuilder::new(&usb_bus, UsbVidPid(0x16c0, 0x27dd))
        .strings(&[StringDescriptors::default()
            .manufacturer("implRust")
            .product("Ferris")
            .serial_number("TEST")])
        .unwrap()
        .device_class(usbd_serial::USB_CLASS_CDC)
        .build();

    let mut said_hello = false;
    loop {
        // Send data to the PC
        if !said_hello && timer.get_counter().ticks() >= 2_000_000 {
            said_hello = true;
            let _ = serial.write(b"Hello, Rust!\r\n");
        }

        // Read data from PC
        if usb_dev.poll(&mut [&mut serial]) {
            let mut buf = [0u8; 64];
            let mut rsp = [0u8; 64];

            if let Ok(count) = serial.read(&mut buf) {
                let index = 0;
                for &b in &buf[..count] {
                    if b == b'r' {
                        app_pins.led.set_high().unwrap();
                    } else {
                        app_pins.led.set_low().unwrap();
                    }
                    rsp[index * 2 + 0] = b.to_ascii_uppercase();
                    rsp[index * 2 + 1] = b.to_ascii_lowercase();
                }
                rsp[count * 2 + 0] = b'\r';
                rsp[count * 2 + 1] = b'\n';
                serial.write(&rsp[..count * 2 + 2]).unwrap();
            }

        }
    }
}

#[link_section = ".bi_entries"]
#[used]
pub static PICOTOOL_ENTRIES: [hal::binary_info::EntryAddr; 5] = [
    hal::binary_info::rp_cargo_bin_name!(),
    hal::binary_info::rp_cargo_version!(),
    hal::binary_info::rp_program_description!(c"USB Fun"),
    hal::binary_info::rp_cargo_homepage_url!(),
    hal::binary_info::rp_program_build_attribute!(),
];

// Normally added by rp-hal
#[link_section = ".boot2"]
#[used]
pub static BOOT_LOADER: [u8; 256] = rp2040_boot2::BOOT_LOADER_AT25SF128A;

/**/
