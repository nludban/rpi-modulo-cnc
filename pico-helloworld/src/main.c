/*********************************************************************/
/*
 * pico-helloworld:main.c
 */
/*********************************************************************/

#include <pico/stdlib.h>
#include <pico/binary_info.h>

#include <hardware/pio.h>
#include <hardware/clocks.h>

// TinyUSB glue
#include <bsp/board.h>
#include <tusb.h>

#include "ws2812.pio.h"

//const uint LED_PIN = 25;	// pico (non-W)
const uint LED_PIN = 13;	// Metro rp2040 "Onboard #13 LED"
const uint WS2812_PIN = 14;	// Metro rp2040 "Onboard RGB NeoPixel"
const uint IS_RGBW = 0;

inline
uint32_t
ws2812_rgb(uint32_t r, uint32_t g, uint32_t b) {
   return ((((g << 8) + r) << 8) + b) << 8;
}

inline
void
put_pixel(PIO pio, uint sm, uint32_t grbx) {
   pio_sm_put_blocking(pio, sm, grbx);
}


/*
tusb_rhport_init_t dev_init = {
   .role = TUSB_ROLE_DEVICE,
   .speed = TUSB_SPEED_AUTO
};
*/

int main() {

   bi_decl(bi_program_description("First Blink"));
   bi_decl(bi_1pin_with_name(LED_PIN, "On-board LED"));

   board_init();
   tusb_init(/*BOARD_TUD_RHPORT, &dev_init*/);
   if (board_init_after_tusb)
      board_init_after_tusb();

   gpio_init(LED_PIN);
   gpio_set_dir(LED_PIN, GPIO_OUT);

   PIO pio;
   uint sm, offset;
   (void) pio_claim_free_sm_and_add_program_for_gpio_range(&ws2812_program,
							   &pio, &sm, &offset,
							   WS2812_PIN, 1, true);
   ws2812_program_init(pio, sm, offset, WS2812_PIN, 800000, IS_RGBW);

   for (;;) {
      put_pixel(pio, sm, 0x00000000);	// off
      gpio_put(LED_PIN, 0);
      sleep_ms(1000);

      put_pixel(pio, sm, 0x22000000);	// green
      gpio_put(LED_PIN, 1);
      sleep_ms(1000);

      put_pixel(pio, sm, 0x00220000);	// red
      gpio_put(LED_PIN, 1);
      sleep_ms(1000);

      put_pixel(pio, sm, 0x00002200);	// blue
      gpio_put(LED_PIN, 1);
      sleep_ms(1000);

      put_pixel(pio, sm, 0x00000022);	// -
      gpio_put(LED_PIN, 1);
      sleep_ms(1000);

   }

   for (;;) {
      gpio_put(LED_PIN, 0);	// Off
      sleep_ms(100);
      tud_task();
      //if (tud_cdc_n_connected(0)) {
	 gpio_put(LED_PIN, 1);	// On
	 sleep_ms(500);
      //}
   }
}

void tud_cdc_rx_cb(uint8_t itf) {
   gpio_put(LED_PIN, 1);	// On
   sleep_ms(100);
   uint8_t buf[CFG_TUD_CDC_RX_BUFSIZE];
   uint32_t count = tud_cdc_n_read(itf, buf, sizeof(buf));
   if ((itf == 0) && (count > 0))
      tud_cdc_n_write(itf, (const uint8_t *)"One\r\n", 5);
   if ((itf == 1) && (count > 0))
      tud_cdc_n_write(itf, (const uint8_t *)"Two\r\n", 5);
   tud_cdc_n_write_flush(itf);
}

/**/
