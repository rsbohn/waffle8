#ifndef PDP8_LINE_PRINTER_H
#define PDP8_LINE_PRINTER_H

#include <stdio.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PDP8_LINE_PRINTER_DEVICE_CODE 060u
#define PDP8_LINE_PRINTER_IOT_BASE (06000u | ((PDP8_LINE_PRINTER_DEVICE_CODE & 0x3Fu) << 3))
#define PDP8_LINE_PRINTER_BIT_SKIP 0x1u
#define PDP8_LINE_PRINTER_BIT_CLEAR 0x2u
#define PDP8_LINE_PRINTER_BIT_PRINT 0x4u
#define PDP8_LINE_PRINTER_INSTR(bits) (PDP8_LINE_PRINTER_IOT_BASE | (uint16_t)((bits) & 0x7u))

typedef struct pdp8_line_printer pdp8_line_printer_t;
typedef struct pdp8 pdp8_t;

pdp8_line_printer_t *pdp8_line_printer_create(FILE *stream);
void pdp8_line_printer_destroy(pdp8_line_printer_t *printer);
int pdp8_line_printer_attach(pdp8_t *cpu, pdp8_line_printer_t *printer);
int pdp8_line_printer_set_column_limit(pdp8_line_printer_t *printer, uint16_t columns);

#ifdef __cplusplus
}
#endif

#endif
