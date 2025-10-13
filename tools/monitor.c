#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/emulator/pdp8.h"
#include "../src/emulator/pdp8_board.h"

static int parse_number(const char *token, long *value) {
    if (!token || !*token) {
        return -1;
    }

    int base = 8;
    const char *ptr = token;

    if (token[0] == '#') {
        base = 10;
        ptr = token + 1;
    } else if (token[0] == '0' && (token[1] == 'x' || token[1] == 'X')) {
        base = 16;
        ptr = token + 2;
    }

    char *end = NULL;
    long parsed = strtol(ptr, &end, base);
    if (!end || *end != '\0') {
        return -1;
    }

    *value = parsed;
    return 0;
}

static void print_help(void) {
    puts("Commands:");
    puts("  mem <addr> [count]         Dump memory words (octal).");
    puts("  load <addr> <w0> [w1 ...]  Write consecutive words starting at addr.");
    puts("  run [cycles]               Execute up to cycles (default 1000).");
    puts("  regs                       Show PC, AC, link, switch register.");
    puts("  save <file>                Write RAM image to file.");
    puts("  restore <file>             Load RAM image from file.");
    puts("  reset                      Clear state and reload board ROM.");
    puts("  help                       Show this help.");
    puts("  quit                       Exit monitor.");
    puts("Notes: numbers default to octal; prefix with '#' for decimal or 0x for hex.");
}

int main(void) {
    const pdp8_board_spec *board = pdp8_board_host_simulator();
    if (!board) {
        fprintf(stderr, "Unable to fetch host simulator board spec.\n");
        return EXIT_FAILURE;
    }

    pdp8_t *cpu = pdp8_api_create_for_board(board);
    if (!cpu) {
        fprintf(stderr, "Unable to create PDP-8 instance.\n");
        return EXIT_FAILURE;
    }

    const size_t memory_words = board->memory_words ? board->memory_words : 4096u;
    char line[512];

    puts("PDP-8 Monitor. Type 'help' for commands.");
    while (true) {
        fputs("pdp8> ", stdout);
        fflush(stdout);

        if (!fgets(line, sizeof line, stdin)) {
            puts("");
            break;
        }

        line[strcspn(line, "\r\n")] = '\0';

        char *cmd = strtok(line, " \t");
        if (!cmd) {
            continue;
        }

        if (strcmp(cmd, "quit") == 0 || strcmp(cmd, "exit") == 0) {
            break;
        } else if (strcmp(cmd, "help") == 0) {
            print_help();
        } else if (strcmp(cmd, "regs") == 0) {
            uint16_t pc = pdp8_api_get_pc(cpu);
            uint16_t ac = pdp8_api_get_ac(cpu);
            uint8_t link = pdp8_api_get_link(cpu);
            uint16_t sw = pdp8_api_get_switch_register(cpu);
            bool halted = pdp8_api_is_halted(cpu) != 0;
            printf("PC=%04o AC=%04o LINK=%o SW=%04o HALT=%s\n",
                   pc & 0x0FFFu, ac & 0x0FFFu, link & 0x1u, sw & 0x0FFFu, halted ? "yes" : "no");
        } else if (strcmp(cmd, "mem") == 0) {
            char *addr_tok = strtok(NULL, " \t");
            if (!addr_tok) {
                fprintf(stderr, "mem requires address.\n");
                continue;
            }
            long addr_val = 0;
            if (parse_number(addr_tok, &addr_val) != 0 || addr_val < 0) {
                fprintf(stderr, "Invalid address '%s'.\n", addr_tok);
                continue;
            }
            char *count_tok = strtok(NULL, " \t");
            long count_val = 8;
            if (count_tok) {
                if (parse_number(count_tok, &count_val) != 0 || count_val <= 0) {
                    fprintf(stderr, "Invalid count '%s'.\n", count_tok);
                    continue;
                }
            }

            size_t address = (size_t)addr_val;
            for (long i = 0; i < count_val; ++i) {
                size_t current = (address + (size_t)i) % memory_words;
                uint16_t word = pdp8_api_read_mem(cpu, (uint16_t)current);
                printf("%04zo: %04o\n", current, word & 0x0FFFu);
            }
        } else if (strcmp(cmd, "load") == 0) {
            char *addr_tok = strtok(NULL, " \t");
            if (!addr_tok) {
                fprintf(stderr, "load requires address.\n");
                continue;
            }
            long addr_val = 0;
            if (parse_number(addr_tok, &addr_val) != 0 || addr_val < 0) {
                fprintf(stderr, "Invalid address '%s'.\n", addr_tok);
                continue;
            }

            size_t loaded = 0;
            char *word_tok = NULL;
            while ((word_tok = strtok(NULL, " \t")) != NULL) {
                long word_val = 0;
                if (parse_number(word_tok, &word_val) != 0 || word_val < 0 || word_val > 0x0FFF) {
                    fprintf(stderr, "Invalid word '%s'.\n", word_tok);
                    loaded = 0;
                    break;
                }
                size_t current = ((size_t)addr_val + loaded) % memory_words;
                if (pdp8_api_write_mem(cpu, (uint16_t)current, (uint16_t)word_val) != 0) {
                    fprintf(stderr, "Failed to write memory at %04zo.\n", current);
                    loaded = 0;
                    break;
                }
                ++loaded;
            }

            if (loaded > 0) {
                printf("Loaded %zu word(s) starting at %04zo.\n", loaded, (size_t)addr_val % memory_words);
            }
        } else if (strcmp(cmd, "run") == 0) {
            char *cycles_tok = strtok(NULL, " \t");
            size_t cycles = 1000;
            if (cycles_tok) {
                long cycles_val = 0;
                if (parse_number(cycles_tok, &cycles_val) != 0 || cycles_val <= 0) {
                    fprintf(stderr, "Invalid cycle count '%s'.\n", cycles_tok);
                    continue;
                }
                cycles = (size_t)cycles_val;
            }
            int executed = pdp8_api_run(cpu, cycles);
            if (executed < 0) {
                fprintf(stderr, "Run failed.\n");
                continue;
            }
            printf("Executed %d cycle(s). PC=%04o HALT=%s\n",
                   executed, pdp8_api_get_pc(cpu) & 0x0FFFu,
                   pdp8_api_is_halted(cpu) ? "yes" : "no");
        } else if (strcmp(cmd, "save") == 0) {
            char *path = strtok(NULL, " \t");
            if (!path) {
                fprintf(stderr, "save requires file path.\n");
                continue;
            }
            FILE *fp = fopen(path, "wb");
            if (!fp) {
                fprintf(stderr, "Unable to open '%s' for writing: %s\n", path, strerror(errno));
                continue;
            }
            size_t written = 0;
            for (size_t addr = 0; addr < memory_words; ++addr) {
                uint16_t word = pdp8_api_read_mem(cpu, (uint16_t)addr);
                if (fwrite(&word, sizeof word, 1, fp) != 1) {
                    fprintf(stderr, "Write failed at address %04zo: %s\n", addr, strerror(errno));
                    break;
                }
                ++written;
            }
            fclose(fp);
            printf("Saved %zu word(s) to %s.\n", written, path);
        } else if (strcmp(cmd, "restore") == 0) {
            char *path = strtok(NULL, " \t");
            if (!path) {
                fprintf(stderr, "restore requires file path.\n");
                continue;
            }
            FILE *fp = fopen(path, "rb");
            if (!fp) {
                fprintf(stderr, "Unable to open '%s' for reading: %s\n", path, strerror(errno));
                continue;
            }
            size_t restored = 0;
            uint16_t word = 0;
            while (restored < memory_words && fread(&word, sizeof word, 1, fp) == 1) {
                pdp8_api_write_mem(cpu, (uint16_t)restored, word);
                ++restored;
            }
            if (!feof(fp)) {
                fprintf(stderr, "Read error while restoring: %s\n", strerror(errno));
            }
            fclose(fp);
            printf("Restored %zu word(s) from %s.\n", restored, path);
        } else if (strcmp(cmd, "reset") == 0) {
            pdp8_api_reset(cpu);
            puts("CPU reset.");
        } else {
            fprintf(stderr, "Unknown command '%s'. Type 'help' for a list.\n", cmd);
        }
    }

    pdp8_api_destroy(cpu);
    return EXIT_SUCCESS;
}
