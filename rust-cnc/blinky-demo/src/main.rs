//! Blinks the LED on a Pico board
//!
//! This will blink an LED attached to GP25, which is the pin the Pico uses for the on-board LED.
#![no_std]
#![no_main]

//use bsp::entry;
use rp2040_hal::entry;
use defmt::*;
use defmt_rtt as _;
use embedded_hal::digital::OutputPin;
use panic_probe as _;

// Provide an alias for our BSP so we can switch targets quickly.
// Uncomment the BSP you included in Cargo.toml, the rest of the code does not need to change.
//use rp_pico as bsp;
// use sparkfun_pro_micro_rp2040 as bsp;


// rp_pico --> rp2040_hal --> embedded-hal
//                 +--> rp2040_pac

/*
use bsp::hal::{ // aka rp_pico::hal::*
    clocks::{init_clocks_and_plls, Clock},
    pac,
    sio::Sio,
    watchdog::Watchdog,
};
*/
use rp2040_hal as hal;	// cargo add rp2040_hal
use hal::{
    clocks::{init_clocks_and_plls},
    pac,
    sio::Sio,
    watchdog::Watchdog,
};

//use rp_pico::hal::usb::UsbBus;
use usb_device::{class_prelude::*, prelude::*};
use usbd_serial::SerialPort;


//---------------------------------------------------------------------

// 200 Hz
// 4 x 1 (sof, type, dlen, 
// 4 (ts) + 4 x 2 (dp) + i/o?

/*
const MSG_BUF_SIZE = 42;

static usb_dev: UsbDevice<'static, UsbBus>;
static serial: SerialPort<'static, UsbBus>;
static msg_buf: heapless::Vec<u8, { MSG_BUF_SIZE }>;


let usb_bus = c.local.usb_bus;
let usb_bus = usb_bus.write(UsbBusAllocator::new(UsbBus::new(
    c.device.USBCTRL_REGS,
    c.device.USBCTRL_DPRAM,
    clocks.usb_clock,
    true,
    &mut resets,
)));
let serial = SerialPort::new(usb_bus);

let usb_desc = usb_device::device::StringDescriptors::default()
    .manufacturer("Bedroom Builds")
    .product("Serial port")
    .serial_number("RTIC");

let usb_dev = UsbDeviceBuilder::new(usb_bus, UsbVidPid(0x16c0, 0x27dd))
    .device_class(usbd_serial::USB_CLASS_CDC)
    .strings(&[usb_desc])
    .unwrap()
    .build();

let msg_buf = heapless::Vec::<u8, { blink_proto::MSG_BUF_SIZE }>::new();
let msg_q = heapless::Deque::<blink_proto::Message, 10>::new();

fn on_usb(mut ctx: on_usb::Context)
{
    let serial = ctx.local.serial;
    if !ctx.local.usb_dev.poll(&mut [serial]) {
        return;
    }
    let mut buf = [0u8; blink_proto::MSG_BUF_SIZE];
    let msg_buf = ctx.local.msg_buf;
    match serial.read(&mut buf) {
        Ok(count) if count > 0 => {
            if msg_buf.extend_from_slice(&buf[..count]).is_err() {
                error!("bufcopy {}", count);
                msg_buf.clear();
            }
            use blink_proto::ParseResult::*;
            match blink_proto::parse(msg_buf) {
                Found(msg) => {
                    if let blink_proto::Message::Ping { id } = msg {
                        info!("{:?}", msg);
                        let pong = blink_proto::Message::Pong { id };
                        ctx.shared.msg_q.lock(|q| q.push_back(pong).ok());
                    }
                    if let blink_proto::Message::Led {
                        id,
                        blinking,
                        pause,
                    } = msg
                    {
                        info!("{:?}", msg);
                        ctx.shared.led_blink.lock(|b| *b = blinking);
                        ctx.shared.led_pause.lock(|p| *p = pause);
                        // confirm execution with the same message
                        let pong = blink_proto::Message::Led {
                            id,
                            blinking,
                            pause,
                        };
                        ctx.shared.msg_q.lock(|q| q.push_back(pong).ok());
                    }
                    msg_buf.clear();
                }
                Need(b) => {
                    debug!("Need({})", b);
                } // continue reading
                HeaderInvalid | DataInvalid => {
                    debug!("invalid");
                    msg_buf.clear();
                }
            }

            // write back to the host
            loop {
                let msg = ctx.shared.msg_q.lock(|q| q.pop_front());
                let bytes = match msg {
                    Some(msg) => match blink_proto::wrap_msg(msg) {
                        Ok(msg_bytes) => msg_bytes,
                        Err(_) => break,
                    },
                    None => break,
                };
                let mut wr_ptr = bytes.as_slice();
                while !wr_ptr.is_empty() {
                    let _ = serial.write(wr_ptr).map(|len| {
                        wr_ptr = &wr_ptr[len..];
                    });
                }
            }
        }
        _ => {}
    }
}
*/

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


*/

enum MotionCommand {
}

//---------------------------------------------------------------------

#[entry]
fn main() -> ! {
    info!("Program start");
    let mut pac = pac::Peripherals::take().unwrap();
    let core = pac::CorePeripherals::take().unwrap();
    let mut watchdog = Watchdog::new(pac.WATCHDOG);
    //let sio = Sio::new(pac.SIO);

    // External high-speed crystal on the pico board is 12Mhz
    let external_xtal_freq_hz = 12_000_000u32;
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

    //let mut delay = cortex_m::delay::Delay::new(core.SYST, clocks.system_clock.freq().to_Hz());
    //let timer = hal::Timer::new_timer0(pac.TIMER0, &mut pac.RESETS, &clocks);
    let timer = hal::Timer::new(pac.TIMER, &mut pac.RESETS, &clocks);

    let sio = hal::Sio::new(pac.SIO);
    let pins = hal::gpio::Pins::new(
        pac.IO_BANK0,
        pac.PADS_BANK0,
        //sio.gpio_bank0,	// XXX wrong crate version?
        //rp_pico::rp2040_hal::sio::SioGpioBank0, // suggested, but wrong
        //sio::SioGpioBank0,	// suggested, but wrong
        //sio.SioGpioBank0,	// suggested, also wrong
        sio.gpio_bank0,		// available field, and wrong
        &mut pac.RESETS,
    );

    // XXX not for Pico-W
    //let mut led_pin = pins.led.into_push_pull_output();
    let mut led = pins.gpio25.into_push_pull_output();

/*
    loop {
        info!("on!");
        led_pin.set_high().unwrap();
        delay.delay_ms(500);
        info!("off!");
        led_pin.set_low().unwrap();
        delay.delay_ms(500);
    }
*/
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
        .device_class(2) // 2 for the CDC, from: https://www.usb.org/defined-class-codes
        .build();

    let mut said_hello = false;
    loop {
        // Send data to the PC
        if !said_hello && timer.get_counter().ticks() >= 2_000_000 {
            said_hello = true;
            // Writes bytes from `data` into the port and returns the number of bytes written.
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
                        led.set_high().unwrap();
                    } else {
                        led.set_low().unwrap();
                    }
                    rsp[index] = b.to_ascii_uppercase();
                }
                rsp[count + 0] = b'\r';
                rsp[count + 1] = b'\n';
                serial.write(&rsp[..count + 2]).unwrap();
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

// XXX no boot2 section?
// https://users.rust-lang.org/t/help-section-not-linked-into-program/92810
// https://github.com/rp-rs/rp2040-boot2

//#[link_section = ".boot_loader"]
#[link_section = ".boot2"]
#[used]
pub static BOOT_LOADER: [u8; 256] = rp2040_boot2::BOOT_LOADER_AT25SF128A;


// End of file
