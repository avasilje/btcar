// TODO:
// - fix array subrange not processed
// - fix array entries always last???
// - DBG unions
// - DBG enums
// - rework dynamic strings to static
// - cleanup string free
// - Reformat debug printout
// - legalize program traps

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>     

#include <dwarf.h>
#include <libdwarf.h>

#include <glib.h>
#include "scd_reader.h"

#include "scd_dwarf_parser.h"

//#define DEBUG

#ifdef DEBUG
#define DBG_PRINT(a)         DBG_PRINT0(a)
#define DBG_PRINT0(a)        fprintf(stderr,a);
#define DBG_PRINT1(a,b)      fprintf(stderr,a,b)
#define DBG_PRINT2(a,b,c)    fprintf(stderr,a,b,c)
#define DBG_PRINT3(a,b,c,d)  fprintf(stderr,a,b,c,d)
#define DBG_PRINT4(a,b,c,d,e) fprintf(stderr,a,b,c,d,e)
#define DBG_PRINT5(a,b,c,d,e,f) fprintf(stderr,a,b,c,d,e,f)
#else
#define DBG_PRINT(a)         
#define DBG_PRINT0(a)        
#define DBG_PRINT1(a,b)      
#define DBG_PRINT2(a,b,c)    
#define DBG_PRINT3(a,b,c,d)  
#define DBG_PRINT4(a,b,c,d,e)
#define DBG_PRINT5(a,b,c,d,e,f)
#endif

#define FORMAT_TYPE(s,f) { char *p; p = strdup(s); sprintf(s,f,p); free(p); }
#define FORMAT_TYPE2(s,t,f) { char *p; p = strdup(s); sprintf(s,f,t,p); free(p); }
#define FORMAT_TYPE2_REV(s,t,f) { char *p; p = strdup(s); sprintf(s,f,p,t); free(p); }

#define DIE_SIZE_INIT (1024*4)
#define DIE_SIZE_STEP (1024*4)

int is_info = 1;

static int process_elf(dies_pool_t *dies_pool);

static void process_dwarf(dies_pool_t *dies_pool);
static void process_siblings (dies_pool_t *dies_pool, Dwarf_Die die, dies_memb_t **last_root);
static void maybe_process_die(dies_pool_t *dies_pool, Dwarf_Die die, dies_memb_t **last_root);

static dies_memb_t *process_record(dies_pool_t *dies_pool, dies_memb_t *dies_memb, Dwarf_Die die,  Dwarf_Half parent_tag);
static dies_memb_t *process_children(dies_pool_t *dies_pool, Dwarf_Die die);
static int die_nof_children(Dwarf_Die die);
static Dwarf_Die typedef_to_type(Dwarf_Die die);
static void record_format(dies_memb_t *rec, Dwarf_Die die);
static unsigned process_subrange(Dwarf_Die die);

static void dies_check_memory();

static char* die_get_name(Dwarf_Die die);
static char* die_get_type_name(Dwarf_Die die);
static void die_get_type_name_(Dwarf_Die die, char *res);
static Dwarf_Half die_get_tag(Dwarf_Die die);
static unsigned die_attr_size(Dwarf_Die die);
static Dwarf_Die die_get_type(Dwarf_Die die);
static unsigned die_get_size(Dwarf_Die die);
static unsigned die_attr_offset(Dwarf_Die die);
static int die_attr_const_value(Dwarf_Die die);

static Dwarf_Debug   s_dbg;
static int           s_enum_index = -1;

guint
hash_root_die_key(gconstpointer dtype_v)
{
    // die_memb key is it's typename
    guint key_hash = g_str_hash((gconstpointer)dtype_v);
    return key_hash;
}

gboolean
hash_root_die_is_equal(gconstpointer die1_v, gconstpointer die2_v)
{
    // die_memb key is it's typename
    gboolean rv = (strcmp(die1_v, die2_v) == 0);
    return rv;
}

static dies_memb_t *add_record(dies_pool_t *dies_pool)
{
    dies_memb_t *p = NULL;

    p = &dies_pool->dies[dies_pool->next_idx];

    p->memb_type = 0;
    p->type_name = NULL;
    p->field_name = NULL;
    p->size = 0;
    p->offset = 0;
    p->format = 0;
    p->display = 0;
    p->strings = NULL;
    p->self_id = dies_pool->next_idx;
    p->link_id = -1;
    p->flag = 0;
//    p->hf_id = -1;
    p->ett_id = -1;

    DBG_PRINT1("NEW RECORD[%d]()\n", dies_pool->next_idx);

    dies_check_memory(dies_pool);

    dies_pool->next_idx += 1;

    return &dies_pool->dies[dies_pool->next_idx - 1];
}

static dies_memb_t *add_record_root (dies_pool_t *dies_pool, dies_memb_t *dies_memb_scd, Dwarf_Die die)
{
    dies_memb_t *root_memb;

    DBG_PRINT1("PROCESS RECORD(%s)\n", dies_memb_scd->type_name ? dies_memb_scd->type_name : "");

    root_memb = add_record(dies_pool);
    if (!root_memb){
        return NULL;
    }

    root_memb->memb_type  = DIES_MEMB_TYPE_STRUCT;
    root_memb->field_name = strdup("");     // Root entry do not have a name, only type
    root_memb->type_name  = strdup(dies_memb_scd->type_name);
    root_memb->offset     = 0;
    root_memb->size       = die_get_size(die);
    root_memb->display    = dies_memb_scd->display;
    root_memb->format     = dies_memb_scd->format;
    root_memb->flag       = DIES_MEMB_FLAG_IS_ROOT;

    // If type_name already exist then root_entry must not be created.
    // scd_dies must be linked to existing dies_memb
    if (g_hash_table_contains(dies_pool->dies_hash, (gpointer)root_memb->type_name)) {
        while (1);
    }

    g_hash_table_insert(dies_pool->dies_hash, (gpointer)root_memb->type_name, (gpointer)root_memb);

    DBG_PRINT2("ROOT MEMBER: type_name='%s' size='%d'\n", root_memb->type_name, root_memb->size);

    return root_memb;
}

int dwarf2_init()
{
    int res = RESULT_OK;
    return res;
}

int scd_dwarf_parse(char *scd_fn, scd_info_t **scd_info_out, GSList **dies_pools_list)
{
    int   res;

    GHashTableIter iter_elf;
    GHashTableIter iter_scd;
    scd_info_t   *scd_info;
    scd_entry_t  *scd_entry_key;
    scd_entry_t  *scd_entry;

    res = RESULT_OK;

    scd_info = scd_init(SCD_HASH_FLAGS_ELF | SCD_HASH_FLAGS_MSG);

    if (scd_info == NULL)
        return RESULT_FAIL;

    *scd_info_out = scd_info;

    res = process_scd(scd_fn, scd_info);

    scd_process_finish(scd_info);

    // Fill up DIES pools list by  processing all ELF files specified in SCD. 
    *dies_pools_list = NULL;
    g_hash_table_iter_init(&iter_elf, scd_info->elf_hash);
    while (g_hash_table_iter_next(&iter_elf, &(gpointer)scd_entry_key, &(gpointer)scd_entry)){
        dies_pool_t *dies_pool;

        // Add new entry to dies pool list
        dies_pool = malloc(sizeof(dies_pool_t));
        dies_pool->elf_fname = g_strdup(scd_entry->str);
        dies_pool->elf_id = scd_entry->eid;
        dies_pool->size = DIE_SIZE_INIT;
        dies_pool->dies = malloc(dies_pool->size * sizeof(dies_memb_t));
        dies_pool->next_idx = 0;
        dies_pool->dies_hash     = g_hash_table_new(hash_root_die_key, hash_root_die_is_equal);     // 
        dies_pool->dies_scd_hash = g_hash_table_new(hash_root_die_key, hash_root_die_is_equal);     // Entry point from SCD. Alway linked with root or parent.

        *dies_pools_list = g_slist_append(*dies_pools_list, (gpointer)dies_pool);

        // Copy all DIEs roots from scd_table to dies_pool and link them together.
        // Loop over all SCD MSG entries
        g_hash_table_iter_init(&iter_scd, scd_info->msg_hash);
        while ( g_hash_table_iter_next(&iter_scd,
                &(gpointer)scd_entry_key,
                &(gpointer)scd_entry)){

            char *scd_str;
            char *scd_str_p;

            size_t      dtype_len;

            // Add only SCD belonging to that elf
            if (scd_entry->eid != dies_pool->elf_id){
                continue;
            }

            DBG_PRINT1("-- SCD %s\n", scd_entry->str);

            // Loop over all data types within SCD string
            scd_str = g_strdup(scd_entry->str);
            scd_str_p = scd_str;

            while (scd_str_p = scd_get_next_dtyp(scd_str_p, &dtype_len)){
                dies_memb_t *die_scd;

                if (dtype_len == 0){
                    break;
                }

                scd_str_p[dtype_len] = 0;
                // Add new DIES_SCD entry in DIEs table if not exist yet
                die_scd = (dies_memb_t *)g_hash_table_lookup(dies_pool->dies_scd_hash,
                                                              (gpointer)scd_str_p);
                if (die_scd == NULL){

                    die_scd = add_record(dies_pool);
                    die_scd->type_name = g_strndup(scd_str_p, dtype_len);
                    die_scd->flag = DIES_MEMB_FLAG_SCD_ROOT;

                    // Add ROOT DIE to hash table for fast access during elf parsing
                    g_hash_table_insert(dies_pool->dies_scd_hash,
                                        (gpointer)die_scd->type_name,
                                        (gpointer)die_scd);

                    DBG_PRINT1("---- New type name added %s\n", die_scd->type_name);

                }
                else
                {
                    DBG_PRINT1("---- Duplicated type name %s\n", die_root->type_name);
                }
                // Add that dies root to SCD list (aka link dies & scd tables)
                scd_entry->dies_list = g_slist_append(scd_entry->dies_list, (gpointer)die_scd);

                scd_str_p += (dtype_len + 1);
            }
            g_free(scd_str);
        }

        // Mark Last SCD dies
        dies_pool->dies[dies_pool->next_idx - 1].flag |= DIES_MEMB_FLAG_IS_LAST;

        // Fillup DIEs pool with entries from particular ELF file specified in SCD
        // A DIE entry created for SCD entries where TAG = SCD_ENTRY_TAG_MSG

        res = process_elf(dies_pool);

        // TODO: Cleanup dies_pool->dies_scd_hash as it is not used any more 
    }

    return RESULT_OK;
}

int dwarf2_finish()
{
//    unsigned i;
    int res = RESULT_OK;

    /* TODO: call scd_finish() */
    // AV: dbl chk. iterrate by pool list
    /* 
    for (i = 0; i < s_dies_size; ++i) {
        dies_memb_t *p = &s_dies[i];
        if (p->type_name != NULL) free(p->type_name);
        if (p->field_name != NULL) free(p->field_name);
        if (p->strings != NULL) free(p->strings);
    }
    free(s_dies);
    */
    return res;
}

static int process_elf(dies_pool_t *dies_pool)
{
    int res = RESULT_OK;

    int fd = -1;
    FILE *fp;
    Dwarf_Debug dbg = 0;
    Dwarf_Error error;
    Dwarf_Handler err_handler;
    Dwarf_Ptr err_arg;

    DBG_PRINT1(">>> process_elf('%s')\n", dies_pool->elf_fname);

    fp = fopen(dies_pool->elf_fname, "rb");

    if (fp == NULL) {
        fprintf(stderr, "open failed\n");
        res = RESULT_FAIL;
        goto FINISH;
    }
    fd = fileno(fp);

    err_arg = NULL;
    err_handler = NULL;
    res = dwarf_init(fd, DW_DLC_READ, err_handler, err_arg, &dbg, &error);

    if (res != DW_DLV_OK) {
        fprintf(stderr, "dwarf_init failed\n");
        res = RESULT_FAIL;
        goto FINISH;
    }

    s_dbg = dbg;

    process_dwarf(dies_pool);

    // Add last empty record for sanity purposes
    add_record(dies_pool);

    res = dwarf_finish(dbg, &error);

    if (res != DW_DLV_OK) {
        fprintf(stderr, "dwarf_finish failed\n");
        return EXIT_FAILURE;
    }
FINISH:
    DBG_PRINT("<<< process_elf\n");
    return res;
}

void process_dwarf(dies_pool_t *dies_pool)
{
    Dwarf_Unsigned cu_header = 0;
    Dwarf_Error error;
    dies_memb_t *last_root = NULL;

    while (1) {
        Dwarf_Die no_die = 0;
        Dwarf_Die cu_die = 0;
        int res;
        res = dwarf_next_cu_header_c(s_dbg, is_info, 0, 0, 0, 0, 0, 0, 0, 0, &cu_header, &error);
        if (res == DW_DLV_ERROR) {
            fprintf(stderr, "dwarf_next_cu_header failed\n");
            exit(EXIT_FAILURE);
        }
        if (res == DW_DLV_NO_ENTRY) {
            if (last_root){
                last_root->flag |= DIES_MEMB_FLAG_IS_LAST;
            }

            return; /* all done */
        }
        res = dwarf_siblingof(s_dbg, no_die, &cu_die, &error);
        if (res == DW_DLV_ERROR) {
            fprintf(stderr, "dwarf_siblingof failed\n");
            exit(EXIT_FAILURE);
        }
        if (res == DW_DLV_NO_ENTRY) {
            fprintf(stderr, "dwarf_siblingof: no DIE entry in CU\n");
            exit(EXIT_FAILURE);
        }
        process_siblings(dies_pool, cu_die, &last_root);
        dwarf_dealloc(s_dbg, cu_die, DW_DLA_DIE);
    }

}

static void process_siblings(dies_pool_t *dies_pool, Dwarf_Die die, dies_memb_t **last_root)
{
    int res;
    Dwarf_Die cur_die = die;
    Dwarf_Die child = 0;
    Dwarf_Error error;
    
    maybe_process_die(dies_pool, die, last_root);

    while (1) {
        Dwarf_Die sib_die = 0;
        res = dwarf_child(cur_die, &child, &error);
        if (res == DW_DLV_ERROR) {
            fprintf(stderr, "dwarf_child failed\n");
            exit(EXIT_FAILURE);
        }
        if (res == DW_DLV_OK) {
            process_siblings(dies_pool, child, last_root);
        }
        res = dwarf_siblingof(s_dbg, cur_die, &sib_die, &error);
        if (res == DW_DLV_ERROR) {
            fprintf(stderr, "dwarf_sublingof failed\n");
            exit(EXIT_FAILURE);
        }
        if (res == DW_DLV_NO_ENTRY) {
            break; /* all done at current level */
        }
        if (cur_die != die) {
            dwarf_dealloc(s_dbg, cur_die, DW_DLA_DIE);
        }
        cur_die = sib_die;
        maybe_process_die(dies_pool, cur_die, last_root);
    }

}

static void link_if_possible(dies_pool_t *dies_pool, dies_memb_t *dies_memb, char *name)
{
    dies_memb_t* rec;

    if (name) {
        rec = (dies_memb_t*)g_hash_table_lookup(dies_pool->dies_hash, name);

        if (rec) {    // AV: todo move scd memb to dedicated hash
            if (rec->self_id != dies_memb->self_id) {
                dies_memb->link_id = rec->self_id;
            }
        }
    }
    return;
}

static void maybe_process_die(dies_pool_t *dies_pool, Dwarf_Die die, dies_memb_t **last_root)
{
    char *name = NULL;
    Dwarf_Half   tag;

    name = die_get_name(die);
    tag = die_get_tag(die);

    if (name && (DW_TAG_typedef == tag || DW_TAG_structure_type == tag)) {
        
        // Check is DIE has to be processed
        dies_memb_t *dies_memb_scd = NULL;
        dies_memb_scd = (dies_memb_t*)g_hash_table_lookup(dies_pool->dies_scd_hash, name);      // Check in SCD dies_memb hash

        // Data type need to be processed if it is exist 
        // in die pool and not processed yet
        if (dies_memb_scd != NULL){
        
            g_hash_table_remove(dies_pool->dies_scd_hash, name);

            link_if_possible(dies_pool, dies_memb_scd, name);

            if (dies_memb_scd->link_id == -1) {
                dies_memb_t *root_memb = NULL;

                // Create, Link and Process new root if not exist yet
                root_memb = add_record_root(dies_pool, dies_memb_scd, die);

                process_record(dies_pool, root_memb, die, 0);

                dies_memb_scd->link_id  = root_memb->self_id;
                dies_memb_scd->size     = root_memb->size;

                *last_root = root_memb;
            }
        }
    }
    else {
        DBG_PRINT1("SKIP RECORD(%s)\n", name ? name : "");
    }

    if (name) dwarf_dealloc(s_dbg, name, DW_DLA_STRING);
}

static dies_memb_t *process_record(dies_pool_t *dies_pool, dies_memb_t *dies_memb, Dwarf_Die die, Dwarf_Half parent_tag)
{
    dies_memb_t *rec = NULL;
    unsigned   index = 0;
    Dwarf_Half   tag;
    char *name;

    tag = die_get_tag(die);
    name = die_get_name(die);
    Dwarf_Die type_die = die_get_type(die);

    DBG_PRINT3("process_record(%x,%x) %s\n", tag, parent_tag, name);

    // Try to link with existing die_memb
    if (dies_memb && (tag != DW_TAG_member) && (tag != DW_TAG_base_type)) {
        link_if_possible(dies_pool, dies_memb, name);
        if (dies_memb->link_id != -1){
            return dies_memb;   // Do nothing if linked successufuly 
        }
    }

    switch (tag) {
    case DW_TAG_volatile_type:
    case DW_TAG_typedef:
    {
        DBG_PRINT("TYPEDEF: \n");
        process_record(dies_pool, dies_memb, type_die, tag);
        break;
    }
    case DW_TAG_structure_type:
    case DW_TAG_union_type:
    {
        dies_memb_t *last = NULL;

        DBG_PRINT("TYPE: \n");

        dies_memb->memb_type = DIES_MEMB_TYPE_STRUCT;
        if (DW_TAG_union_type == tag) {
            // AV: to do rework to fixed length strings
            char s[256];
            sprintf(s, strlen(dies_memb->type_name) ? "union %s" : "union", dies_memb->type_name);
            free(dies_memb->type_name);
            dies_memb->type_name = strdup(s);
        }

        last = process_children(dies_pool, die);

        if (last) {
            dies_memb->flag |= DIES_MEMB_FLAG_IS_PARENT;
            last->flag |= DIES_MEMB_FLAG_IS_LAST;
        }
        break;
    }
    case DW_TAG_member:
    {
        char *type_name;
    
        if (dies_memb != NULL)
        {
            while (1);
        }

        dies_memb = add_record(dies_pool);
        type_name = die_get_type_name(type_die);        // AV: rework required

        dies_memb->memb_type = DIES_MEMB_TYPE_FIELD;
        dies_memb->type_name = strdup(type_name);
        dies_memb->field_name = strdup(name ? name : "");
        dies_memb->offset = die_attr_offset(die);
        dies_memb->size = die_get_size(die);
        record_format(dies_memb, type_die);

        DBG_PRINT4("MEMBER: name='%s' type_name='%s' type_tag='%x' size='%d'\n", name, type_name, tag, dies_memb->size);

        if (!g_hash_table_contains(dies_pool->dies_hash, (gpointer)type_name)) {
            g_hash_table_insert(dies_pool->dies_hash, (gpointer)type_name, (gpointer)dies_memb);
        }

        process_record(dies_pool, dies_memb, type_die, DW_TAG_member);

        if (type_name) free(type_name);
        break;
    }
    case DW_TAG_enumeration_type:
    {
        int nof_children = die_nof_children(die);
        dies_memb_t *cur = NULL;

        DBG_PRINT("ENUMERATION\n");

        record_format(cur, die);

        cur->strings = malloc((nof_children + 1) * sizeof(value_string));
        s_enum_index = 0;

        process_children(dies_pool, die);

        cur->strings[s_enum_index].value = 0;
        cur->strings[s_enum_index].strptr = NULL;

        break;
    }
    case DW_TAG_enumerator:
    {
        dies_memb_t *cur = NULL;
        int          value = die_attr_const_value(die);
        char        *name = die_get_name(die);

        DBG_PRINT3("ENUMERATOR: %d -> %s(%d)\n", s_enum_index, name, value);

        cur->strings[s_enum_index].value = value;
        cur->strings[s_enum_index].strptr = strdup(name);

        s_enum_index += 1;

        break;
    }
    case DW_TAG_array_type:
    {
        dies_memb_t *last = NULL;
        char        *type_name = NULL;      // Must be freed
        int          type_size = 0;
        int          array_size = 0;

        array_size = die_attr_size(die);
        DBG_PRINT1("array_size: %d\n", array_size);

        DBG_PRINT("ARRAY: \n");

        switch (parent_tag) {
        case DW_TAG_member:
        case DW_TAG_array_type:
        case DW_TAG_typedef:
        case DW_TAG_volatile_type:
        {
            dies_memb->memb_type = DIES_MEMB_TYPE_ARRAY;
            type_name = die_get_type_name(type_die);
            type_size = die_get_size(type_die);

            /* Dirty string array implementation */
            if (0 == strcmp(type_name, "char"))
            {
                dies_memb->display = STR_ASCII;
                dies_memb->format = FT_STRINGZ;
            }

            if (die_nof_children(die))
                dies_memb->flag |= DIES_MEMB_FLAG_IS_PARENT;

            last = process_children(dies_pool, die);

            if (last && type_die) {
                dies_memb_t *prev = NULL;
                unsigned int range = last->size;
                char range_s[128];
                last->type_name = strdup(type_name);
                last->field_name = strdup(dies_memb->field_name);
                last->size = type_size;
                last->flag |= DIES_MEMB_FLAG_IS_LAST;
                if (DW_TAG_typedef != parent_tag) {
                    sprintf(range_s, "%s[%u]", dies_memb->type_name, range);
                    free(dies_memb->type_name);
                    dies_memb->type_name = strdup(range_s);
                }

                record_format(last, type_die);

                process_record(dies_pool, dies_memb, type_die, DW_TAG_array_type);

                DBG_PRINT2("cur->size: %u range: %u\n", dies_memb->size, range);
                dies_memb->size *= range;
                if (array_size && array_size != dies_memb->size) {
                    dies_memb->size = array_size;
                }

                // dies_memb ==> Prev
                DBG_PRINT1("cur->size: %u\n", dies_memb->size);

                /*
                dies_memb = prev_record(cur);
                if (DIES_MEMB_TYPE_ARRAY == dies_memb->memb_type) {
                    DBG_PRINT2("prev->size: %u range: %u\n", dies_memb->size, range);
                    dies_memb->size *= range;
                    DBG_PRINT1("prev->size: %u\n", dies_memb->size);
                    sprintf(range_s, "%s[%u]", dies_memb->type_name, range);
                    free(dies_memb->type_name);
                    dies_memb->type_name = strdup(range_s);
                }
                */
            }
            if (type_name) free(type_name);

            break;
        } // End of Array type case

        default:
            ; /* do nothing */
        } // End of Array type switch
        break;
    }
    case DW_TAG_subrange_type:
    {
        DBG_PRINT("SUBRANGE: \n");
        if (dies_memb != NULL)
        {
            while (1);
        }
        dies_memb = add_record(dies_pool);
        
        dies_memb->memb_type = DIES_MEMB_TYPE_FIELD;
        dies_memb->size = process_subrange(die);

        DBG_PRINT1("range: %u\n", dies_memb->size);

        break;
    }
    default:
        ;
    }

    if (type_die) dwarf_dealloc(s_dbg, type_die, DW_DLA_DIE);   // AV: move outside switch
    if (name) dwarf_dealloc(s_dbg, name, DW_DLA_STRING);

    return dies_memb;
}

static dies_memb_t *process_children(dies_pool_t *dies_pool, Dwarf_Die die)
{
    Dwarf_Die child_die = NULL;
    Dwarf_Die sib_die = NULL;
    Dwarf_Die cur_die = NULL;
    dies_memb_t *last_rec = NULL;
    Dwarf_Error error;
    int res;

    DBG_PRINT(">>>>> process_children\n");

    res = dwarf_child(die, &child_die, &error);
    if (res == DW_DLV_ERROR) {
        fprintf(stderr, "dwarf_child failed\n");
        exit(EXIT_FAILURE);
    }
    if (res == DW_DLV_NO_ENTRY) {
        DBG_PRINT("no children\n");
        goto FINISH;
    }

    {
        Dwarf_Half tag;
        tag = die_get_tag(child_die);
        if ((tag != DW_TAG_member) && (tag != DW_TAG_subrange_type)) {
            while (1);  // AV: nonamed child???
        }
    }

    last_rec = process_record(dies_pool, NULL, child_die, 0);       // AV: is really NULL

    cur_die = child_die;

    while (1) {
        res = dwarf_siblingof(s_dbg, cur_die, &sib_die, &error);
        if (res == DW_DLV_ERROR) {
            fprintf(stderr, "dwarf_sublingof failed\n");
            exit(EXIT_FAILURE);
        }
        if (res == DW_DLV_NO_ENTRY) {
            DBG_PRINT("no more children\n");
            break;
        }

        {   // AV: test
            Dwarf_Half tag;
            tag = die_get_tag(sib_die);
            if ((tag != DW_TAG_member) && (tag != DW_TAG_subrange_type))
            {
                while (1);  // AV: nonamed child???
            }
        }

        last_rec = process_record(dies_pool, NULL, sib_die, 0);

        // Preserve Child_die until all siblings processed
        if (cur_die != child_die)
            dwarf_dealloc(s_dbg, cur_die, DW_DLA_DIE);

        cur_die = sib_die;
    }
    if (cur_die != child_die)
        dwarf_dealloc(s_dbg, cur_die, DW_DLA_DIE);

    dwarf_dealloc(s_dbg, child_die, DW_DLA_DIE);

FINISH:

    DBG_PRINT("<<<<< process_children\n");

    return last_rec;
}

static int die_nof_children(Dwarf_Die die)
{
    Dwarf_Die child_die = NULL;
    Dwarf_Die sib_die = NULL;
    Dwarf_Die cur_die = NULL;
    Dwarf_Error error;
    int children = 0;
    int res;

    DBG_PRINT(">>>>> process_children\n");

    res = dwarf_child(die, &child_die, &error);
    if (res == DW_DLV_ERROR) {
        fprintf(stderr, "dwarf_child failed\n");
        exit(EXIT_FAILURE);
    }
    if (res == DW_DLV_NO_ENTRY) {
        DBG_PRINT("no children\n");
        goto FINISH;
    }

    children += 1;

    cur_die = child_die;

    while (1) {
        res = dwarf_siblingof(s_dbg, cur_die, &sib_die, &error);
        if (res == DW_DLV_ERROR) {
            fprintf(stderr, "dwarf_sublingof failed\n");
            exit(EXIT_FAILURE);
        }
        if (res == DW_DLV_NO_ENTRY) {
            DBG_PRINT("no more children\n");
            break;
        }

        children += 1;

        if (cur_die != child_die)
            dwarf_dealloc(s_dbg, cur_die, DW_DLA_DIE);

        cur_die = sib_die;
    }
    if (cur_die != child_die)
        dwarf_dealloc(s_dbg, cur_die, DW_DLA_DIE);

    dwarf_dealloc(s_dbg, child_die, DW_DLA_DIE);

FINISH:

    DBG_PRINT("<<<<< process_children\n");

    return children;
}

static Dwarf_Die typedef_to_type(Dwarf_Die die)
{
    Dwarf_Die  cur_die;
    Dwarf_Die  type_die;
    Dwarf_Half tag;

    type_die = die_get_type(die);
    if (NULL == type_die) {
        fprintf(stderr, "no type for typedef die\n");
        exit(EXIT_FAILURE);
    }
    tag = die_get_tag(type_die);
    cur_die = type_die;

    DBG_PRINT1("typedef_to_type(%x)\n", tag);

    /* Go through till first non-typedef tag */
    while (DW_TAG_typedef == tag || DW_TAG_volatile_type == tag) {
        type_die = die_get_type(cur_die);
        tag = die_get_tag(type_die);

        DBG_PRINT1("typedef_to_type(%x)\n", tag);

        if (type_die != cur_die)
            dwarf_dealloc(s_dbg, cur_die, DW_DLA_DIE);
        cur_die = type_die;
    }

    return cur_die;
}

static void record_format(dies_memb_t *rec, Dwarf_Die die)
{
    ftenum_t       format = FT_NONE;
    field_display_e display = BASE_NONE;
    Dwarf_Die      type_die = die;
    Dwarf_Half     tag = die_get_tag(type_die);

    if (DW_TAG_typedef == tag || DW_TAG_volatile_type == tag) {
        type_die = typedef_to_type(type_die);
        tag = die_get_tag(type_die);
    }

    if (DW_TAG_pointer_type == tag) {
        display = BASE_HEX;
        format = FT_UINT32; // AV: TODO: platform dependend part. Fix required
    }
    else if (DW_TAG_enumeration_type == tag) {
        display = BASE_HEX;
        format = FT_UINT32;
    }
    else if (DW_TAG_base_type == tag) {
        char *type_name = die_get_name(type_die);

#define MATCH_TYPE(type_str,type_substr,unsign_format,sign_format)  \
    if (strstr(type_str, type_substr)) {                            \
      if (strstr(type_str, "unsigned")) {                           \
        format = unsign_format;                                     \
        display = BASE_DEC_HEX;                                     \
            } else {                                                      \
        format = sign_format;                                       \
        display = BASE_DEC;                                         \
            }                                                             \
      goto END_MATCH_TYPE;                                          \
        }
#define MATCH_TYPE_END                          \
    END_MATCH_TYPE:

        MATCH_TYPE(type_name, "long long", FT_UINT64, FT_INT64);
        MATCH_TYPE(type_name, "long", FT_UINT32, FT_INT32);
        MATCH_TYPE(type_name, "short", FT_UINT16, FT_INT16);
        MATCH_TYPE(type_name, "char", FT_UINT8, FT_INT8);
        MATCH_TYPE(type_name, "int", FT_UINT32, FT_INT32);
        MATCH_TYPE(type_name, "_Bool", FT_UINT8, FT_INT8);
        MATCH_TYPE_END;

        DBG_PRINT3("FORMAT(%s) -> (%d,%d)\n", type_name, format, display);
    }
    rec->display = display;
    rec->format = format;
}

static void dies_check_memory(dies_pool_t *dies_pool)
{
    if (dies_pool->next_idx == dies_pool->size) { /* size limit is reached, reallocate memory */
        DBG_PRINT("MEMORY REALLOCATION\n");
        dies_pool->size += DIE_SIZE_STEP;
        dies_pool->dies = realloc(dies_pool->dies, dies_pool->size * sizeof(dies_memb_t));
        if (NULL == dies_pool->dies) {
            fprintf(stderr, "realloc failed\n");
            exit(EXIT_FAILURE);
        }
    }
}


static Dwarf_Half die_get_tag(Dwarf_Die die)
{
    Dwarf_Error error;
    Dwarf_Half tag;
    int res;

    res = dwarf_tag(die, &tag, &error);
    if (res != DW_DLV_OK) {
        fprintf(stderr, "dwarf_tag failed\n");
        exit(EXIT_FAILURE);
    }

    return tag;
}

static unsigned die_attr_size(Dwarf_Die die)
{
    Dwarf_Attribute attr = 0;
    Dwarf_Unsigned udata = 0;
    Dwarf_Error error;
    unsigned data = 0;
    int res;

    res = dwarf_attr(die, DW_AT_byte_size, &attr, &error);
    if (res == DW_DLV_ERROR) {
        fprintf(stderr, "dwarf_attr failed\n");
        exit(EXIT_FAILURE);
    }
    if (res == DW_DLV_NO_ENTRY) {
        goto FINISH;
    }

    res = dwarf_formudata(attr, &udata, &error);
    if (res == DW_DLV_OK) {
        data = (unsigned)udata;
    }
    else if (res == DW_DLV_ERROR) {
        fprintf(stderr, "dwarf_formudata failed\n");
        exit(EXIT_FAILURE);
    }
FINISH:
    dwarf_dealloc(s_dbg, attr, DW_DLA_ATTR);
    return data;
}

static int die_attr_const_value(Dwarf_Die die)
{
    Dwarf_Attribute attr = 0;
    Dwarf_Unsigned udata = 0;
    Dwarf_Signed sdata = 0;
    Dwarf_Error error;
    int data = 0;
    int res;

    res = dwarf_attr(die, DW_AT_const_value, &attr, &error);
    if (res == DW_DLV_ERROR) {
        fprintf(stderr, "dwarf_attr failed\n");
        exit(EXIT_FAILURE);
    }
    if (res == DW_DLV_NO_ENTRY) {
        goto FINISH;
    }

    res = dwarf_formsdata(attr, &sdata, &error);
    if (res == DW_DLV_OK) {
        data = (int)sdata;
    }
    else if (res == DW_DLV_ERROR) {
        fprintf(stderr, "dwarf_formsdata failed\n");
        res = dwarf_formudata(attr, &udata, &error);
        if (res == DW_DLV_OK) {
            data = (int)udata;
        }
        else if (res == DW_DLV_ERROR) {
            fprintf(stderr, "dwarf_formudata failed\n");
            goto FINISH;
        }
    }
FINISH:
    dwarf_dealloc(s_dbg, attr, DW_DLA_ATTR);
    return data;
}

static unsigned die_attr_offset(Dwarf_Die die)
{
    Dwarf_Locdesc *llbuf = 0;
    Dwarf_Locdesc **llbufarray = 0;
    Dwarf_Signed no_of_elements = 0;

    enum Dwarf_Form_Class class = DW_FORM_CLASS_UNKNOWN;
    Dwarf_Half attr_name = DW_AT_data_member_location;
    Dwarf_Half form, directform;
    Dwarf_Half version;
    Dwarf_Half offset_size;
    Dwarf_Attribute attr = 0;
    Dwarf_Error error = 0;
    unsigned offset = 0;
    int llent = 0;
    int i;
    int res;

    res = dwarf_attr(die, attr_name, &attr, &error);
    if (res == DW_DLV_ERROR) {
        fprintf(stderr, "dwarf_attr failed\n");
        exit(EXIT_FAILURE);
    }
    if (res == DW_DLV_NO_ENTRY) {
        goto FINISH;
    }

    res = dwarf_whatform(attr, &form, &error);
    if (res != DW_DLV_OK) {
        fprintf(stderr, "dwarf_whatform failed\n");
        exit(EXIT_FAILURE);
    }

    dwarf_whatform_direct(attr, &directform, &error);

    res = dwarf_get_version_of_die(die, &version, &offset_size);
    if (res != DW_DLV_OK) {
        fprintf(stderr, "dwarf_get_version_of_die failed\n");
        exit(EXIT_FAILURE);
    }

    class = dwarf_get_form_class(version, attr_name, offset_size, form);

    if (class != DW_FORM_CLASS_BLOCK) {
        fprintf(stderr, "CLASS = '%x'\n", class);
        goto FINISH;
    }

    res = dwarf_loclist_n(attr, &llbufarray, &no_of_elements, &error);
    if (res != DW_DLV_OK) {
        fprintf(stderr, "dwarf_loclist_n failed\n");
        exit(EXIT_FAILURE);
    }

    for (llent = 0; llent < no_of_elements; ++llent) {
        Dwarf_Half no_of_ops = 0;

        llbuf = llbufarray[llent];
        no_of_ops = llbuf->ld_cents;
        for (i = 0; i < no_of_ops; i++) {
            Dwarf_Loc *op = &llbuf->ld_s[i];
            Dwarf_Unsigned data;
            data = op->lr_number;
            offset = (unsigned)data;
            goto FINISH;
        }
    }

FINISH:
    for (i = 0; i < no_of_elements; ++i) {
        dwarf_dealloc(s_dbg, llbufarray[i]->ld_s, DW_DLA_LOC_BLOCK);
        dwarf_dealloc(s_dbg, llbufarray[i], DW_DLA_LOCDESC);
    }
    dwarf_dealloc(s_dbg, llbufarray, DW_DLA_LIST);
    return offset;
}

static char* die_get_name(Dwarf_Die die)
{
    char *name = NULL;
    Dwarf_Error error;
    int res;

    res = dwarf_diename(die, &name, &error);
    if (res == DW_DLV_ERROR) {
        fprintf(stderr, "dwarf_diename failed\n");
        exit(EXIT_FAILURE);
    }

    return name;
}

static char* die_get_type_name(Dwarf_Die die)
{
    static char res[256];

    memset(res, 0, 256);
    die_get_type_name_(die, res);

    return strdup(res);
}

static void die_get_type_name_(Dwarf_Die die, char *res)
{
    Dwarf_Half tag;

    tag = die_get_tag(die);

    if (tag == DW_TAG_base_type
        || tag == DW_TAG_structure_type
        || tag == DW_TAG_union_type
        || tag == DW_TAG_typedef) {
        char *name = NULL;
        name = die_get_name(die);
        if (name)
            FORMAT_TYPE2(res, name, "%s%s");
        if (name) dwarf_dealloc(s_dbg, name, DW_DLA_STRING);
    }
    else {
        Dwarf_Die type_die = NULL;
        switch (tag) {
        case DW_TAG_pointer_type:
            FORMAT_TYPE(res, "*%s");
            break;
        case DW_TAG_subroutine_type:
            FORMAT_TYPE(res, "%s()");
            break;
        case DW_TAG_array_type:
            /* FORMAT_TYPE(res, "%s[]"); */
            break;
        default:
            ; /* do nothing */
        }
        type_die = die_get_type(die);
        if (type_die) {
            die_get_type_name_(type_die, res);
        }
        else if (DW_TAG_pointer_type == tag
                 || DW_TAG_subroutine_type == tag) {
            FORMAT_TYPE(res, "void%s");
        }
        dwarf_dealloc(s_dbg, type_die, DW_DLA_DIE);
    }
}

static unsigned process_subrange(Dwarf_Die die)
{
    Dwarf_Attribute attr = 0;
    Dwarf_Unsigned udata = 0;
    Dwarf_Signed sdata = 0;
    Dwarf_Error error;
    unsigned range = 0;
    int res;

    DBG_PRINT(">>>>> process_subrange\n");

    res = dwarf_attr(die, DW_AT_upper_bound, &attr, &error);
    if (res == DW_DLV_ERROR) {
        fprintf(stderr, "dwarf_attr failed\n");
        exit(EXIT_FAILURE);
    }
    if (res == DW_DLV_NO_ENTRY) {
        range = 0;
        goto FINISH_NO_ATTR;
    }

    res = dwarf_formudata(attr, &udata, &error);
    if (res == DW_DLV_OK) {
        range = (unsigned)udata + 1;
    }
    else if (res == DW_DLV_ERROR) {
        fprintf(stderr, "dwarf_formudata failed\n");
        res = dwarf_formsdata(attr, &sdata, &error);
        if (res == DW_DLV_OK) {
            range = (unsigned)sdata + 1;
        }
        else if (res == DW_DLV_ERROR) {
            fprintf(stderr, "dwarf_formsdata failed\n");
            goto FINISH;
        }
    }

FINISH:
    dwarf_dealloc(s_dbg, attr, DW_DLA_ATTR);
FINISH_NO_ATTR:

    DBG_PRINT("<<<<< process_subrange\n");

    return range;
}

static Dwarf_Die die_get_type(Dwarf_Die die)
{
    Dwarf_Attribute attr = 0;
    Dwarf_Off offset = 0;
    Dwarf_Die type_die = 0;
    Dwarf_Error error;
    int res;

    res = dwarf_attr(die, DW_AT_type, &attr, &error);
    if (res == DW_DLV_ERROR) {
        fprintf(stderr, "dwarf_attr failed\n");
        exit(EXIT_FAILURE);
    }
    if (res == DW_DLV_NO_ENTRY) {
        goto FINISH;
    }

    res = dwarf_global_formref(attr, &offset, &error);
    if (res != DW_DLV_OK) {
        fprintf(stderr, "dwarf_global_formref failed\n");
        exit(EXIT_FAILURE);
    }

    res = dwarf_offdie_b(s_dbg, offset, is_info, &type_die, &error);
    if (res == DW_DLV_ERROR) {
        fprintf(stderr, "dwarf_offdie_b failed\n");
        exit(EXIT_FAILURE);
    }
    if (res == DW_DLV_NO_ENTRY) {
        goto FINISH;
    }

FINISH:
    dwarf_dealloc(s_dbg, attr, DW_DLA_ATTR);

    return type_die;
}

static unsigned die_get_size(Dwarf_Die die)
{
    Dwarf_Die cur_die = NULL;
    Dwarf_Half tag;
    unsigned size;
    cur_die = die;
    while (1) {
        Dwarf_Die type_die;

        size = die_attr_size(cur_die);
        tag = die_get_tag(cur_die);

        if (0 != size && DW_TAG_array_type != tag) { /* skip array size, we need array element size */
            break;
        }
        else if (DW_TAG_pointer_type == tag) { /* set 4 bytes size for pointers */
            size = 4;
            break;
        }

        type_die = die_get_type(cur_die);
        if (NULL == type_die) {
            break;
        }
        if (die != cur_die)
            dwarf_dealloc(s_dbg, cur_die, DW_DLA_DIE);

        cur_die = type_die;
    }

    if (die != cur_die)
        dwarf_dealloc(s_dbg, cur_die, DW_DLA_DIE);

    return size;
}



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
static char* format2str(ftenum_t format)
{
    static char *ftenum_str[FT_NUM_TYPES] = {
        [FT_NONE         ] = "FT_NONE         ",      /* used for text labels with no value */
        [FT_PROTOCOL     ] = "FT_PROTOCOL     ",
        [FT_BOOLEAN      ] = "FT_BOOLEAN      ",      /* TRUE and FALSE come from <glib.h> */
        [FT_UINT8        ] = "FT_UINT8        ",
        [FT_UINT16       ] = "FT_UINT16       ",
        [FT_UINT24       ] = "FT_UINT24       ",      /* really a UINT32 but displayed as 3 hex-digits if FD_HEX*/
        [FT_UINT32       ] = "FT_UINT32       ",
        [FT_UINT64       ] = "FT_UINT64       ",
        [FT_INT8         ] = "FT_INT8         ",
        [FT_INT16        ] = "FT_INT16        ",
        [FT_INT24        ] = "FT_INT24        ",      /* same as for UINT24 */
        [FT_INT32        ] = "FT_INT32        ",
        [FT_INT64        ] = "FT_INT64        ",
        [FT_FLOAT        ] = "FT_FLOAT        ",
        [FT_DOUBLE       ] = "FT_DOUBLE       ",
        [FT_ABSOLUTE_TIME] = "FT_ABSOLUTE_TIME",
        [FT_RELATIVE_TIME] = "FT_RELATIVE_TIME",
        [FT_STRING       ] = "FT_STRING       ",
        [FT_STRINGZ      ] = "FT_STRINGZ      ",      /* for use with proto_tree_add_item() */
        [FT_UINT_STRING  ] = "FT_UINT_STRING  ",      /* for use with proto_tree_add_item() */
        [FT_ETHER        ] = "FT_ETHER        ",
        [FT_BYTES        ] = "FT_BYTES        ",
        [FT_UINT_BYTES   ] = "FT_UINT_BYTES   ",
        [FT_IPv4         ] = "FT_IPv4         ",
        [FT_IPv6         ] = "FT_IPv6         ",
        [FT_IPXNET       ] = "FT_IPXNET       ",
        [FT_FRAMENUM     ] = "FT_FRAMENUM     ",      /* a UINT32 but if selected lets you go to frame with that number */
        [FT_PCRE         ] = "FT_PCRE         ",      /* a compiled Perl-Compatible Regular Expression object */
        [FT_GUID         ] = "FT_GUID         ",      /* GUID UUID */
        [FT_OID          ] = "FT_OID          ",      /* OBJECT IDENTIFIER */
        [FT_EUI64        ] = "FT_EUI64        ",
        [FT_AX25         ] = "FT_AX25         ",
        [FT_VINES        ] = "FT_VINES        ",
        [FT_REL_OID      ] = "FT_REL_OID      ",
        [FT_SYSTEM_ID    ] = "FT_SYSTEM_ID    ",
        [FT_STRINGZPAD   ] = "FT_STRINGZPAD   ",
        [FT_FCWWN        ] = "FT_FCWWN        ",
  };

    return ftenum_str[format];
}

static char* display2str(field_display_e disp)
{
    static char *display_str[] =  {
      [BASE_NONE   ] = "BASE_NONE   ",    /**< none */
      [BASE_DEC    ] = "BASE_DEC    ",    /**< decimal */
      [BASE_HEX    ] = "BASE_HEX    ",    /**< hexadecimal */
      [BASE_OCT    ] = "BASE_OCT    ",    /**< octal */
      [BASE_DEC_HEX] = "BASE_DEC_HEX",    /**< decimal (hexadecimal) */
      [BASE_HEX_DEC] = "BASE_HEX_DEC",    /**< hexadecimal (decimal) */
      [BASE_CUSTOM ] = "BASE_CUSTOM "     /**< call custom routine (in ->strings) to format */
    };

    return display_str[disp];
}
static void dies_print(FILE *fout, dies_memb_t *dies_memb, int cur_level)
{
#define MAX_INDENT    (10 * 4)
    static char flag_str[5];
    static char indent_str[MAX_INDENT+1] = {0};
    static char data_name[256] = {0};

    if (dies_memb == NULL && cur_level == 0) {
        // Print header
        fprintf(fout, "\n");
        fprintf(fout, "\n");
        fprintf(fout, "\n");
        fprintf(fout, "Dies members:\n");
        fprintf(fout, "Type: (S)truct, (A)array;     Flags: (L)ast, (P)arent, (R)oot, (S)SCD entry\n");
        fprintf(fout, "=================================================================================================================================================\n");
        fprintf(fout, " %-3s | %-3s | %-1s | %-40s | %-4s | %-4s | %-s | %-16s | %-12s |\n",
                      "ID", "Lnk", "T", "Data type", "Offs", "Size", "Flag", "Format", "Display");
        fprintf(fout, "=================================================================================================================================================\n");
        return;
    }
    else if (dies_memb == NULL && cur_level == -1) {
        // Print footer
        fprintf(fout, "=================================================================================================================================================\n");
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

//                 ID    | LinkID
//                       |      | DieType   
//                       |      |     |  Level Indent, Data Type & Name          
//                       |      |     |       | Offset  
//                       |      |     |       |     | Size     
//                       |      |     |       |     |     | Flag
//                       |      |     |       |     |     |     | Format
//                       |      |     |       |     |     |     |      | Display
    fprintf(fout, " %-3d | %-3d | %1s | %-40s | %4d | %4d | %4s | %16s | %12s | \n",
        dies_memb->self_id,
        dies_memb->link_id,
        get_dies_memb_type_str(dies_memb->memb_type),
        data_name,
        dies_memb->offset,
        dies_memb->size,
        get_dies_flag_str(dies_memb->flag, flag_str),

        format2str(dies_memb->format),
        display2str(dies_memb->display)
    );

    indent_str[cur_level * 4] = ' ';
}

static dies_memb_t* dies_print_lvl(FILE *fout, dies_memb_t *dies_memb, int cur_level, int max_idx)
{
    /* sanity check here */

    do {
        gint flag;

        if (dies_memb->self_id == max_idx - 1){
            fprintf(fout, "ERROR\n");
            return dies_memb;
        }

        flag = dies_memb->flag;

        dies_print(fout, dies_memb, cur_level);

        dies_memb ++;

        // Print child
        if (flag & DIES_MEMB_FLAG_IS_PARENT) {
            dies_memb = dies_print_lvl(fout, dies_memb, cur_level+1, max_idx);
        } 

        if (flag & DIES_MEMB_FLAG_IS_LAST) {
            return dies_memb;
        } 

    } while (1);

    fprintf(fout, "ERROR\n");

    return NULL;
}

void dies_pool_print(FILE *fout, GSList *dies_pools_list)
{
    GSList *list_entry;
    list_entry = g_slist_nth(dies_pools_list, 0);
    while (list_entry){
        dies_pool_t *dies_pool;

        dies_pool = g_slist_nth_data(list_entry, 0);
        if (dies_pool->size > 0) {
            dies_memb_t *die;
            dies_print(fout, NULL, 0);
            die = dies_print_lvl(fout, &dies_pool->dies[0], 0, dies_pool->next_idx);  // SCD root entries
            dies_print_lvl(fout, die, 0, dies_pool->next_idx);

            dies_print(fout, NULL, -1);
        }
        list_entry = g_slist_next(list_entry);
    }
}

void print_ids(FILE *fout, scd_entry_t *scd_entry, unsigned int ids_size)
{
  unsigned int i;
  fprintf(fout, "IDS:\n");
  for (i = 0; i < ids_size; ++i) {
    fprintf(fout, "{ %2d, %2d, %4d, %-60s %p }\n", 
        scd_entry[i].eid,
        scd_entry[i].fid,
        scd_entry[i].lid,
        scd_entry[i].str,
        scd_entry[i].data);
  };

}
