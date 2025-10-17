#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "../src/emulator/pdp8.h"
#include "../src/emulator/pdp8_board.h"
#include "../src/emulator/kl8e_console.h"
#include "../src/emulator/line_printer.h"
#include "../src/emulator/paper_tape.h"
#include "../src/emulator/paper_tape_device.h"

#define ASSERT_EQ(label, expected, actual)                                                             \
    do {                                                                                               \
        if ((expected) != (actual)) {                                                                  \
            fprintf(stderr, "Assertion failed: %s (expected %06o, got %06o)\n", label, expected, actual); \
            return 0;                                                                                  \
        }                                                                                              \
    } while (0)

#define ASSERT_TRUE(label, condition)                                                                  \
    do {                                                                                               \
        if (!(condition)) {                                                                            \
            fprintf(stderr, "Assertion failed: %s\n", label);                                          \
            return 0;                                                                                  \
        }                                                                                              \
    } while (0)

#define ASSERT_INT_EQ(label, expected, actual)                                                         \
    do {                                                                                               \
        long long _exp = (long long)(expected);                                                        \
        long long _act = (long long)(actual);                                                          \
        if (_exp != _act) {                                                                            \
            fprintf(stderr, "Assertion failed: %s (expected %lld, got %lld)\n", label, _exp, _act);    \
            return 0;                                                                                  \
        }                                                                                              \
    } while (0)

#define ASSERT_STR_EQ(label, expected, actual)                                                         \
    do {                                                                                               \
        const char *_exp = (expected);                                                                 \
        const char *_act = (actual);                                                                   \
        if ((_exp == NULL && _act != NULL) || (_exp != NULL && _act == NULL) ||                        \
            (_exp && _act && strcmp(_exp, _act) != 0)) {                                               \
            fprintf(stderr, "Assertion failed: %s\n", label);                                          \
            return 0;                                                                                  \
        }                                                                                              \
    } while (0)

static int test_memory_reference(void) {
    pdp8_t *cpu = pdp8_api_create(4096);
    if (!cpu) {
        fprintf(stderr, "Unable to allocate CPU\n");
        return 0;
    }

    /* TAD direct */
    pdp8_api_set_ac(cpu, 0400);
    pdp8_api_write_mem(cpu, 0000, 01010); /* TAD 0010 */
    pdp8_api_write_mem(cpu, 0001, 07402); /* HLT */
    pdp8_api_write_mem(cpu, 0010, 0200);
    pdp8_api_set_pc(cpu, 0000);
    pdp8_api_step(cpu);
    ASSERT_EQ("TAD result", 0600, pdp8_api_get_ac(cpu));
    ASSERT_EQ("Link after TAD", 0, pdp8_api_get_link(cpu));

    /* ISZ with skip */
    pdp8_api_write_mem(cpu, 0002, 02020); /* ISZ 0020 */
    pdp8_api_write_mem(cpu, 0003, 07402); /* HLT */
    pdp8_api_write_mem(cpu, 0020, 07777);
    pdp8_api_set_pc(cpu, 0002);
    pdp8_api_step(cpu);
    ASSERT_EQ("ISZ increments", 00000, pdp8_api_read_mem(cpu, 0020));
    ASSERT_EQ("ISZ skip", 0004, pdp8_api_get_pc(cpu));

    /* JMS stores PC and jumps */
    pdp8_api_write_mem(cpu, 0004, 04030); /* JMS 0030 */
    pdp8_api_write_mem(cpu, 0030, 07402); /* HLT */
    pdp8_api_write_mem(cpu, 0031, 00000);
    pdp8_api_set_pc(cpu, 0004);
    pdp8_api_step(cpu);
    ASSERT_EQ("Return address stored", 0005, pdp8_api_read_mem(cpu, 0030));
    ASSERT_EQ("PC after JMS", 0031, pdp8_api_get_pc(cpu));

    /* JMP */
    pdp8_api_write_mem(cpu, 0031, 05040); /* JMP 0040 */
    pdp8_api_write_mem(cpu, 0040, 07402);
    pdp8_api_step(cpu);
    ASSERT_EQ("JMP target", 0040, pdp8_api_get_pc(cpu));

    pdp8_api_destroy(cpu);
    return 1;
}

static int test_indirect_and_auto_increment(void) {
    pdp8_t *cpu = pdp8_api_create(4096);
    if (!cpu) {
        return 0;
    }

    pdp8_api_write_mem(cpu, 0000, 01010 | 00400); /* TAD I 0010 */
    pdp8_api_write_mem(cpu, 0001, 07402);
    pdp8_api_write_mem(cpu, 0010, 00020); /* pointer */
    pdp8_api_write_mem(cpu, 00021, 00005);

    pdp8_api_set_pc(cpu, 0);
    pdp8_api_step(cpu);
    ASSERT_EQ("Indirect fetch", 00005, pdp8_api_get_ac(cpu));
    ASSERT_EQ("Auto-increment", 00021, pdp8_api_read_mem(cpu, 0010));

    pdp8_api_destroy(cpu);
    return 1;
}

static int test_operate_group1(void) {
    pdp8_t *cpu = pdp8_api_create(4096);
    if (!cpu) {
        return 0;
    }

    pdp8_api_set_ac(cpu, 01234);
    pdp8_api_set_link(cpu, 1);
    pdp8_api_write_mem(cpu, 0000, 07200); /* CLA */
    pdp8_api_write_mem(cpu, 0001, 07040); /* CMA */
    pdp8_api_write_mem(cpu, 0002, 07001); /* IAC */
    pdp8_api_write_mem(cpu, 0003, 07010); /* RAR */
    pdp8_api_write_mem(cpu, 0004, 07402); /* HLT */

    pdp8_api_set_pc(cpu, 0000);
    pdp8_api_step(cpu);
    ASSERT_EQ("CLA clears AC", 00000, pdp8_api_get_ac(cpu));
    pdp8_api_step(cpu);
    ASSERT_EQ("CMA", 07777, pdp8_api_get_ac(cpu));
    pdp8_api_step(cpu);
    ASSERT_EQ("IAC", 00000, pdp8_api_get_ac(cpu));
    ASSERT_EQ("IAC link", 0, pdp8_api_get_link(cpu));
    pdp8_api_set_ac(cpu, 00000);
    pdp8_api_set_link(cpu, 1);
    pdp8_api_step(cpu);
    ASSERT_EQ("RAR rotates AC", 04000, pdp8_api_get_ac(cpu));
    ASSERT_EQ("RAR link", 0, pdp8_api_get_link(cpu));

    pdp8_api_destroy(cpu);
    return 1;
}

static int test_operate_group2(void) {
    pdp8_t *cpu = pdp8_api_create(4096);
    if (!cpu) {
        return 0;
    }

    pdp8_api_set_ac(cpu, 04000);
    pdp8_api_set_link(cpu, 1);
    pdp8_api_set_switch_register(cpu, 00012);

    pdp8_api_write_mem(cpu, 0000, 07600); /* CLA */
    pdp8_api_write_mem(cpu, 0001, 07500); /* SMA (negative -> skip) */
    pdp8_api_write_mem(cpu, 0002, 07404); /* OSR */
    pdp8_api_write_mem(cpu, 0003, 07402); /* HLT */

    pdp8_api_set_pc(cpu, 0000);
    pdp8_api_step(cpu);
    ASSERT_EQ("Group2 CLA", 00000, pdp8_api_get_ac(cpu));

    pdp8_api_set_ac(cpu, 04000); /* negative because bit 11 set */
    pdp8_api_step(cpu); /* SMA -> skip */
    ASSERT_EQ("SMA skip", 0003, pdp8_api_get_pc(cpu));

    pdp8_api_set_pc(cpu, 0002);
    pdp8_api_set_ac(cpu, 00000);
    pdp8_api_step(cpu);
    ASSERT_EQ("OSR merges switch register", 00012, pdp8_api_get_ac(cpu));

    pdp8_api_destroy(cpu);
    return 1;
}

struct iot_state {
    int invoked;
};

static void sample_iot(pdp8_t *cpu, uint16_t instruction, void *context) {
    (void)instruction;
    struct iot_state *state = (struct iot_state *)context;
    state->invoked += 1;
    pdp8_api_request_skip(cpu);
}

static int test_iot(void) {
    pdp8_t *cpu = pdp8_api_create(4096);
    if (!cpu) {
        return 0;
    }

    struct iot_state state = {0};
    pdp8_api_register_iot(cpu, 03, sample_iot, &state);

    pdp8_api_write_mem(cpu, 0000, 06030); /* device 03 */
    pdp8_api_write_mem(cpu, 0001, 07402);
    pdp8_api_set_pc(cpu, 0);
    pdp8_api_step(cpu);
    ASSERT_EQ("IOT invoked", 1, state.invoked);
    ASSERT_EQ("IOT skip", 0002, pdp8_api_get_pc(cpu));

    pdp8_api_destroy(cpu);
    return 1;
}

static int test_clear_halt(void) {
    pdp8_t *cpu = pdp8_api_create(4096);
    if (!cpu) {
        return 0;
    }

    pdp8_api_write_mem(cpu, 0000, 07402); /* HLT */
    pdp8_api_set_pc(cpu, 0000);
    ASSERT_INT_EQ("execute HLT", 1, pdp8_api_step(cpu));
    ASSERT_TRUE("halt flag set", pdp8_api_is_halted(cpu) != 0);

    pdp8_api_clear_halt(cpu);
    ASSERT_TRUE("halt flag cleared", pdp8_api_is_halted(cpu) == 0);

    pdp8_api_write_mem(cpu, 0001, 07001); /* IAC */
    pdp8_api_set_ac(cpu, 07777);
    pdp8_api_set_pc(cpu, 0001);
    ASSERT_INT_EQ("run executes after clear", 1, pdp8_api_run(cpu, 1));
    ASSERT_EQ("IAC result", 00000, pdp8_api_get_ac(cpu));

    pdp8_api_destroy(cpu);
    return 1;
}

static int test_kl8e_console(void) {
    pdp8_t *cpu = pdp8_api_create(4096);
    if (!cpu) {
        return 0;
    }

    FILE *sink = tmpfile();
    if (!sink) {
        pdp8_api_destroy(cpu);
        return 0;
    }

    pdp8_kl8e_console_t *console = pdp8_kl8e_console_create(NULL, sink);
    if (!console) {
        fclose(sink);
        pdp8_api_destroy(cpu);
        return 0;
    }

    if (pdp8_kl8e_console_attach(cpu, console) != 0) {
        pdp8_kl8e_console_destroy(console);
        fclose(sink);
        pdp8_api_destroy(cpu);
        return 0;
    }

    ASSERT_INT_EQ("queue input", 0, pdp8_kl8e_console_queue_input(console, 'A'));

    pdp8_api_write_mem(cpu, 0000, PDP8_KL8E_KEYBOARD_INSTR(PDP8_KL8E_KEYBOARD_BIT_SKIP));
    pdp8_api_write_mem(cpu, 0001, 00000);
    pdp8_api_write_mem(cpu, 0002,
                       PDP8_KL8E_KEYBOARD_INSTR(PDP8_KL8E_KEYBOARD_BIT_CLEAR | PDP8_KL8E_KEYBOARD_BIT_READ));
    pdp8_api_write_mem(cpu, 0003, PDP8_KL8E_TELEPRINTER_INSTR(PDP8_KL8E_TELEPRINTER_BIT_SKIP));
    pdp8_api_write_mem(cpu, 0004, 00000);
    pdp8_api_write_mem(cpu, 0005,
                       PDP8_KL8E_TELEPRINTER_INSTR(PDP8_KL8E_TELEPRINTER_BIT_CLEAR | PDP8_KL8E_TELEPRINTER_BIT_LOAD));
    pdp8_api_write_mem(cpu, 0006, 07402);

    pdp8_api_set_pc(cpu, 0000);
    pdp8_api_step(cpu);
    ASSERT_EQ("KSF skip", 0002, pdp8_api_get_pc(cpu));

    pdp8_api_step(cpu);
    ASSERT_EQ("KRB loads AC", 'A' & 0x7F, pdp8_api_get_ac(cpu));
    ASSERT_INT_EQ("input consumed", 0, (int)pdp8_kl8e_console_input_pending(console));

    pdp8_api_step(cpu);
    ASSERT_EQ("TSF skip", 0005, pdp8_api_get_pc(cpu));

    pdp8_api_step(cpu);
    ASSERT_INT_EQ("output buffered", 1, (int)pdp8_kl8e_console_output_pending(console));
    uint8_t out_ch = 0;
    ASSERT_INT_EQ("pop output", 0, pdp8_kl8e_console_pop_output(console, &out_ch));
    ASSERT_EQ("output char", 'A' & 0x7F, out_ch);

    pdp8_kl8e_console_destroy(console);
    fclose(sink);
    pdp8_api_destroy(cpu);
    return 1;
}

static int test_line_printer(void) {
    pdp8_t *cpu = pdp8_api_create(4096);
    if (!cpu) {
        return 0;
    }

    FILE *sink = tmpfile();
    if (!sink) {
        pdp8_api_destroy(cpu);
        return 0;
    }

    pdp8_line_printer_t *printer = pdp8_line_printer_create(sink);
    ASSERT_TRUE("line printer created", printer != NULL);
    ASSERT_INT_EQ("line printer attach", 0, pdp8_line_printer_attach(cpu, printer));

    pdp8_api_write_mem(cpu, 0000, 07200); /* CLA */
    pdp8_api_write_mem(cpu, 0001, 01010); /* TAD 0010 */
    pdp8_api_write_mem(cpu, 0002,
                       PDP8_LINE_PRINTER_INSTR(PDP8_LINE_PRINTER_BIT_CLEAR | PDP8_LINE_PRINTER_BIT_PRINT));
    pdp8_api_write_mem(cpu, 0003, PDP8_LINE_PRINTER_INSTR(PDP8_LINE_PRINTER_BIT_SKIP));
    pdp8_api_write_mem(cpu, 0004, 07402); /* HLT */
    pdp8_api_write_mem(cpu, 0005, 07402); /* HLT (skip target) */
    pdp8_api_write_mem(cpu, 0010, 00101); /* ASCII 'A' */
    pdp8_api_set_pc(cpu, 0000);

    pdp8_api_run(cpu, 16);
    ASSERT_INT_EQ("halted after print", 1, pdp8_api_is_halted(cpu));
    ASSERT_EQ("PC after skip-ready", 0006, pdp8_api_get_pc(cpu));

    fflush(sink);
    long position = ftell(sink);
    ASSERT_TRUE("line printer wrote data", position >= 1);
    fseek(sink, 0, SEEK_SET);
    char buffer[4] = {0};
    size_t len = fread(buffer, 1, sizeof(buffer) - 1, sink);
    ASSERT_INT_EQ("printed length", 1, len);
    ASSERT_STR_EQ("printed character", "A", buffer);

    pdp8_line_printer_destroy(printer);
    fclose(sink);
    pdp8_api_destroy(cpu);
    return 1;
}

static int test_paper_tape_parser(void) {
    const char *path = "paper_tape_parser.tmp";
    FILE *fp = fopen(path, "w");
    if (!fp) {
        return 0;
    }

    fprintf(fp, "AA001: 000000 000001 111111 000000\n");
    fprintf(fp, "AA002: 101010 101010 010101 010101\n");
    fclose(fp);

    pdp8_paper_tape *image = NULL;
    if (pdp8_paper_tape_load(path, &image) != 0) {
        remove(path);
        return 0;
    }

    ASSERT_TRUE("tape image present", image != NULL);
    ASSERT_STR_EQ("tape label", "AA", image->label);
    ASSERT_INT_EQ("two blocks parsed", 2, (int)image->block_count);

    const pdp8_paper_tape_block *block1 = pdp8_paper_tape_find(image, 0001);
    ASSERT_TRUE("block 001 present", block1 != NULL);
    ASSERT_INT_EQ("block 001 word count", 2, (int)block1->word_count);
    ASSERT_EQ("block 001 word 0", 000001, block1->words[0]);
    ASSERT_EQ("block 001 word 1", 07700, block1->words[1]);

    const pdp8_paper_tape_block *block2 = pdp8_paper_tape_find(image, 0002);
    ASSERT_TRUE("block 002 present", block2 != NULL);
    ASSERT_INT_EQ("block 002 word count", 2, (int)block2->word_count);
    ASSERT_EQ("block 002 word 0", 05252, block2->words[0]);
    ASSERT_EQ("block 002 word 1", 02525, block2->words[1]);

    pdp8_paper_tape_destroy(image);
    remove(path);

    fp = fopen(path, "w");
    if (!fp) {
        return 0;
    }

    fputs("AA003:", fp);
    for (size_t i = 0; i < PDP8_PAPER_TAPE_MAX_WORDS + 1; ++i) {
        fputs(" 000000000000", fp);
    }
    fputc('\n', fp);
    fclose(fp);

    pdp8_paper_tape *too_large = NULL;
    ASSERT_INT_EQ("block size limit enforced", -1,
                  pdp8_paper_tape_load(path, &too_large));
    ASSERT_TRUE("no tape returned on failure", too_large == NULL);
    remove(path);

    return 1;
}

static int test_paper_tape_device(void) {
    pdp8_t *cpu = pdp8_api_create(4096);
    if (!cpu) {
        return 0;
    }

    pdp8_paper_tape_device_t *device = pdp8_paper_tape_device_create();
    if (!device) {
        pdp8_api_destroy(cpu);
        return 0;
    }

    ASSERT_INT_EQ("load tape image", 0, pdp8_paper_tape_device_load(device, "tapes/tp_demo.tape"));
    ASSERT_STR_EQ("tape device label", "TP", pdp8_paper_tape_device_label(device));
    ASSERT_INT_EQ("attach tape device", 0, pdp8_paper_tape_device_attach(cpu, device));

    pdp8_api_write_mem(cpu, 0000, PDP8_PAPER_TAPE_INSTR(PDP8_PAPER_TAPE_BIT_SELECT));
    pdp8_api_write_mem(cpu, 0001, PDP8_PAPER_TAPE_INSTR(PDP8_PAPER_TAPE_BIT_READ));
    pdp8_api_write_mem(cpu, 0002, 05001); /* JMP 0001 */

    pdp8_api_set_pc(cpu, 0000);
    pdp8_api_set_ac(cpu, 0001);
    ASSERT_INT_EQ("select block 1", 1, pdp8_api_step(cpu));

    for (size_t i = 0; i < PDP8_PAPER_TAPE_MAX_WORDS; ++i) {
        ASSERT_INT_EQ("read block 1 word", 1, pdp8_api_step(cpu));
        ASSERT_EQ("block 1 data", 05252, pdp8_api_get_ac(cpu));
        ASSERT_INT_EQ("loop block 1", 1, pdp8_api_step(cpu));
    }

    pdp8_api_set_ac(cpu, 0);
    ASSERT_INT_EQ("read past block 1", 1, pdp8_api_step(cpu));
    ASSERT_EQ("block 1 exhausted", 0, pdp8_api_get_ac(cpu));
    ASSERT_INT_EQ("loop after block 1 exhaustion", 1, pdp8_api_step(cpu));

    pdp8_api_set_pc(cpu, 0000);
    pdp8_api_set_ac(cpu, 0002);
    ASSERT_INT_EQ("select block 2", 1, pdp8_api_step(cpu));

    for (size_t i = 0; i < PDP8_PAPER_TAPE_MAX_WORDS; ++i) {
        ASSERT_INT_EQ("read block 2 word", 1, pdp8_api_step(cpu));
        ASSERT_EQ("block 2 data", 02445, pdp8_api_get_ac(cpu));
        ASSERT_INT_EQ("loop block 2", 1, pdp8_api_step(cpu));
    }

    pdp8_paper_tape_device_destroy(device);
    pdp8_api_destroy(cpu);
    return 1;
}

static int test_board_spec(void) {
    const pdp8_board_spec *spec = pdp8_board_adafruit_fruit_jam();
    ASSERT_TRUE("Fruit Jam spec available", spec != NULL);
    ASSERT_STR_EQ("Fruit Jam name", "Adafruit Fruit Jam", spec->name);
    ASSERT_STR_EQ("Fruit Jam MCU", "RP2350", spec->mcu);
    ASSERT_INT_EQ("Fruit Jam memory words", 4096, spec->memory_words);
    ASSERT_INT_EQ("Fruit Jam tick", 60, spec->tick_hz);

    pdp8_t *cpu = pdp8_api_create_for_board(spec);
    ASSERT_TRUE("CPU created for Fruit Jam", cpu != NULL);
    ASSERT_TRUE("Board attached", pdp8_api_get_board(cpu) == spec);
    ASSERT_EQ("Write last word", 0, pdp8_api_write_mem(cpu, (uint16_t)(spec->memory_words - 1u), 01234));
    ASSERT_EQ("Read last word", 01234, pdp8_api_read_mem(cpu, (uint16_t)(spec->memory_words - 1u)));

    pdp8_api_destroy(cpu);
    return 1;
}

int main(int argc, char **argv) {
    int verbose = 0;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-v") == 0) {
            verbose = 1;
        } else {
            fprintf(stderr, "Usage: %s [-v]\n", argv[0]);
            return EXIT_FAILURE;
        }
    }

    struct {
        const char *name;
        int (*fn)(void);
    } tests[] = {
        {"memory reference", test_memory_reference},
        {"indirect", test_indirect_and_auto_increment},
        {"operate group 1", test_operate_group1},
        {"operate group 2", test_operate_group2},
        {"iot", test_iot},
        {"clear halt", test_clear_halt},
        {"kl8e console", test_kl8e_console},
        {"line printer", test_line_printer},
        {"paper tape parser", test_paper_tape_parser},
        {"paper tape device", test_paper_tape_device},
        {"fruit jam board", test_board_spec},
    };

    size_t total = sizeof(tests) / sizeof(tests[0]);
    size_t passed = 0;
    for (size_t i = 0; i < total; ++i) {
        int result = tests[i].fn();
        if (result) {
            ++passed;
            if (verbose) {
                printf("%s: PASS\n", tests[i].name);
            }
            continue;
        }

        if (verbose) {
            printf("%s: FAIL\n", tests[i].name);
        }
        fprintf(stderr, "Test failed: %s\n", tests[i].name);
        break;
    }

    if (passed == total) {
        printf("All %zu tests passed.\n", total);
        return EXIT_SUCCESS;
    }
    return EXIT_FAILURE;
}
