#ifndef _SCD_DWARF_PARSER_H_
#define _SCD_DWARF_PARSER_H_

#ifdef WIRESHARK_ENV
#include <config.h>
#include <epan\ftypes\ftypes.h>
#include <epan\proto.h>
#include <epan\value_string.h>
#else
#include "scd_dwarf_wenv.h"
#endif 

#define DIES_MEMB_FLAG_IS_LAST          1       // Indicates last sibling of child
#define DIES_MEMB_FLAG_IS_PARENT        2       // Indicates that child present
#define DIES_MEMB_FLAG_IS_ROOT          4       // Root of type definition
#define DIES_MEMB_FLAG_SCD_ROOT         8       // DIES_MEMB added by SCD parser. Not processed by dwarf_parser

typedef enum _dies_memb_type_t{
  DIES_MEMB_TYPE_STRUCT,
  DIES_MEMB_TYPE_ARRAY,
  DIES_MEMB_TYPE_FIELD,
  DIES_MEMB_TYPE_LAST
} dies_memb_type_t;

typedef struct _dies_memb_t {

  dies_memb_type_t  memb_type;
  char              *type_name;     // Need to be freed
  char              *field_name;
  guint32           offset;         // Relative to parent
  guint32           size;
  
  ftenum_t          format;
  field_display_e    display;
  value_string      *strings;      
  
  gint self_id;
  gint link_id;
  gint flag;

  int   hf_id;              // field ID filled by WireShark on registration
  gint  ett_id;             // subtree ID filled by WireShark on registration 
   
} dies_memb_t;

typedef struct _dies_pool_t {
  char        *elf_fname;
  int          elf_id;
  dies_memb_t *dies;
  guint        size;
  guint        next_idx;
  GHashTable  *dies_hash;           // Is used to store already processed DIEs members. Contain all but SCD_DIEs
  GHashTable  *dies_scd_hash;       // Is used to store all unique entries from SCD files
                                    // Contains only SCD_DIEs which are always linked to a corresponding ROOT die
} dies_pool_t;

#define RESULT_OK   0
#define RESULT_FAIL 1

int dwarf2_init();
int scd_dwarf_parse(char *scd_fn, scd_info_t **scd_info_out, GSList **dies_pools_list);
int dwarf2_finish();

#endif /* _SCD_DWARF_PARSER_H_ */
