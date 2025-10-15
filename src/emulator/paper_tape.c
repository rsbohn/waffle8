#include "paper_tape.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int ensure_block_capacity(pdp8_paper_tape *image, size_t desired_count) {
    if (desired_count <= image->block_capacity) {
        return 0;
    }

    size_t capacity = image->block_capacity ? image->block_capacity : 4;
    while (capacity < desired_count) {
        capacity *= 2;
    }

    pdp8_paper_tape_block *next = (pdp8_paper_tape_block *)realloc(image->blocks,
        capacity * sizeof(pdp8_paper_tape_block));
    if (!next) {
        return -1;
    }

    image->blocks = next;
    image->block_capacity = capacity;
    return 0;
}

static int parse_bits(const char *text, uint16_t **out_words, size_t *out_word_count) {
    size_t bit_capacity = 64;
    size_t bit_count = 0;
    uint8_t *bits = (uint8_t *)malloc(bit_capacity * sizeof(uint8_t));
    if (!bits) {
        return -1;
    }

    for (const char *p = text; *p != '\0'; ++p) {
        if (isspace((unsigned char)*p)) {
            continue;
        }
        if (*p == '#') {
            break;
        }
        if (*p != '0' && *p != '1') {
            free(bits);
            return -1;
        }
        if (bit_count == bit_capacity) {
            size_t new_capacity = bit_capacity * 2;
            uint8_t *next = (uint8_t *)realloc(bits, new_capacity * sizeof(uint8_t));
            if (!next) {
                free(bits);
                return -1;
            }
            bits = next;
            bit_capacity = new_capacity;
        }
        bits[bit_count++] = (uint8_t)(*p - '0');
    }

    if (bit_count == 0 || (bit_count % 12u) != 0u) {
        free(bits);
        return -1;
    }

    size_t word_count = bit_count / 12u;
    if (word_count > PDP8_PAPER_TAPE_MAX_WORDS) {
        free(bits);
        return -1;
    }
    uint16_t *words = (uint16_t *)calloc(word_count, sizeof(uint16_t));
    if (!words) {
        free(bits);
        return -1;
    }

    for (size_t word_index = 0; word_index < word_count; ++word_index) {
        uint16_t value = 0;
        for (size_t bit_index = 0; bit_index < 12u; ++bit_index) {
            value = (uint16_t)((value << 1) | bits[word_index * 12u + bit_index]);
        }
        words[word_index] = (uint16_t)(value & 0x0FFFu);
    }

    free(bits);
    *out_words = words;
    *out_word_count = word_count;
    return 0;
}

int pdp8_paper_tape_load(const char *path, pdp8_paper_tape **out_image) {
    if (!path || !out_image) {
        return -1;
    }

    *out_image = NULL;

    FILE *fp = fopen(path, "r");
    if (!fp) {
        return -1;
    }

    pdp8_paper_tape *image = (pdp8_paper_tape *)calloc(1, sizeof(pdp8_paper_tape));
    if (!image) {
        fclose(fp);
        return -1;
    }

    char line[1024];
    size_t line_number = 0;
    while (fgets(line, sizeof(line), fp) != NULL) {
        ++line_number;

        char *cursor = line;
        while (isspace((unsigned char)*cursor)) {
            ++cursor;
        }
        if (*cursor == '\0' || *cursor == '#') {
            continue;
        }

        if (!isupper((unsigned char)cursor[0]) || !isupper((unsigned char)cursor[1])) {
            pdp8_paper_tape_destroy(image);
            fclose(fp);
            return -1;
        }

        char label[3] = {cursor[0], cursor[1], '\0'};
        cursor += 2;

        while (isspace((unsigned char)*cursor)) {
            ++cursor;
        }

        char block_text[4] = {0};
        size_t block_len = 0;
        while (*cursor != '\0' && *cursor != ':' && !isspace((unsigned char)*cursor)) {
            if (*cursor < '0' || *cursor > '7' || block_len >= 3) {
                pdp8_paper_tape_destroy(image);
                fclose(fp);
                return -1;
            }
            block_text[block_len++] = *cursor++;
        }

        while (isspace((unsigned char)*cursor)) {
            ++cursor;
        }

        if (*cursor != ':' || block_len != 3) {
            pdp8_paper_tape_destroy(image);
            fclose(fp);
            return -1;
        }
        ++cursor; /* Skip ':' */

        while (isspace((unsigned char)*cursor)) {
            ++cursor;
        }

        char *bit_text = cursor;
        char *newline = strchr(bit_text, '\n');
        if (newline) {
            *newline = '\0';
        }

        uint16_t *words = NULL;
        size_t word_count = 0;
        if (parse_bits(bit_text, &words, &word_count) != 0) {
            pdp8_paper_tape_destroy(image);
            fclose(fp);
            return -1;
        }

        if (image->label[0] == '\0') {
            memcpy(image->label, label, sizeof(label));
        } else if (strncmp(image->label, label, sizeof(label)) != 0) {
            free(words);
            pdp8_paper_tape_destroy(image);
            fclose(fp);
            return -1;
        }

        uint16_t block_value = (uint16_t)strtoul(block_text, NULL, 8);
        for (size_t i = 0; i < image->block_count; ++i) {
            if (image->blocks[i].block == block_value) {
                free(words);
                pdp8_paper_tape_destroy(image);
                fclose(fp);
                return -1;
            }
        }

        if (ensure_block_capacity(image, image->block_count + 1) != 0) {
            free(words);
            pdp8_paper_tape_destroy(image);
            fclose(fp);
            return -1;
        }

        pdp8_paper_tape_block *block = &image->blocks[image->block_count];
        block->block = block_value;
        block->word_count = word_count;
        block->words = words;
        image->block_count += 1;
    }

    fclose(fp);
    if (image->block_count == 0) {
        pdp8_paper_tape_destroy(image);
        return -1;
    }

    *out_image = image;
    return 0;
}

void pdp8_paper_tape_destroy(pdp8_paper_tape *image) {
    if (!image) {
        return;
    }
    if (image->blocks) {
        for (size_t i = 0; i < image->block_count; ++i) {
            free(image->blocks[i].words);
        }
        free(image->blocks);
    }
    free(image);
}

const pdp8_paper_tape_block *pdp8_paper_tape_find(const pdp8_paper_tape *image, uint16_t block) {
    if (!image) {
        return NULL;
    }
    for (size_t i = 0; i < image->block_count; ++i) {
        if (image->blocks[i].block == block) {
            return &image->blocks[i];
        }
    }
    return NULL;
}
