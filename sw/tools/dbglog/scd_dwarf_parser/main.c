#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>
#include <scd_reader.h>

#define SCD_DWARF_STANDALONE

//#include "scd_dwarf_wenv.h"

#include "scd_dwarf_parser.h"

scd_info_t *scd_info;

static void print(scd_entry_t *ids, unsigned ids_size, dies_memb_t *dies, unsigned dies_size);

char *dies_memb_type_str[DIES_MEMB_TYPE_LAST] = {
    
    [DIES_MEMB_TYPE_STRUCT] = "S",
    [DIES_MEMB_TYPE_ARRAY ] = "A",
    [DIES_MEMB_TYPE_FIELD]  = " "
};

char *get_dies_memb_type_str(dies_memb_type_t  memb_type){
    return dies_memb_type_str[memb_type];
}

char *get_dies_flag_str(gint flag, char *str){

    memset(str, '-', 4);
    str[4] = 0;

    if (DIES_MEMB_FLAG_IS_LAST      & flag) {
        str[0] = 'L';
    }
    if (DIES_MEMB_FLAG_IS_PARENT    & flag) {
        str[1] = 'P';
    }
    if (DIES_MEMB_FLAG_IS_ROOT      & flag) {
        str[2] = 'R';
    }
    if (DIES_MEMB_FLAG_SCD_ROOT     & flag) {
        str[3] = 'S';
    }

    return str;    
}

static void dies_print( dies_memb_t *dies_memb, int cur_level)
{
#define MAX_INDENT    (10 * 4)
    static char flag_str[5];
    static char indent_str[MAX_INDENT+1] = {0};
    static char data_name[256] = {0};

    if (dies_memb == NULL && cur_level == 0) {
        // Print header
        printf("\n");
        printf("\n");
        printf("\n");
        printf("Dies members:\n");
        printf("Type: (S)truct, (A)array;     Flags: (L)ast, (P)arent, (R)oot, (S)SCD entry\n");
        printf("=================================================================================\n");
        printf(" %-3s | %-3s | %-1s | %-40s | %-4s | %-4s | %-s \n",
               "ID", "Lnk", "T", "Data type", "Offs", "Size", "Flag");
        printf("=================================================================================\n");
        return;
    }
    else if (dies_memb == NULL && cur_level == -1) {
        // Print footer
        printf("=================================================================================\n");
        return;
    }


    if (!indent_str[0]){
        memset(indent_str, ' ', MAX_INDENT);
        indent_str[MAX_INDENT] = 0;
    }

    indent_str[cur_level * 4] = '\0';
    sprintf(data_name, "%s %s %s", 
        indent_str,
        dies_memb->type_name,
        dies_memb->field_name);

//          ID    | LinkID
//                |      | DieType   
//                |      |     |  Level Indent, Data Type & Name          
//                |      |     |       | Offset  
//                |      |     |       |     | Size     
//                |      |     |       |     |     | Flag
    printf(" %-3d | %-3d | %1s | %-40s | %4d | %4d | %4s \n",
        dies_memb->self_id,
        dies_memb->link_id,
        get_dies_memb_type_str(dies_memb->memb_type),
        data_name,
        dies_memb->offset,
        dies_memb->size,
        get_dies_flag_str(dies_memb->flag, flag_str)
    );

    indent_str[cur_level * 4] = ' ';
}

static dies_memb_t* dies_print_lvl(dies_memb_t *dies_memb, int cur_level, int max_idx)
{
    /* sanity check here */

    do {
        gint flag;

        if (dies_memb->self_id == max_idx - 1){
            printf("ERROR\n");
            return dies_memb;
        }

        flag = dies_memb->flag;

        dies_print(dies_memb, cur_level);

        dies_memb ++;

        // Print child
        if (flag & DIES_MEMB_FLAG_IS_PARENT) {
            dies_memb = dies_print_lvl(dies_memb, cur_level+1, max_idx);
        } 

        if (flag & DIES_MEMB_FLAG_IS_LAST) {
            return dies_memb;
        } 

    } while (1);

    printf("ERROR\n");

    return NULL;
}

void dies_pool_print(GSList *dies_pools_list)
{
    GSList *list_entry;
    list_entry = g_slist_nth(dies_pools_list, 0);
    while (list_entry){
        dies_pool_t *dies_pool;

        dies_pool = g_slist_nth_data(list_entry, 0);
        if (dies_pool->size > 0) {

            dies_print(NULL, 0);
            dies_print_lvl(&dies_pool->dies[0], 0, dies_pool->next_idx);

            dies_print(NULL, -1);
        }
        list_entry = g_slist_next(list_entry);
    }
}

int main(int argc, char* argv[])
{
    int res;
    dies_memb_t *dies;
    unsigned dies_size;
    GSList *dies_pools_list;

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

    res = scd_dwarf_parse(
        scd_fn, 
        &scd_info, 
        &dies_pools_list);


    if (res != RESULT_OK) {
        return EXIT_FAILURE;
    }

    dies = NULL;
    dies_size = 0;

    print(scd_info->scd_table, scd_info->scd_table_size,
          dies, dies_size);

    /* Printout DIEs pool */
    dies_pool_print(dies_pools_list);

    /* res = dwarf2_finish(); */
    //if (res != RESULT_OK) {
    //  return EXIT_FAILURE;
    //};

    return EXIT_SUCCESS;
}

void print(scd_entry_t *scd_entry, unsigned int ids_size, dies_memb_t *dies, unsigned dies_size)
{
  unsigned int i;
  printf("IDS:\n");
  for (i = 0; i < ids_size; ++i) {
    printf("{ %2d, %2d, %4d, %-60s %p }\n", 
        scd_entry[i].eid,
        scd_entry[i].fid,
        scd_entry[i].lid,
        scd_entry[i].str,
        scd_entry[i].data);
  };

  /*
  printf("DIEs:\n");
  for (i = 0; i < dies_size; ++i) {
    printf("{ %2d, %48s, %48s, %4u, %4u, %2u, %2u, %8lx, %2d, %2d, %2d, %2d }\n", dies[i].memb_type, dies[i].type_name, dies[i].field_name, dies[i].offset, dies[i].size, dies[i].format, dies[i].display, (unsigned long)dies[i].strings, dies[i].self_id, dies[i].flag, dies[i].hf_id, dies[i].ett_id);
  };

  */
}

