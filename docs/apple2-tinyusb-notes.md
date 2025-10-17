# Apple //e Reload TinyUSB Host Notes

The Fruit Jam Apple //e port under `/opt/fruitjam-apple2` exposes a full
TinyUSB host stack running on the RP2350. These notes capture the pieces we
need when wiring the PDP-8 monitor port.

## Host pinout and power sequencing

- D+ lives on GPIO 20 and D− on GPIO 19 (`PIN_USB_HOST_DP/DM`).
- GPIO 11 switches the 5 V host rail (`PIN_USB_HOST_VBUS`) and is driven high
  before TinyUSB initialisation so downstream devices receive power.
- The build uses Pico-PIO-USB, so the `pio_usb_configuration_t` handed to
  TinyUSB selects the DP/DM ordering, binds the pair to PIO 1, and reserves DMA
  channel 9 for transmit traffic before calling `tuh_configure()`.

## TinyUSB configuration knobs

`platforms/rp2040/src/tusb_config.h` enables the TinyUSB host stack, the PIO USB
bridge, and allocates plenty of HID slots for composite keyboards:

```c
#define CFG_TUH_ENABLED     1
#define CFG_TUH_RPI_PIO_USB 1
#define CFG_TUH_HID         4
#define CFG_TUH_DEVICE_MAX  (CFG_TUH_HUB ? 5 : 1)
```

Those settings allow one upstream hub plus four HID interfaces, aligning with
typical keyboard+mouse combos.

## Runtime initialisation flow

1. Boost the regulator (`vreg_set_voltage()`) and system clock to 264 MHz.
2. Initialise stdio for debug prints and configure the `pio_usb_configuration_t`
   structure based on the board pinout.
3. Enable VBUS on GPIO 11 and hand the PIO USB config to TinyUSB via
   `tuh_configure()`.
4. Call `tusb_init()` before touching DVI or the emulator so enumeration can
   start immediately.
5. Once the main loop is running, pump `tuh_task()` each frame after the emulator
   tick burst and before sleeping, keeping control transfers flowing.

## Keyboard report handling

- TinyUSB provides HID callbacks in `hid_app.c`. `tuh_hid_mount_cb()` requests
  the first report after identifying whether a device is a keyboard or a
  gamepad.
- When `tuh_hid_report_received_cb()` sees a keyboard report it calls
  `find_pressed_keys()`/`find_released_keys()` which walk the 6-key rollover
  array, translate HID codes through `keycode_to_ascii_table`, and synthesise
  control characters when modifiers are held.
- Shift toggles the upper ASCII column, Ctrl strips bit 0x60 to generate
  `^A`-style control codes, and any value outside the ASCII table is forwarded as
  `0x100 | keycode` so platform code can handle arrows and function keys.

These translations ultimately call the board-provided `kbd_raw_key_down()` and
`kbd_raw_key_up()` hooks, giving us a ready-made path for feeding bytes into the
KL8E queue once `monitor_platform_enqueue_key()` is wired up.
