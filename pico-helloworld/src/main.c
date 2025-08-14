/*********************************************************************/
/*
 * pico-helloworld:main.c
 */
/*********************************************************************/

#include "sbfmt.h"

#include "ws2812.pio.h"

#include <pico/stdlib.h>
#include <pico/binary_info.h>

#include <hardware/clocks.h>
#include <hardware/gpio.h>
#include <hardware/irq.h>
#include <hardware/pio.h>
#include <hardware/pwm.h>

// TinyUSB glue
#include <bsp/board_api.h>	// Also fine: bsp/board.h
#include <tusb.h>

#include <ctype.h>

//const uint LED_PIN = 25;	// pico (non-W)
const uint LED_PIN = 13;	// Metro rp2040 "Onboard #13 LED"
const uint WS2812_PIN = 14;	// Metro rp2040 "Onboard RGB NeoPixel"
const uint IS_RGBW = 0;

//--------------------------------------------------

void
enable_led(
   int			pin
   )
{
   gpio_init(pin);
   gpio_set_dir(pin, GPIO_OUT);
}

void
led_on(int pin)
{
   gpio_put(pin, 1);
}

void
led_off(int pin)
{
   gpio_put(pin, 0);
}

void
led_on_off(int pin, int on_off)
{
   gpio_put(pin, !!on_off);
}

//--------------------------------------------------

static PIO		pixel_pio;
static uint		pixel_sm;

void
enable_ws2812_pixel(
   int			pin
   )
{
   uint offset;
   (void) pio_claim_free_sm_and_add_program_for_gpio_range(
      &ws2812_program,
      &pixel_pio, &pixel_sm, &offset,
      pin, 1, true);
   ws2812_program_init(pixel_pio, pixel_sm, offset, pin, 800000, IS_RGBW);
}


inline
uint32_t
ws2812_rgb(uint32_t r, uint32_t g, uint32_t b) {
   return ((((g << 8) + r) << 8) + b) << 8;
}

static
inline
void
put_pixel(uint32_t grbx) {
   pio_sm_put_blocking(pixel_pio, pixel_sm, grbx);
}

//--------------------------------------------------

enum {
  BLINK_NOT_MOUNTED = 250,
  BLINK_MOUNTED = 1000,
  BLINK_SUSPENDED = 2500,
};

static uint32_t blink_interval_ms = BLINK_NOT_MOUNTED;

volatile uint pwm_count;

void
gpio_intr_callback(
   uint			gpio,
   uint32_t		event_mask
   )
// gpio_irq_callback_t from hardware/GPIO
// See also irq_handler_t, gpio_add_raw_irq_handler_masked(...)
{
   gpio_acknowledge_irq(gpio, event_mask);
   //...
}

void
pwm_intr_handler()
{
   pwm_clear_irq(pwm_gpio_to_slice_num(1));
   pwm_count++;
   //put_pixel(0x22000000);	// green
}

static
void
echo_serial_port(uint8_t itf, uint8_t buf[], uint32_t count)
{
  uint8_t const case_diff = 'a' - 'A';

  for (uint32_t i = 0; i < count; i++) {
    if (itf == 0) {
      // echo back 1st port as lower case
      if (isupper(buf[i])) buf[i] += case_diff;
    } else {
      // echo back 2nd port as upper case
      if (islower(buf[i])) buf[i] -= case_diff;
    }

    tud_cdc_n_write_char(itf, buf[i]);
    tud_cdc_n_write_char(itf, buf[i]);
    tud_cdc_n_write_char(itf, buf[i]);
  }
  tud_cdc_n_write_flush(itf);
}

// Invoked when device is mounted
void tud_mount_cb(void) {
  blink_interval_ms = BLINK_MOUNTED;
}

// Invoked when device is unmounted
void tud_umount_cb(void) {
  blink_interval_ms = BLINK_NOT_MOUNTED;
}

static
void
cdc_task(void)
{
   for (int itf = 0; itf < CFG_TUD_CDC; itf++) {
      // connected() check for DTR bit
      // Most but not all terminal client set this when making connection
      // if ( tud_cdc_n_connected(itf) )
      {
	 if (tud_cdc_n_available(itf)) {
	    uint8_t buf[64];

	    uint32_t count = tud_cdc_n_read(itf, buf, sizeof(buf));

	    // echo back to both serial ports
	    echo_serial_port(0, buf, count);
	    echo_serial_port(1, buf, count);
	 }
      }
   }
}

// Invoked when cdc when line state changed e.g connected/disconnected
// Use to reset to DFU when disconnect with 1200 bps
void tud_cdc_line_state_cb(uint8_t instance, bool dtr, bool rts) {
  (void)rts;

  // DTR = false is counted as disconnected
  if (!dtr) {
    // touch1200 only with first CDC instance (Serial)
    if (instance == 0) {
      cdc_line_coding_t coding;
      tud_cdc_get_line_coding(&coding);
      if (coding.bit_rate == 1200) {
        if (board_reset_to_bootloader) {
          board_reset_to_bootloader();
        }
      }
    }
  }
}

void
notyet_tud_cdc_rx_cb(uint8_t itf) {
   //gpio_put(LED_PIN, 1);	// On
   //sleep_ms(100);
   uint8_t buf[CFG_TUD_CDC_RX_BUFSIZE];
   uint32_t count = tud_cdc_n_read(itf, buf, sizeof(buf));
   if ((itf == 0) && (count > 0))
      tud_cdc_n_write(itf, (const uint8_t *)"One\r\n", 5);
   if ((itf == 1) && (count > 0))
      tud_cdc_n_write(itf, (const uint8_t *)"Two\r\n", 5);
   tud_cdc_n_write_flush(itf);
}

//---------------------------------------------------------------------

void
enable_pwm_10_Hz(
   int			pin
   )
{
   uint slice = pwm_gpio_to_slice_num(pin);
   uint channel = pwm_gpio_to_channel(pin);

   // Visible 10-Hz for validation
   pwm_config cfg = pwm_get_default_config();
   pwm_config_set_clkdiv_int(&cfg, 250);	// 500-kHz
   pwm_config_set_wrap(&cfg, 50000);		// TOP => 10-Hz
   pwm_init(slice, &cfg, false);		// Also, CC => 0.

   gpio_set_function(pin, GPIO_FUNC_PWM);
   pwm_set_chan_level(slice, channel, 25000);	// CC = 50% duty cycle
   pwm_set_enabled(slice, true);
   return;
}

void
enable_pwm_3_125_MHz(
   int			pin
   )
{
   uint slice = pwm_gpio_to_slice_num(pin);
   uint channel = pwm_gpio_to_channel(pin);

   pwm_config cfg = pwm_get_default_config();
   pwm_config_set_wrap(&cfg, 40);		// 125 / 40 = 3.125-MHz
   pwm_init(slice, &cfg, false);

   gpio_set_function(pin, GPIO_FUNC_PWM);	// Start driving pin.
   pwm_set_chan_level(slice, channel, 20);	// 50% duty cycle
   pwm_set_enabled(slice, true);
   return;
}

void
enable_pwm_counter_ms(
   int			pin
   )
{
   uint slice = pwm_gpio_to_slice_num(pin);
   assert(pwm_gpio_to_channel(pin) == PWM_CHAN_B);

   // Or exernal resistor required?
   gpio_pull_down(pin);

   // Interrupt on rising edge, 1-kHz from 3.125-MHz
   pwm_config cfg = pwm_get_default_config();
   pwm_config_set_clkdiv_mode(&cfg, PWM_DIV_B_RISING);
   pwm_config_set_wrap(&cfg, 3125);

   pwm_init(slice, &cfg, false);
   pwm_set_enabled(slice, true);
   gpio_set_function(pin, GPIO_FUNC_PWM); // Only safe after config

   // enable intr
   pwm_clear_irq(slice);
   pwm_set_irq_enabled(slice, true);
   irq_set_exclusive_handler(PWM_DEFAULT_IRQ_NUM(), pwm_intr_handler);
   irq_set_enabled(PWM_DEFAULT_IRQ_NUM(), true);

}

int
main()
{
   //bi_decl(bi_program_description("rpi-modulo-cnc:pico-helloworld"));
   //bi_decl(bi_1pin_with_name(LED_PIN, "On-board LED"));

   board_init();

   tusb_rhport_init_t dev_init = {
      .role = TUSB_ROLE_DEVICE,
      .speed = TUSB_SPEED_AUTO
   };
   tusb_init(BOARD_TUD_RHPORT, &dev_init);

   if (board_init_after_tusb) {
      board_init_after_tusb();
   }

   // sys_clk = 125 MHz...
   //uint32_t boot_sys_clk = clock_
   //set_sys_clock_pll()
   //set_sys_clock_khz(125 * 1000, true);
   // pico-sdk/src/rp2_common/hardware_clocks/scripts/vcocalc.py 125.000

   enable_led(LED_PIN);
   //enable_pwm_10_Hz(LED_PIN);

   // USB spec generally allows 500ms for a response, excepting
   // startup/resume under 10ms, unless the host OS is lenient...
   for (int i = 0; i < 500; i++) {
      tud_task();
      sleep_ms(1);
   }

   enable_ws2812_pixel(WS2812_PIN);

   // boot status to pixel?

   // GP00 => UART0 TX output (debug)?
   // GP01 => 1-kHz interrupt source
   //	PWM0B = rising edge counter [0-3125) {supervisor or debug}
   //	GPIO input + interrupt {from psst command pulse = sync/fault}
   // GP02 => command /pulse => synch/fault
   // GP03 => command data-in
   // GP04 => command data-out
   // GP05 => command clock (3.125-MHz)
   // GP06 => status /pulse => fault
   // GP07 => status data-in
   // GP08 => status data-out

   // For test, generate 3.125-MHz out on GP05
   enable_pwm_3_125_MHz(5);

   // For test, externally wire the 3.125-MHz to GP01
   // Note for test, it's driven push-pull
   // Metro rp2040: GP01/UART0-RX will be on "RX" board pin when the
   // selector switch is on the tx=0 rx=1 side.
   enable_pwm_counter_ms(1);

/*
   for (;;) {
      //put_pixel(0x22000000);	// green - by intr handler
      //sleep_ms(1000);
      put_pixel(0x00220000);	// red
      sleep_ms(1000);
      put_pixel(0x00002200);	// blue
      sleep_ms(1000);
   }
*/

   // 9-28 => node specific

   // steppers:
   // GP22 => step clock input (3.125-MHz)
   // GP16-21 => 3x {step,dir}

   // PWM -> time counter input [0-10_000)

/*
   for (int i = 0; i < 15; i++) {
      gpio_put(LED_PIN, 1);
      sleep_ms(100);
      gpio_put(LED_PIN, 0);
      sleep_ms(100);
   }
*/

   // Main loop - USB service, fault watchdog
   // Interrupt by PSST - enqueue and/or forward
   // Interrupt by 1-kHz - process steppers and commands
   // Time synch: with idle PSST, restart interval pulse

   int blinky_on_off = 1;
   led_on_off(LED_PIN, blinky_on_off);

   for (;;) {
      sleep_ms(50);
      
      blinky_on_off = 1 - blinky_on_off;
      led_on_off(LED_PIN, blinky_on_off);

      tud_task();
      cdc_task();

      //if (tud_cdc_n_connected(0)) {
      //}

      // Cycle through 4 colors every 5 seconds
      switch ((pwm_count / 1250) % 4) {
      case 3:
	 put_pixel(0x00000000);	// off
	 break;
      case 2:
	 put_pixel(0x22000000);	// green
	 break;
      case 1:
	 put_pixel(0x00220000);	// red
	 break;
      case 0:
	 put_pixel(0x00002200);	// blue
	 break;
      }

      //put_pixel(0x00000022);	// -
      //gpio_put(LED_PIN, 1);
   }

}

//---------------------------------------------------------------------

void sbfmt_putc(struct sbfmt_buffer *sb, char c) {
   if (sb->p < sb->end) {
      *(sb->p) = c;
      sb->p++;
   }
}

void sbfmt_puts(struct sbfmt_buffer *sb, const char *s) {
   while (*s != '\0') {
      sbfmt_putc(sb, *s);
      s++;
   }
}

void sbfmt_int(
   struct sbfmt_buffer	*sb,
   int			value,
   int			width,
   char			pad
   )
{
   char tmp[16];
   tmp[15] = '\0';
   char *t = tmp+14;
   int sign = (value < 0);
   if (sign)
      value = -value;
   for (;;) {
      *t = '0' + value % 10;
      t--;
      value /= 10;
      if (value <= 0)
	 break;
   }
   if (sign) {
      *t = '-';
      t--;
   }
   return sbfmt_puts(sb, t);
}


/**/
