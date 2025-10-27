#include <stdio.h>
#include <stdlib.h>
#include "../src/emulator/paper_tape.h"

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <tapefile>\n", argv[0]);
        return 1;
    }
    pdp8_paper_tape *img = NULL;
    if (pdp8_paper_tape_load(argv[1], &img) != 0) {
        fprintf(stderr, "Failed to load %s\n", argv[1]);
        return 2;
    }
    printf("Loaded label=%s blocks=%zu\n", img->label, img->block_count);
    for (size_t i = 0; i < img->block_count; ++i) {
        printf("block %03o: words=%zu\n", (unsigned)img->blocks[i].block, img->blocks[i].word_count);
        if (img->blocks[i].block == 2) {
            printf("Words for block 002:\n");
            for (size_t j = 0; j < img->blocks[i].word_count && j < 256; ++j) {
                printf(" %04o", img->blocks[i].words[j]);
                if ((j+1) % 16 == 0) printf("\n");
            }
            printf("\n");
        }
    }
    pdp8_paper_tape_destroy(img);
    return 0;
}
