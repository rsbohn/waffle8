#include "paper_tape.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <stdbool.h>

/* Helper to report parsing errors with line numbers. */
static void paper_tape_report_error(size_t line_number, const char *fmt, ...) {
    va_list ap;
    fprintf(stderr, "paper_tape_load: line %zu: ", line_number);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, "\n");
}

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
            /* invalid character in bit string */
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
        /* empty or not a multiple of 12 bits */
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

/* Diagnose why a bit-text failed to parse and report a helpful message. */
static void analyze_bit_text(size_t line_number, const char *text) {
    size_t bit_count = 0;
    for (const char *p = text; *p != '\0'; ++p) {
        if (isspace((unsigned char)*p)) continue;
        if (*p == '#') break;
        if (*p != '0' && *p != '1') {
            paper_tape_report_error(line_number, "bit string contains non-binary character '%c'", *p);
            return;
        }
        ++bit_count;
    }
    if (bit_count == 0) {
        paper_tape_report_error(line_number, "bit string is empty");
        return;
    }
    if ((bit_count % 12u) != 0u) {
        paper_tape_report_error(line_number, "bit string length %zu is not a multiple of 12", bit_count);
        return;
    }
    size_t word_count = bit_count / 12u;
    if (word_count > PDP8_PAPER_TAPE_MAX_WORDS) {
        paper_tape_report_error(line_number, "bit string contains %zu words, exceeds max %u", word_count,
                                (unsigned)PDP8_PAPER_TAPE_MAX_WORDS);
        return;
    }
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
    /* Track parser used for each parsed block for a brief summary. */
    enum parser_type { PARSER_BITS = 0, PARSER_ASCII_OCTAL = 1 };
    size_t parser_used_capacity = 0;
    size_t parser_used_count = 0;
    int *parser_used = NULL; /* parallel array to image->blocks for quick reporting */
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
            paper_tape_report_error(line_number, "expected two-letter label at start of line");
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
                paper_tape_report_error(line_number, "malformed block number");
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
            paper_tape_report_error(line_number, "expected ':' after three-octal block number");
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
    int this_parser = -1;
    if (parse_bits(bit_text, &words, &word_count) != 0) {
            /* If parse_bits fails, try ASCII-octal token format (one ASCII byte per token). */
            /* Attempt to parse tokens like 070 101 123 or 012 for newline, octal 000-377. */
            const char *p = bit_text;
            bool any_octal = false;
            /* Count tokens first */
            size_t token_count = 0;
            while (*p) {
                while (isspace((unsigned char)*p)) ++p;
                if (*p == '\0' || *p == '#') break;
                char tok[4] = {0};
                size_t ti = 0;
                size_t actual_len = 0;
                while (*p && !isspace((unsigned char)*p) && *p != '#') {
                    if (ti < 3) tok[ti++] = *p;
                    ++actual_len;
                    ++p;
                }
                if (ti == 0) break;
                /* ensure all chars are octal digits and token is not too long */
                bool ok = true;
                for (size_t i = 0; i < ti; ++i) {
                    if (tok[i] < '0' || tok[i] > '7') { ok = false; break; }
                }
                if (!ok || actual_len > 3) {
                    any_octal = false;
                    break;
                }
                any_octal = true;
                ++token_count;
            }

            if (any_octal && token_count > 0) {
                /* allocate words and parse tokens into 12-bit words (zero-extended 8-bit ASCII)
                   each token is octal, range 000-377 (0-255 decimal) */
                if (token_count > PDP8_PAPER_TAPE_MAX_HALFWORDS) {
                    paper_tape_report_error(line_number, "ASCII-octal block contains %zu words, exceeds max %u",
                                            token_count, (unsigned)PDP8_PAPER_TAPE_MAX_HALFWORDS);
                    pdp8_paper_tape_destroy(image);
                    fclose(fp);
                    return -1;
                }
                words = (uint16_t *)calloc(token_count, sizeof(uint16_t));
                if (!words) {
                    pdp8_paper_tape_destroy(image);
                    fclose(fp);
                    return -1;
                }

                /* parse again to fill words */
                p = bit_text;
                size_t wi = 0;
                while (*p) {
                    while (isspace((unsigned char)*p)) ++p;
                    if (*p == '\0' || *p == '#') break;
                    char tok[4] = {0};
                    size_t ti = 0;
                    size_t actual_len = 0;
                    while (*p && !isspace((unsigned char)*p) && *p != '#') {
                        if (ti < 3) tok[ti++] = *p;
                        ++actual_len;
                        ++p;
                    }
                    if (ti == 0) break;
                    if (actual_len > 3) {
                        free(words);
                        paper_tape_report_error(line_number, "ASCII-octal token too long (%zu digits, max 3)", actual_len);
                        pdp8_paper_tape_destroy(image);
                        fclose(fp);
                        return -1;
                    }
                    char *endptr = NULL;
                    long val = strtol(tok, &endptr, 8);
                    if (!endptr || *endptr != '\0' || val < 0 || val > 0xFF) {
                        free(words);
                        paper_tape_report_error(line_number, "invalid ASCII-octal token '%s'", tok);
                        pdp8_paper_tape_destroy(image);
                        fclose(fp);
                        return -1;
                    }
                    words[wi++] = (uint16_t)(val & 0xFF);
                }
                word_count = wi;
                this_parser = PARSER_ASCII_OCTAL;
            } else {
                /* not an ASCII-octal line either */
                analyze_bit_text(line_number, bit_text);
                paper_tape_report_error(line_number, "failed to parse bit string for block %s", block_text);
                pdp8_paper_tape_destroy(image);
                fclose(fp);
                free(parser_used);
                return -1;
            }
        } else {
            this_parser = PARSER_BITS;
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
                paper_tape_report_error(line_number, "duplicate block number %s", block_text);
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
            free(parser_used);
            return -1;
        }

        pdp8_paper_tape_block *block = &image->blocks[image->block_count];
        block->block = block_value;
        block->word_count = word_count;
        block->words = words;
        image->block_count += 1;
        /* record parser used for this block */
        if (parser_used_count + 1 > parser_used_capacity) {
            size_t newcap = parser_used_capacity ? parser_used_capacity * 2 : 8;
            int *next = (int *)realloc(parser_used, newcap * sizeof(int));
            if (!next) {
                free(parser_used);
                paper_tape_report_error(line_number, "out of memory recording parser usage");
                pdp8_paper_tape_destroy(image);
                fclose(fp);
                return -1;
            }
            parser_used = next;
            parser_used_capacity = newcap;
        }
        parser_used[parser_used_count++] = this_parser;
    }

    fclose(fp);
    if (image->block_count == 0) {
        pdp8_paper_tape_destroy(image);
        free(parser_used);
        return -1;
    }

    /* Print a concise summary: loaded N blocks from the given file and which parser(s) were used. */
    const char *parser_name = "unknown";
    if (parser_used_count == 0) {
        parser_name = "none";
    } else {
        int first = parser_used[0];
        bool same = true;
        for (size_t i = 1; i < parser_used_count; ++i) {
            if (parser_used[i] != first) {
                same = false;
                break;
            }
        }
        if (same) {
            parser_name = (first == PARSER_BITS) ? "bits" : "ascii-octal";
        } else {
            parser_name = "mixed";
        }
    }
    fprintf(stderr,
            "paper_tape_load: Loaded %zu blocks from %s parser=%s label=%s\n",
            image->block_count,
            path ? path : "(unknown)",
            parser_name,
            image->label);
    free(parser_used);

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
