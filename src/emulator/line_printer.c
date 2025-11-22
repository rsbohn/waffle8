#include "line_printer.h"

#include "pdp8.h"

#include <stdbool.h>
#include <stdlib.h>

#define PDP8_LINE_PRINTER_DEFAULT_COLUMN_LIMIT 132u

struct pdp8_line_printer {
    FILE *stream;
    uint16_t column_limit;
    uint16_t column;
    bool ready;
    bool color_active;
    pdp8_line_printer_output_callback output_callback;
    void *output_context;
};

static void line_printer_output_char(pdp8_line_printer_t *printer, uint8_t value) {
    if (!printer) {
        return;
    }
    if (printer->stream) {
        fputc((int)value, printer->stream);
    }
    if (printer->output_callback) {
        printer->output_callback(value, printer->output_context);
    }
}

static void line_printer_start_color(pdp8_line_printer_t *printer) {
    if (!printer || !printer->stream || printer->color_active) {
        return;
    }
    fputs("\x1b[33m", printer->stream);
    printer->color_active = true;
}

static void line_printer_stop_color(pdp8_line_printer_t *printer) {
    if (!printer || !printer->stream || !printer->color_active) {
        return;
    }
    fputs("\x1b[0m", printer->stream);
    printer->color_active = false;
}

static void line_printer_emit(pdp8_line_printer_t *printer, uint8_t ch) {
    if (!printer) {
        return;
    }

    if (ch == '\r') {
        line_printer_stop_color(printer);
        line_printer_output_char(printer, '\r');
        printer->column = 0;
    } else if (ch == '\n') {
        line_printer_stop_color(printer);
        line_printer_output_char(printer, '\n');
        printer->column = 0;
    } else if (ch == '\f') {
        line_printer_stop_color(printer);
        line_printer_output_char(printer, '\f');
        printer->column = 0;
    } else if (ch == '\t') {
        line_printer_start_color(printer);
        uint16_t spaces = 8u - (printer->column % 8u);
        for (uint16_t i = 0; i < spaces; ++i) {
            line_printer_output_char(printer, ' ');
        }
        printer->column = (uint16_t)(printer->column + spaces);
        line_printer_stop_color(printer);
    } else {
        line_printer_start_color(printer);
        if (ch < 0x20u) {
            ch = '?';
        }
        line_printer_output_char(printer, ch);
        printer->column = (uint16_t)(printer->column + 1u);
        if (printer->column_limit > 0u && printer->column >= printer->column_limit) {
            line_printer_stop_color(printer);
            line_printer_output_char(printer, '\n');
            printer->column = 0;
        } else {
            line_printer_stop_color(printer);
        }
    }
    if (printer->stream) {
        fflush(printer->stream);
    }
}

static void line_printer_iot(pdp8_t *cpu, uint16_t instruction, void *context) {
    pdp8_line_printer_t *printer = (pdp8_line_printer_t *)context;
    if (!printer) {
        return;
    }

    uint8_t microcode = (uint8_t)(instruction & 0x7u);

    if (microcode & 0x1u) {
        if (printer->ready) {
            pdp8_api_request_skip(cpu);
        }
    }

    if (microcode & 0x2u) {
        printer->ready = false;
    }

    if (microcode & 0x4u) {
        uint16_t ac = pdp8_api_get_ac(cpu);
        uint8_t ch = (uint8_t)(ac & 0x7Fu);
        line_printer_emit(printer, ch);
        printer->ready = true;
    }
}

pdp8_line_printer_t *pdp8_line_printer_create(FILE *stream) {
    pdp8_line_printer_t *printer = (pdp8_line_printer_t *)calloc(1, sizeof(pdp8_line_printer_t));
    if (!printer) {
        return NULL;
    }
    printer->stream = stream ? stream : stdout;
    printer->column_limit = PDP8_LINE_PRINTER_DEFAULT_COLUMN_LIMIT;
    printer->column = 0;
    printer->ready = true;
    printer->color_active = false;
    return printer;
}

void pdp8_line_printer_destroy(pdp8_line_printer_t *printer) {
    if (!printer) {
        return;
    }
    if (printer->stream) {
        line_printer_stop_color(printer);
        fflush(printer->stream);
    }
    free(printer);
}

int pdp8_line_printer_attach(pdp8_t *cpu, pdp8_line_printer_t *printer) {
    if (!cpu || !printer) {
        return -1;
    }
    return pdp8_api_register_iot(cpu, PDP8_LINE_PRINTER_DEVICE_CODE, line_printer_iot, printer);
}

int pdp8_line_printer_set_column_limit(pdp8_line_printer_t *printer, uint16_t columns) {
    if (!printer) {
        return -1;
    }
    printer->column_limit = columns;
    return 0;
}

int pdp8_line_printer_set_stream(pdp8_line_printer_t *printer, FILE *stream) {
    if (!printer) {
        return -1;
    }
    printer->stream = stream;
    return 0;
}

int pdp8_line_printer_set_output_callback(pdp8_line_printer_t *printer,
                                          pdp8_line_printer_output_callback callback,
                                          void *context) {
    if (!printer) {
        return -1;
    }
    printer->output_callback = callback;
    printer->output_context = context;
    return 0;
}
