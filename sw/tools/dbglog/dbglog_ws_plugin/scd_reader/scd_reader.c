/* TODO: add SCD_close to free resources */
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <glib.h>

#include "scd_reader.h"

#define RESULT_OK   0
#define RESULT_FAIL 1

#define SCD_ALLOC_STEP_SIZE (2048)
#define SCD_TYPENAME_SIZE (SCD_LINE_SIZE-16)

#define SCD_INFO_SIGNATURE 0x8343BEAD   /* SC */

gboolean
scd_entry_ids_is_equal(gconstpointer scd_entry1_v, gconstpointer scd_entry2_v){

  const scd_entry_t *scd_entry1 = (scd_entry_t *)scd_entry1_v;
  const scd_entry_t *scd_entry2 = (scd_entry_t *)scd_entry2_v;

  return ((scd_entry1->lid == scd_entry2->lid) && 
          (scd_entry1->fid == scd_entry2->fid) && 
          (scd_entry1->eid == scd_entry2->eid));
}

gboolean
scd_entry_eid_is_equal(gconstpointer scd_entry1_v, gconstpointer scd_entry2_v){

  const scd_entry_t *scd_entry1 = (scd_entry_t *)scd_entry1_v;
  const scd_entry_t *scd_entry2 = (scd_entry_t *)scd_entry2_v;

  return (scd_entry1->eid == scd_entry2->eid);
}

gboolean
scd_entry_fid_is_equal(gconstpointer scd_entry1_v, gconstpointer scd_entry2_v){

  const scd_entry_t *scd_entry1 = (scd_entry_t *)scd_entry1_v;
  const scd_entry_t *scd_entry2 = (scd_entry_t *)scd_entry2_v;

  return (scd_entry1->fid == scd_entry2->fid);
}

gboolean
scd_evt_tag_oid_is_equal(gconstpointer scd_evt1_v, gconstpointer scd_evt2_v){

  //const scd_evt_t *scd_evt1 = (scd_evt_t *)scd_evt1_v;
  //const scd_evt_t *scd_evt2 = (scd_evt_t *)scd_evt2_v;

  //return ((scd_evt1->tag == scd_evt2->tag) && 
  //        (scd_evt1->oid == scd_evt2->oid));

  return 0;
}

guint 
scd_evt_hash_tag_oid_key(gconstpointer scd_evt_v){

  const scd_entry_t *scd_entry = (scd_entry_t *)scd_evt_v;

  //return (scd_evt->tag << 16) +
  //       (scd_evt->oid <<  0);

  return 0;
}

guint 
scd_entry_hash_ids_key(gconstpointer scd_entry_v){

  const scd_entry_t *scd_entry = (scd_entry_t *)scd_entry_v;

  // eid   0    0    0         -- ELF file ID
  // fid  fid   0    0         -- File ID
  //  0    0   lid  lid        -- Line ID

  return (scd_entry->eid << 24) +
         (scd_entry->fid << 16) +
         (scd_entry->lid <<  0);
}

guint 
scd_entry_hash_elf_key(gconstpointer scd_entry_v){

  const scd_entry_t *scd_entry = (scd_entry_t *)scd_entry_v;

  return scd_entry->eid;
}

guint 
scd_entry_hash_fid_key(gconstpointer scd_entry_v){

  const scd_entry_t *scd_entry = (scd_entry_t *)scd_entry_v;

  return scd_entry->fid;
}

gboolean
scd_entry_is_equal(gconstpointer scd_entry1_v, gconstpointer scd_entry2_v){

  const scd_entry_t *scd_entry1 = (scd_entry_t *)scd_entry1_v;
  const scd_entry_t *scd_entry2 = (scd_entry_t *)scd_entry2_v;

  return (strcmp(scd_entry1->str, scd_entry2->str) == 0);
}

scd_info_t* 
scd_init(guint32 hash_flags){

  scd_info_t  *scd_info;

  scd_info = g_malloc(sizeof(*scd_info));
  if (!scd_info)
    return NULL;

  scd_info->signature = SCD_INFO_SIGNATURE;

  scd_info->elf_hash = NULL;
  scd_info->fid_hash = NULL;
  scd_info->msg_hash = NULL;

  scd_info->scd_table = NULL;
  scd_info->scd_table_size = 0;

  scd_info->elf_hash = g_hash_table_new(scd_entry_hash_elf_key, scd_entry_eid_is_equal);
  scd_info->fid_hash = g_hash_table_new(scd_entry_hash_fid_key, scd_entry_fid_is_equal);
  scd_info->msg_hash = g_hash_table_new(scd_entry_hash_ids_key, scd_entry_ids_is_equal);

  scd_info->scd_proc_closed_flag = 0;

  return scd_info;
}

static gint 
parse_scd(const char *scd_fn, scd_entry_t **scd_table, guint *scd_table_size){

  gint res = RESULT_OK;
  FILE *fp;
  char line[SCD_LINE_SIZE];
  
  scd_entry_t *tbl;
  guint curr_size;
  guint i;

  fp = NULL;
  fp = fopen(scd_fn, "r");
  if (fp == NULL)
    return RESULT_FAIL;

  tbl = *scd_table;
  curr_size = *scd_table_size;

  i = curr_size;

  while (fgets(line, SCD_LINE_SIZE, fp) != NULL) {
    gchar buf[SCD_TYPENAME_SIZE];
    gchar *s = &line[2];
    gchar tag;
    scd_entry_t *scd_entry;

    if (curr_size == i) { /* size limit is reached or not allocated yet */
      curr_size += SCD_ALLOC_STEP_SIZE;
      tbl = (scd_entry_t*)g_realloc(tbl, curr_size * sizeof(scd_entry_t));
      if (NULL == tbl){
        res = RESULT_FAIL;
        goto FINISH;
      }else{
        *scd_table_size = i;
        *scd_table = tbl;
      }
    }

    tag = line[0];
    scd_entry = &(tbl)[i++];

    switch (tag){

      case SCD_ENTRY_TAG_ELF:
          sscanf(s, "%d %[^\n]s", &scd_entry->eid, buf);
          scd_entry->str = g_strdup(buf);

          scd_entry->fid = 0;
          scd_entry->lid = 0;
          scd_entry->tag = tag;
          scd_entry->data = NULL;
          scd_entry->dies_list = NULL;
          break;

      case SCD_ENTRY_TAG_FID:

          sscanf(s, "%d %[^\n]s", &scd_entry->fid, buf);
          scd_entry->str = g_strdup(buf);

          scd_entry->eid = 0;
          scd_entry->lid = 0;
          scd_entry->tag = tag;
          scd_entry->data = NULL;
          scd_entry->dies_list = NULL;
          break;

      case SCD_ENTRY_TAG_MSG:

          sscanf(s, "%d %d %d %[^\n]s", &scd_entry->eid, &scd_entry->fid, &scd_entry->lid, buf);
          scd_entry->str = g_strdup(buf);

          scd_entry->tag = tag;
          scd_entry->data = NULL;
          scd_entry->dies_list = NULL;
          break;

      default:
          break;
    }

  }

FINISH:

  if (fp){
    fclose(fp);
  }

  if (res == RESULT_OK) {
    *scd_table_size = i;
  }

  return res;
}

gint 
process_scd(const char *scd_fn, scd_info_t *scd_info){

  if (scd_info->scd_proc_closed_flag)   
    return RESULT_FAIL;       /* SCD table already hashed and can't be reallocated 
                                 Rework realloc() to overcome this limitation      */

  return parse_scd(scd_fn, &scd_info->scd_table, &scd_info->scd_table_size); 
}

const char* scd_get_next_dtyp(const char *str_in, char **str_out)
{
  // D   1   1   286   "signature %(sign_struct) just digit %(uint8_t)"
  //                                ^^^^^^^^^^^               ^^^^^^^
    const char *start_pos;

    *str_out = NULL;
    while (*str_in){
        if (*str_in++ != '%'){
            continue;
        }

        // "%" sign found check bracket
        if (*str_in++ != '('){
            continue;
        }

        // Open bracket found. Check closing bracket
        start_pos = str_in;
        while (*str_in){
            if (*str_in != ')'){
                str_in++;
                continue;
            }

            // Data type found. Add to hash
            if (str_in != start_pos){
                // avoid adding empty strings
                *str_out = g_strndup(start_pos, str_in - start_pos);
                return str_in++;
            }

            // bad format. Just ignore
            break;
        }
    }
    
    return NULL;
}

static void
fillup_scd_hashes(scd_info_t *scd_info){

  guint i;
  GHashTable* scd_ids_hash;

  guint       scd_entry_tbl_size;
  scd_entry_t   *scd_entry;

  scd_entry          = scd_info->scd_table;
  scd_entry_tbl_size = scd_info->scd_table_size;

  for (i = 0; i < scd_entry_tbl_size; i++){

    scd_ids_hash = NULL;

    /* Choose proper HASH for SCD entry by its TAG */
    switch(scd_entry->tag){
      case SCD_ENTRY_TAG_MSG:    
        scd_ids_hash = scd_info->msg_hash;
        break;
      case SCD_ENTRY_TAG_ELF:
          scd_ids_hash = scd_info->elf_hash;
          break;
      case SCD_ENTRY_TAG_FID:    
          scd_ids_hash = scd_info->fid_hash;
          break;
      default:
        break;
    }

    if (scd_ids_hash){
      g_hash_table_insert(scd_ids_hash, (gpointer)scd_entry, (gpointer)scd_entry);
    }

    scd_entry++;
  }

  return;
} 

void 
scd_process_finish(scd_info_t *scd_info){

  /* Note: The function required to support several SCD files 
     Hash can't be filled on-fly because data structures might be 
     reallocated
  */
  fillup_scd_hashes(scd_info);
  scd_info->scd_proc_closed_flag = 1;

  return;
}

const char*
scd_get_elfname(scd_info_t *scd_info, guint32 eid){

  scd_entry_t  scd_entry_key;
  scd_entry_t  *scd_entry;
  
  scd_entry_key.eid = eid;  // Other fields are not used and left uninitialized

  scd_entry = (scd_entry_t*)g_hash_table_lookup(scd_info->elf_hash, &scd_entry_key);

  if (scd_entry) return scd_entry->str;
  else return NULL;
}

const char*
scd_get_fidname(scd_info_t *scd_info, guint32 fid){

  scd_entry_t  scd_entry_key;
  scd_entry_t  *scd_entry;
  
  scd_entry_key.fid = fid;  // Other fields are not used and left uninitialized

  scd_entry = (scd_entry_t*)g_hash_table_lookup(scd_info->fid_hash, &scd_entry_key);

  if (scd_entry) return scd_entry->str;
  else return NULL;
}

scd_entry_t*
scd_get_by_ids(scd_info_t *scd_info, guint32 eid, guint32 fid, guint32 lid){

  scd_entry_t   scd_entry_key;
  scd_entry_t  *scd_entry;
  
  scd_entry_key.eid = eid;
  scd_entry_key.fid = fid;
  scd_entry_key.lid = lid;

  scd_entry = (scd_entry_t*)g_hash_table_lookup(scd_info->msg_hash, &scd_entry_key);

  return scd_entry;
}


const char*
scd_get_msg_by_ids(scd_info_t *scd_info, guint32 eid, guint32 fid, guint32 lid){

  scd_entry_t  *scd_entry;
    
  scd_entry = scd_get_by_ids(scd_info, eid, fid, lid);
  
  if (scd_entry) return scd_entry->str;
  else return NULL;
}

void*
scd_get_data_by_ids(scd_info_t *scd_info, guint32 eid, guint32 fid, guint32 lid){
  scd_entry_t  *scd_entry;
    
  scd_entry = scd_get_by_ids(scd_info, eid, fid, lid);
  
  if (scd_entry) return scd_entry->data;
  else return NULL;
}
