#include "paper_tape_punch.h"

#include "pdp8.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

struct pdp8_paper_tape_punch {
    FILE *stream;
    bool owns_stream;
    bool ready;
    size_t bytes_written;
    pdp8_paper_tape_punch_output_callback callback;
    void *callback_context;
};

static void punch_close_stream(struct pdp8_paper_tape_punch *punch) {
    if (!punch) {
        return;
    }
    if (punch->owns_stream && punch->stream) {
        fclose(punch->stream);
    }
    punch->stream = NULL;
    punch->owns_stream = false;
}

static void punch_emit(struct pdp8_paper_tape_punch *punch, uint8_t value) {
    if (!punch) {
        return;
    }
    if (punch->stream) {
        fputc((int)value, punch->stream);
        fflush(punch->stream);
    }
    if (punch->callback) {
        punch->callback(value, punch->callback_context);
    }
    punch->bytes_written++;
}

static void paper_tape_punch_iot(pdp8_t *cpu, uint16_t instruction, void *context) {
    struct pdp8_paper_tape_punch *punch = (struct pdp8_paper_tape_punch *)context;
    if (!cpu || !punch) {
        return;
    }

    uint8_t microcode = (uint8_t)(instruction & 0x7u);

    if ((microcode & PDP8_PAPER_TAPE_PUNCH_BIT_SKIP) && punch->ready) {
        pdp8_api_request_skip(cpu);
    }

    if (microcode & PDP8_PAPER_TAPE_PUNCH_BIT_CLEAR) {
        punch->ready = false;
    }

    if (microcode & PDP8_PAPER_TAPE_PUNCH_BIT_PUNCH) {
        uint16_t ac = pdp8_api_get_ac(cpu);
        uint8_t output = (uint8_t)(ac & 0xFFu);
        punch->ready = false;
        punch_emit(punch, output);
        punch->ready = true;
    }
}

pdp8_paper_tape_punch_t *pdp8_paper_tape_punch_create(void) {
    struct pdp8_paper_tape_punch *punch =
        (struct pdp8_paper_tape_punch *)calloc(1, sizeof(struct pdp8_paper_tape_punch));
    if (!punch) {
        return NULL;
    }
    punch->stream = NULL;
    punch->owns_stream = false;
    punch->ready = true;
    punch->bytes_written = 0u;
    punch->callback = NULL;
    punch->callback_context = NULL;
    return punch;
}

void pdp8_paper_tape_punch_destroy(pdp8_paper_tape_punch_t *punch_ptr) {
    struct pdp8_paper_tape_punch *punch = (struct pdp8_paper_tape_punch *)punch_ptr;
    if (!punch) {
        return;
    }
    punch_close_stream(punch);
    free(punch);
}

int pdp8_paper_tape_punch_attach(pdp8_t *cpu, pdp8_paper_tape_punch_t *punch_ptr) {
    if (!cpu || !punch_ptr) {
        return -1;
    }
    return pdp8_api_register_iot(cpu,
                                 PDP8_PAPER_TAPE_PUNCH_DEVICE_CODE,
                                 paper_tape_punch_iot,
                                 punch_ptr);
}

int pdp8_paper_tape_punch_set_stream(pdp8_paper_tape_punch_t *punch_ptr, FILE *stream) {
    struct pdp8_paper_tape_punch *punch = (struct pdp8_paper_tape_punch *)punch_ptr;
    if (!punch) {
        return -1;
    }
    if (punch->stream == stream && !punch->owns_stream) {
        return 0;
    }
    punch_close_stream(punch);
    punch->stream = stream;
    punch->owns_stream = false;
    return 0;
}

int pdp8_paper_tape_punch_set_output_path(pdp8_paper_tape_punch_t *punch_ptr, const char *path) {
    struct pdp8_paper_tape_punch *punch = (struct pdp8_paper_tape_punch *)punch_ptr;
    if (!punch) {
        return -1;
    }

    punch_close_stream(punch);

    if (!path || !*path) {
        return 0;
    }

    if (strcmp(path, "stdout") == 0 || strcmp(path, "-") == 0) {
        punch->stream = stdout;
        punch->owns_stream = false;
        return 0;
    }
    if (strcmp(path, "stderr") == 0) {
        punch->stream = stderr;
        punch->owns_stream = false;
        return 0;
    }

    FILE *fp = fopen(path, "wb");
    if (!fp) {
        return -1;
    }
    punch->stream = fp;
    punch->owns_stream = true;
    return 0;
}

int pdp8_paper_tape_punch_set_output_callback(pdp8_paper_tape_punch_t *punch_ptr,
                                              pdp8_paper_tape_punch_output_callback callback,
                                              void *context) {
    struct pdp8_paper_tape_punch *punch = (struct pdp8_paper_tape_punch *)punch_ptr;
    if (!punch) {
        return -1;
    }
    punch->callback = callback;
    punch->callback_context = context;
    return 0;
}

size_t pdp8_paper_tape_punch_bytes_written(const pdp8_paper_tape_punch_t *punch_ptr) {
    const struct pdp8_paper_tape_punch *punch =
        (const struct pdp8_paper_tape_punch *)punch_ptr;
    return punch ? punch->bytes_written : 0u;
}
