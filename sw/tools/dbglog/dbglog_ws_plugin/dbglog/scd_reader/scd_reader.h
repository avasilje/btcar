#ifndef __SCD_READER_H__
#define __SCD_READER_H__

#define SCD_LINE_SIZE 1024

#define SCD_ENTRY_TAG_ELF    'E'      // Tag that defines ELF file
#define SCD_ENTRY_TAG_FID    'F'      // Tag that defines files IDs
#define SCD_ENTRY_TAG_MSG    'D'      // Tag that defines Messages ID (ELF ID, File ID, Line ID)

typedef struct _scd_entry_t {
  char          tag;                  // Memb of  SCD_ENTRY_TAG_xxx
  int           eid;
  int           fid;
  int           lid;
  const char    *str;
  GSList        *dies_list;         // List of root entries in DIEs table for this SCD's data types. DIEs are always linked.
  int           status;
} scd_entry_t;

typedef struct _scd_info_t {
  guint32     signature;
  
  GHashTable  *msg_hash;           // The hash containing whole SCD MSG. Hash - func(IDs); Key - all IDs
  GHashTable  *fid_hash;
  GHashTable  *elf_hash;           // The hash containing names of elf files. Hash - EID; Key - elf file name

  scd_entry_t   *scd_table;
  guint          scd_table_size;
  guint          scd_proc_closed_flag;

} scd_info_t;


extern scd_info_t* 
scd_init(void);

extern void 
scd_free(scd_info_t *scd_info);

extern gint 
scd_process(const char *scd_fn, scd_info_t *scd_info);

extern void
scd_process_finish(scd_info_t *scd_info);

extern const char*
scd_get_elfname(scd_info_t *scd_info, guint32 eid);

extern const char*
scd_get_fidname(scd_info_t *scd_info, guint32 eid, guint32 fid);

extern void*
scd_get_data_by_ids(scd_info_t *scd_info, guint32 eid, guint32 fid, guint32 lid);

extern const char*
scd_get_msg_by_ids(scd_info_t *scd_info, guint32 eid, guint32 fid, guint32 lid);

scd_entry_t*
scd_get_by_ids(scd_info_t *scd_info, guint32 eid, guint32 fid, guint32 lid);

extern char*
scd_get_next_dtyp(const char *str_in, size_t *str_out_len);

#endif /* __SCD_READER_H__ */
