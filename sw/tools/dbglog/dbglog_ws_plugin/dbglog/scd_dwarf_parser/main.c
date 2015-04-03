#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>
#include <scd_reader.h>

#define SCD_DWARF_STANDALONE

//#include "scd_dwarf_wenv.h"

#include "scd_dwarf_parser.h"

scd_info_t *scd_info;

int main(int argc, char* argv[])
{
    int res;
    dies_memb_t *dies;
    unsigned dies_size;
    GHashTable *dies_pools_hash;

    char* scd_fn = NULL;
    if (argc < 2)
    {
        printf("Please specify scd file name.\n");
        exit(EXIT_FAILURE);
    }

    scd_fn = argv[1];
    printf("********** SCD DWRAF parser ***********\n");
    printf("SCD file name: %s\n\n", scd_fn);

    res = dwarf2_init();
    if (res != RESULT_OK) {
        return EXIT_FAILURE;
    };

    dies_pools_hash = NULL;
    res = scd_dwarf_parse(
        scd_fn, 
        &scd_info, 
        &dies_pools_hash);


    if (res != RESULT_OK) {
        return EXIT_FAILURE;
    }

    print_ids(stdout, scd_info->scd_table, scd_info->scd_table_size);

    /* Printout DIEs pool */
    dies_pool_print(stdout, dies_pools_hash);

    scd_dwarf_free(scd_info);

    return EXIT_SUCCESS;
}

