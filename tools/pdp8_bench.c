#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "../src/emulator/pdp8.h"

static double now_seconds(void) {
    struct timespec ts;
    if (timespec_get(&ts, TIME_UTC) == 0) {
        return 0.0;
    }
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

typedef int (*program_loader)(pdp8_t *cpu);

struct bench_stats {
    double elapsed_sec;
    size_t loops;
    size_t instructions;
};

static int run_benchmark(const char *label,
                         program_loader loader,
                         size_t instructions_per_loop,
                         size_t loop_iterations,
                         struct bench_stats *out_stats) {
    pdp8_t *cpu = pdp8_api_create(4096);
    if (!cpu) {
        fprintf(stderr, "[%s] Unable to allocate PDP-8 instance\n", label);
        return -1;
    }

    if (loader(cpu) != 0) {
        fprintf(stderr, "[%s] Failed to load benchmark program\n", label);
        pdp8_api_destroy(cpu);
        return -1;
    }

    size_t target_instructions = loop_iterations * instructions_per_loop;
    const size_t chunk_size = 1000000u * instructions_per_loop;
    size_t executed_instructions = 0;

    double start = now_seconds();
    while (executed_instructions < target_instructions) {
        size_t remaining = target_instructions - executed_instructions;
        size_t request = remaining < chunk_size ? remaining : chunk_size;
        int ran = pdp8_api_run(cpu, request);
        if (ran <= 0) {
            fprintf(stderr, "[%s] Emulator stopped after %zu instructions\n", label,
                    executed_instructions);
            pdp8_api_destroy(cpu);
            return -1;
        }
        executed_instructions += (size_t)ran;
    }
    double end = now_seconds();

    pdp8_api_destroy(cpu);

    if (out_stats) {
        out_stats->elapsed_sec = end - start;
        out_stats->loops = loop_iterations;
        out_stats->instructions = executed_instructions;
    }
    return 0;
}

static void print_stats(const char *label, const struct bench_stats *stats) {
    double loops_per_sec = stats->loops / stats->elapsed_sec;
    double instr_per_sec = stats->instructions / stats->elapsed_sec;

    printf("%s\n", label);
    printf("  Loop iterations: %zu (%.2f million)\n", stats->loops, stats->loops / 1e6);
    printf("  Instructions executed: %zu (%.2f million)\n", stats->instructions,
           stats->instructions / 1e6);
    printf("  Elapsed time: %.3f s\n", stats->elapsed_sec);
    printf("  Throughput: %.2f Mloops/s, %.2f MIPS\n\n",
           loops_per_sec / 1e6, instr_per_sec / 1e6);
}

static int load_plain_loop(pdp8_t *cpu) {
    /* Two-instruction loop: NOP; JMP 0000 */
    if (pdp8_api_write_mem(cpu, 0000, 07000) != 0) {
        return -1;
    }
    if (pdp8_api_write_mem(cpu, 0001, 05000) != 0) {
        return -1;
    }
    pdp8_api_set_pc(cpu, 0000);
    return 0;
}

static int load_auto_increment_loop(pdp8_t *cpu) {
    /* Loop using TAD I 0010 to force auto-increment, then jump back */
    if (pdp8_api_write_mem(cpu, 0000, 01010 | 00400) != 0) { /* TAD I 0010 */
        return -1;
    }
    if (pdp8_api_write_mem(cpu, 0001, 05000) != 0) { /* JMP 0000 */
        return -1;
    }
    if (pdp8_api_write_mem(cpu, 0010, 0000) != 0) { /* pointer starts at zero */
        return -1;
    }
    pdp8_api_set_pc(cpu, 0000);
    pdp8_api_set_ac(cpu, 0);
    pdp8_api_set_link(cpu, 0);
    return 0;
}

static int load_jms_operate_loop(pdp8_t *cpu) {
    /*
     * Six-instruction loop within the 0o0100 budget:
     *   0000: JMS 0010
     *   0001: JMP 0000
     *   0010: (return slot)
     *   0011: IAC
     *   0012: BSW
     *   0013: RAR
     *   0014: JMP I 0010
     */
    if (pdp8_api_write_mem(cpu, 0000, 04010) != 0) { /* JMS 0010 */
        return -1;
    }
    if (pdp8_api_write_mem(cpu, 0001, 05000) != 0) { /* JMP 0000 */
        return -1;
    }
    if (pdp8_api_write_mem(cpu, 0011, 07001) != 0) { /* IAC */
        return -1;
    }
    if (pdp8_api_write_mem(cpu, 0012, 07002) != 0) { /* BSW (byte swap) */
        return -1;
    }
    if (pdp8_api_write_mem(cpu, 0013, 07010) != 0) { /* RAR */
        return -1;
    }
    if (pdp8_api_write_mem(cpu, 0014, 05410) != 0) { /* JMP I 0010 */
        return -1;
    }

    pdp8_api_set_pc(cpu, 0000);
    pdp8_api_set_ac(cpu, 0);
    pdp8_api_set_link(cpu, 0);
    return 0;
}

int main(int argc, char **argv) {
    size_t loop_iterations = 50000000u; /* default: 50 million loops */
    if (argc > 2) {
        fprintf(stderr, "Usage: %s [loop_count]\n", argv[0]);
        return EXIT_FAILURE;
    }
    if (argc == 2) {
        char *endptr = NULL;
        unsigned long long parsed = strtoull(argv[1], &endptr, 10);
        if (!argv[1][0] || (endptr && *endptr != '\0')) {
            fprintf(stderr, "Invalid loop count: %s\n", argv[1]);
            return EXIT_FAILURE;
        }
        loop_iterations = (size_t)parsed;
    }

    struct bench_stats basic_stats;
    if (run_benchmark("NOP/JMP loop", load_plain_loop, 2u, loop_iterations, &basic_stats) != 0) {
        return EXIT_FAILURE;
    }

    struct bench_stats auto_stats;
    if (run_benchmark("Auto-increment loop", load_auto_increment_loop, 2u, loop_iterations,
                      &auto_stats) != 0) {
        return EXIT_FAILURE;
    }

    struct bench_stats jms_stats;
    if (run_benchmark("JMS/operate loop", load_jms_operate_loop, 6u, loop_iterations,
                      &jms_stats) != 0) {
        return EXIT_FAILURE;
    }

    printf("PDP-8 microbenchmarks (loop iterations per scenario = %zu)\n\n", loop_iterations);
    print_stats("NOP/JMP loop", &basic_stats);
    print_stats("Auto-increment loop", &auto_stats);
    print_stats("JMS/operate loop", &jms_stats);

    return EXIT_SUCCESS;
}
