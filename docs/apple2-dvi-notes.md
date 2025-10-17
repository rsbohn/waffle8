# Apple //e Reload DVI Integration Notes

The Fruit Jam Apple //e port under `/opt/fruitjam-apple2` drives HDMI via the
RP2350 HSTX engine. This document captures the relevant configuration snippets
for reusing its DVI setup.

## Board pinout and power

```c
#define HSTX_CKP 13
#define HSTX_D0P 15
#define HSTX_D1P 17
#define HSTX_D2P 19
#define PIN_USB_HOST_VBUS (11u)
```

The TMDS clock sits on GPIO 13 with data lanes on GPIO 15/17/19, and USB VBUS
power is switched through GPIO 11.【F:docs/apple2-dvi-notes.md†L11-L18】

```c
#define FRAME_WIDTH  640
#define FRAME_HEIGHT 480
#define VREG_VSEL    VREG_VOLTAGE_1_20
```

`main()` sets the RP2350 regulator to 1.2 V and boosts the system clock to
264 MHz before continuing with display bring-up, matching the 400 MHz TMDS bit
clock requirement called out in the source.【F:docs/apple2-dvi-notes.md†L20-L29】

## Runtime bring-up sequence

```c
if (!common_hal_picodvi_framebuffer_construct(&picodvi, 640, 240,
        HSTX_CKP, HSTX_D0P, HSTX_D1P, HSTX_D2P, 8)) {
    abort();
}
multicore_launch_core1(core1_main);
```

The primary core allocates a 640×240 RGB332 framebuffer on the TMDS pins above
and spawns the second core to feed the DVI engine.【F:docs/apple2-dvi-notes.md†L31-L39】

```c
common_hal_picodvi_framebuffer_start(&picodvi);
while (1) {
    audio_handle_buffer();
}
```

Core 1 continuously services audio and keeps the TMDS pipeline running, while
core 0 updates Apple //e video and copies it into the RGB332 lookup texture each
frame.【F:docs/apple2-dvi-notes.md†L41-L49】

## HSTX timing, scaling, and DMA

```c
uint32_t freq = 125875000;
clock_configure(clk_hstx, ..., freq);
if (width % 160 == 0) {
    self->output_width = 640;
    self->output_height = 480;
}
```

The helper programs the HSTX PLL for ~125.875 MHz TMDS and chooses 640×480
output timing whenever the logical width is a multiple of 160 pixels (the Apple
//e framebuffer is 640×240, so the pipeline doubles scanlines).【F:docs/apple2-dvi-notes.md†L51-L61】

```c
self->dma_commands = malloc(...);
self->dma_pixel_channel = dma_claim_unused_channel(false);
self->dma_command_channel = dma_claim_unused_channel(false);
...
hstx_ctrl_hw->expand_tmds = ... // RGB332 coefficients
...
hstx_ctrl_hw->bit[bit] = lane_data_sel_bits;
```

DMA command and pixel channels cooperate to stream porch/sync control words and
RGB332 pixels into the HSTX FIFO, while the TMDS expanders are tuned for 8-bit
RGB332 data and each TMDS lane is bound to its GPIO pair before enabling the
pipeline interrupt handler.【F:docs/apple2-dvi-notes.md†L63-L77】

