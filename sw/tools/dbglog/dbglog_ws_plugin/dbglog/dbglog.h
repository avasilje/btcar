#ifndef __DBGLOG_H__
#define __DBGLOG_H__

#define SCD_LOG_MSG_INFO_STR_LEN 1024
typedef struct _scd_log_msg_t{
  char     info_str[SCD_LOG_MSG_INFO_STR_LEN];
}scd_log_msg_t;

#define DBGLOG_HDR_PTYPE_MSG_STR   0
#define DBGLOG_HDR_PTYPE_MSG_INT8  1
#define DBGLOG_HDR_PTYPE_MSG_INT32 2
#define DBGLOG_HDR_PTYPE_MSG_INT64 3
#define DBGLOG_HDR_PTYPE_MSG_MAC   4
#define DBGLOG_HDR_PTYPE_MSG_IPV6  5

#define DBGLOG_HDR_PTYPE_MSG_FIRST 0
#define DBGLOG_HDR_PTYPE_MSG_LAST  5

typedef struct _dbglog_hdr_t{
  guint8  ver;
  guint8  flags;
  guint8  mark;

  guint8  eid;
  guint16 fid;
  guint16 lid;

  guint32 data_len;

  gboolean encoding;

  const gchar *eid_name;
  const gchar *fid_name;

} dbglog_hdr_t;

typedef struct _dissect_dies_itt_info_t {
    tvbuff_t    *tvb;
    gint        offset_lvl[100];
    proto_tree *parent_tree[100];
    gint        hf_id_idx;

} dissect_dies_itt_info_t;

#define MAX_PROTO_ABBR  1024
typedef struct _dies_info_t {

  GHashTable *dies_pools_hash;      // One dies_pool per ELF file

  guint   trees_num;
  guint   fields_num;

  int   hf_idx;
  int   ett_idx;

  int       lvl_idx;
  int       max_idx;
  size_t    abbr_lvl[100];      // length of abbr at certain level. Max used index = max_idx ==> Depth level
  char      abbr[MAX_PROTO_ABBR];

  int              *hf_ids;
  hf_register_info  *hf_dies;
  gint  **ett_dies;

  gboolean       encoding;      // AV: ??? is it property of elf file ???

} dies_info_t; 

#define UNREFERENCED_PARAM(x)   x=x

#endif  /* __DBGLOG_H__ */