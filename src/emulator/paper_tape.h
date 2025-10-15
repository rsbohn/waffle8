#ifndef PDP8_PAPER_TAPE_H
#define PDP8_PAPER_TAPE_H

#include <stddef.h>
#include <stdint.h>

typedef struct pdp8_paper_tape_block {
    uint16_t block;            /* Block number (octal in source schema). */
    size_t word_count;         /* Number of 12-bit words in this block. */
    uint16_t *words;           /* Array of 12-bit words (upper bits zeroed). */
} pdp8_paper_tape_block;

typedef struct pdp8_paper_tape {
    char label[3];                    /* Two-character tape label, null-terminated. */
    size_t block_count;               /* Number of blocks parsed. */
    size_t block_capacity;            /* Allocated capacity for blocks array. */
    pdp8_paper_tape_block *blocks;    /* Block array. */
} pdp8_paper_tape;

#define PDP8_PAPER_TAPE_MAX_HALFWORDS 64u
#define PDP8_PAPER_TAPE_MAX_WORDS ((PDP8_PAPER_TAPE_MAX_HALFWORDS) / 2u)

int pdp8_paper_tape_load(const char *path, pdp8_paper_tape **out_image);
void pdp8_paper_tape_destroy(pdp8_paper_tape *image);
const pdp8_paper_tape_block *pdp8_paper_tape_find(const pdp8_paper_tape *image, uint16_t block);

#endif /* PDP8_PAPER_TAPE_H */
