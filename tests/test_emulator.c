#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "../src/emulator/pdp8.h"
#include "../src/emulator/pdp8_board.h"
#include "../src/emulator/kl8e_console.h"

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

int main(void) {
    struct {
        const char *name;
        int (*fn)(void);
    } tests[] = {
        {"memory reference", test_memory_reference},
        {"indirect", test_indirect_and_auto_increment},
        {"operate group 1", test_operate_group1},
        {"operate group 2", test_operate_group2},
        {"iot", test_iot},
        {"kl8e console", test_kl8e_console},
        {"fruit jam board", test_board_spec},
    };

    size_t total = sizeof(tests) / sizeof(tests[0]);
    size_t passed = 0;
    for (size_t i = 0; i < total; ++i) {
        if (tests[i].fn()) {
            ++passed;
        } else {
            fprintf(stderr, "Test failed: %s\n", tests[i].name);
            break;
        }
    }

    if (passed == total) {
        printf("All %zu tests passed.\n", total);
        return EXIT_SUCCESS;
    }
    return EXIT_FAILURE;
}
