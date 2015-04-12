/* packet-dbglog.c
 * Routines for DBG Log Viewer protocol packet disassembly
 * By Andrejs Vasiljevs
 * Copyright 2015 Andrejs Vasiljevs
 *
  * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include "config.h"

#include <string.h>
#include <stdio.h>
#include <epan/proto.h>
#include <epan/packet.h>
#include <epan/prefs.h>
#include <wiretap/wtap.h>

#include <scd_reader.h>
#include <scd_dwarf_parser.h>

#define DBG_DEIS_DUMP

#ifdef DBG_DEIS_DUMP
#include <wsutil/file_util.h>
#endif

#include "dbglog.h"

char str_scratch[1024];
dissect_dies_itt_info_t     dissect_dies_itt_info;      // AV: ??? should it be placed inside DIES_INFO ???

#define TAB_STOP_LAST 44
static gint field_name_tab_stop[] = {20, 28, 36, TAB_STOP_LAST};
static gchar white_space_str[TAB_STOP_LAST+2+1] = "                                              "; /* +2 delimiter; +1 null term. */

static scd_info_t   *scd_info;
static dies_info_t  dies_info;

static dbglog_hdr_t  dbglog_hdr;
static scd_log_msg_t  scd_log_msg;

static char *scd_filename  = "C:\\MyWorks\\BTCar\\sw\\tools\\dbglog\\ref_target\\scd.txt";

static int proto_dbglog = -1;

static int hf_dbglog_version   = -1;
static int hf_dbglog_flags     = -1;
static int hf_dbglog_eid       = -1;
static int hf_dbglog_mark       = -1;
static int hf_dbglog_datalen     = -1;
static int hf_dbglog_fid        = -1;
static int hf_dbglog_lid        = -1;

static int hf_dbglog_msg = -1;

static gint ett_msghdr      = -1;
static gint ett_dbglog_msg  = -1;

static gint *ett_dbglog[] = {
    &ett_msghdr,
    &ett_dbglog_msg
  };

const value_string endianess_info[] = {
    {0x0100,     "Native"},
    {0x0001,     "Reversed"},
    {0,      NULL}
};
const value_string endianess_internal[] = {
    {0,     "Native"},
    {1,     "Reversed"},
    {0,      NULL}
};

const value_string *val_str_task_id = NULL;

#define BITMASK(offset, size) (((1 << size)- 1) << offset)
#define GET_BITS(data, offset, size) ((data >> offset) & ((1 << size)- 1))

void proto_reg_handoff_dbglog(void);

/* Fixed fields definitions */
#if 1
static hf_register_info log_event_hf[] = {

  { &hf_dbglog_version, 
    { "Version", "dl.version",
      FT_UINT8, BASE_DEC, NULL, 0, "", HFILL
    }
  },

  { &hf_dbglog_flags, 
    { "Flags", "dl.flags",
      FT_UINT8, BASE_DEC, NULL, 0, "", HFILL
    }
  },

  { &hf_dbglog_eid, 
    { "EID", "dl.eid",
      FT_UINT8, BASE_DEC, NULL, 0, "", HFILL
    }
  },

  { &hf_dbglog_mark, 
    { "Mark", "dl.mark",
      FT_UINT8, BASE_HEX, NULL, 0, "", HFILL
    }
  },

  { &hf_dbglog_datalen, 
    { "Data length", "dl.datalen",
      FT_UINT8, BASE_DEC, NULL, 0, "", HFILL
    }
  },
  { &hf_dbglog_fid, 
    { "File ID", "dl.fid",
      FT_UINT8, BASE_DEC, NULL, 0, "", HFILL
    }
  },
  { &hf_dbglog_lid, 
    { "Line number", "dl.lid",
      FT_UINT16, BASE_DEC, NULL, 0, "", HFILL
    }
  },

  { &hf_dbglog_msg, 
    { "Log message", "dl.log_msg",
      FT_NONE, BASE_NONE, NULL, 0x0, "", HFILL
    }
  }

};
# endif 

static void
decorate_msghdr(tvbuff_t *tvb, proto_tree *tree, proto_item  *item){

  int offset;

  /* Decorate MSGHDR item */
  if (!tree) return;

  offset = 0;

  /* Decorate event info  */
  proto_tree_add_item(tree, hf_dbglog_version  , tvb, offset + 0, sizeof(guint8), dbglog_hdr.encoding);
  proto_tree_add_item(tree, hf_dbglog_flags    , tvb, offset + 1, sizeof(guint8), dbglog_hdr.encoding);
  proto_tree_add_item(tree, hf_dbglog_mark     , tvb, offset + 2, sizeof(guint8), dbglog_hdr.encoding);
  proto_tree_add_item(tree, hf_dbglog_eid      , tvb, offset + 3, sizeof(guint8), dbglog_hdr.encoding);
  proto_tree_add_item(tree, hf_dbglog_fid      , tvb, offset + 4, sizeof(guint16), dbglog_hdr.encoding);
  proto_tree_add_item(tree, hf_dbglog_lid      , tvb, offset + 6, sizeof(guint16), dbglog_hdr.encoding);
  proto_tree_add_item(tree, hf_dbglog_datalen  , tvb, offset + 8, sizeof(guint16), dbglog_hdr.encoding);

  /*
  proto_item_set_text(item_info, "Endianess: %s", 
    val_to_str(dbglog_hdr.encoding, endianess_internal, "Unknown(%u)"));
  */

  /* Decorate tree root */
  proto_item_set_text(item, "DBGLOG header. EID:%s, FID:%s, LID:%d, Len:%d", 
    dbglog_hdr.eid_name,
    dbglog_hdr.fid_name,
    dbglog_hdr.lid,
    dbglog_hdr.data_len);
}

static gint
dissect_msghdr(tvbuff_t *tvb){

  gint    bytes_read;
  
  /* Endian agnostic part */
  dbglog_hdr.ver       = tvb_get_guint8(tvb, 0);
  dbglog_hdr.flags     = tvb_get_guint8(tvb, 1);
  dbglog_hdr.mark      = tvb_get_guint8(tvb, 2);
  dbglog_hdr.eid       = tvb_get_guint8(tvb, 3);
  
  dbglog_hdr.encoding = GET_BITS(dbglog_hdr.flags,  0, 1);

  if (dbglog_hdr.encoding){
    /* Reversed */
    dbglog_hdr.fid       = tvb_get_letohs(tvb, 4);
    dbglog_hdr.lid       = tvb_get_letohs(tvb, 6);
    dbglog_hdr.data_len  = tvb_get_letohs(tvb, 8);
  } else {
    /* Native */
    dbglog_hdr.fid       = tvb_get_ntohs(tvb, 4);
    dbglog_hdr.lid       = tvb_get_ntohs(tvb, 6);
    dbglog_hdr.data_len  = tvb_get_ntohs(tvb, 8);
  }

  bytes_read = 10;

  dbglog_hdr.eid_name = scd_get_elfname(scd_info, dbglog_hdr.eid);
  dbglog_hdr.fid_name = scd_get_fidname(scd_info, dbglog_hdr.eid, dbglog_hdr.fid);

  return bytes_read;
}

static const gchar* 
get_padding(const gchar* str){
    
  gint  n, i;
  const gchar* padding;

  padding = &white_space_str[TAB_STOP_LAST];
  n = (gint)strlen(str);
  for(i = 0; i < array_length(field_name_tab_stop); i++){
    if ( n < field_name_tab_stop[i]){
      padding -= field_name_tab_stop[i] - n;
      break;
    }
  }
  return padding;
}

static gchar*
find_next_fmt(gchar *pos){

  while(pos){
    pos = strchr(pos, '%');
    if (pos == NULL) break;
    if (pos[1] != '%') break;
    pos += 2;
  }
  return pos;
}

static gint
process_nonstd_fmt(tvbuff_t *tvb, int *p_offset, char **p_log_msg_pos, gint log_msg_rem, guint8 payload_type, gchar **p_scd_msg){

/* TODO: check g_snprintf return value for WIN32 and GNUC */

  int offset;
  char *out_pos, *scd_msg_pos, *delim_pos;
  int res;
  gint out_bw;
  guint tvb_remaining;

  offset = *p_offset;
  out_pos = *p_log_msg_pos;
  scd_msg_pos = *p_scd_msg + 1; /* +1 to skip '%' */
  out_bw = 0;

  res = tvb_length_remaining(tvb, offset);
  if (res > 0) tvb_remaining = (guint)res;
  else return -1;

  /* Process non standard formats */
  res = RESULT_OK;
  switch(*scd_msg_pos){
    case 'Y' : 
      {  /* MAC DD:DD:DD:DD:DD:DD */ 
        guint8 mac[6];
        if (payload_type != DBGLOG_HDR_PTYPE_MSG_MAC){ res = RESULT_FAIL; break; }
        if (tvb_remaining < sizeof(mac)){ res = RESULT_FAIL; break; }

        tvb_memcpy(tvb, mac, offset, sizeof(mac)); /* TODO: Check byte order from FW & DRV */
        offset += sizeof(mac);
        out_bw += g_snprintf(out_pos, log_msg_rem, "%02X:%02X:%02X:%02X:%02X:%02X", 
          mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        break;
      }

    case 'B' :
      { /* IP4 DD.DD.DD.DD */
        guint8 ipv4[4];
        if (payload_type != DBGLOG_HDR_PTYPE_MSG_INT32){ res = RESULT_FAIL; break; }
        if (tvb_remaining < sizeof(ipv4)){ res = RESULT_FAIL; break; }

        tvb_memcpy(tvb, ipv4, offset, sizeof(ipv4)); /* TODO: Check byte order from FW & DRV */
        offset += sizeof(ipv4);
        out_bw += g_snprintf(out_pos, log_msg_rem, "%d.%d.%d.%d", 
          ipv4[0], ipv4[1], ipv4[2], ipv4[3]);
        break;
      }

    case 'K' : 
      { /* IPv6 DDDD:DDDD:DDDD:DDDD:DDDD:DDDD:DDDD:DDDD */
        guint16 ipv6[8];
        if (payload_type != DBGLOG_HDR_PTYPE_MSG_IPV6){ res = RESULT_FAIL; break; }
        if (tvb_remaining < sizeof(ipv6)){ res = RESULT_FAIL; break; }

        tvb_memcpy(tvb, ipv6, offset, sizeof(ipv6)); 
        offset += sizeof(ipv6);
        out_bw += g_snprintf(out_pos, log_msg_rem, "%04X:%04X:%04X:%04X:%04X:%04X:%04X:%04X", 
          ipv6[0], ipv6[1], ipv6[2], ipv6[3], ipv6[4], ipv6[5], ipv6[6], ipv6[7]);
        break;
      }
    case 'H' : 
      { /* uint64 0xDDDDDDDDDDDDDDDD*/
        guint32 longh[2];
        if (payload_type != DBGLOG_HDR_PTYPE_MSG_INT64){ res = RESULT_FAIL; break; }
        if (tvb_remaining < sizeof(longh)){ res = RESULT_FAIL; break; }
        tvb_memcpy(tvb, longh, offset, sizeof(longh)); /* TODO: Check byte order from FW & DRV */
        offset += sizeof(longh);
        out_bw += g_snprintf(out_pos, log_msg_rem, "0x%08X%08X", 
          longh[0], longh[1]);
        break;
      }
  };

  if (res == RESULT_FAIL){
    return -1;  /* Error - terminate packet processing */
  }

  if (out_bw == 0){
    return 0;   /* nonstd format not recognized - continue processing */
  }

  /* Append SCD message remainder till next '%'*/
  out_pos += out_bw;
  scd_msg_pos ++;

  delim_pos = find_next_fmt(scd_msg_pos);

  /* Truncate SCD till next '%' */
  if (delim_pos){ *delim_pos = 0; }

  out_bw += g_snprintf(out_pos, log_msg_rem - out_bw, "%s", scd_msg_pos);

  /* Restore truncated string */
  if (delim_pos){ *delim_pos = '%'; }

  *p_offset = offset;
  *p_log_msg_pos += out_bw;
  *p_scd_msg = delim_pos; /* return NULL if SCD_MSG fully processed */

  return out_bw;
}      

static gint
process_std_fmt(tvbuff_t *tvb, gint *p_offset, gchar **p_log_msg_pos, gint log_msg_rem, guint8 payload_type, gchar **scd_msg){

/* TODO: Check byte order from FW & DRV */
/* TODO: check g_snprintf return value for WIN32 and GNUC */

  gint res;
  gchar *out_pos, *scd_msg_pos, *delim_pos;
  gint  offset, out_bw;
  guint tvb_remaining;

  guint   payload_len;
  guint8  payload_uint8;
  guint32 payload_uint32;
  
  offset = *p_offset;
  out_pos = *p_log_msg_pos;
  scd_msg_pos = *scd_msg;
  out_bw = 0;
  
  res = tvb_length_remaining(tvb, offset);
  if (res > 0) tvb_remaining = (guint)res;
  else return -1;

  /* Append SCD message remainder till next '%'*/
  delim_pos = find_next_fmt(scd_msg_pos+1);

  /* Truncate SCD till next '%' */
  if (delim_pos){ *delim_pos = 0; }

  res = RESULT_OK;
  switch(payload_type){
    case DBGLOG_HDR_PTYPE_MSG_STR   :
      { guint8  *payload_str = NULL;

        /* Get string length */
        if (tvb_remaining < sizeof(guint16)){ res = RESULT_FAIL; break;}
        payload_len = dbglog_hdr.encoding ? tvb_get_letohs(tvb, offset) : tvb_get_ntohs(tvb, offset);

        /* Get string itself*/
        if (tvb_remaining < payload_len + 2){ res = RESULT_FAIL; break;}
        payload_str = tvb_get_string(wmem_packet_scope(), tvb, offset + 2, payload_len);
        payload_len += 2;
        out_bw = g_snprintf(out_pos, (gulong)log_msg_rem, scd_msg_pos, payload_str);
        g_free(payload_str);
        break;
      }
    case DBGLOG_HDR_PTYPE_MSG_INT8  :
      if (tvb_remaining < sizeof(guint8)){ res = RESULT_FAIL; break;}
      payload_uint8 = tvb_get_guint8(tvb, offset);
      payload_len = 1;
      out_bw = g_snprintf(out_pos, (gulong)log_msg_rem, scd_msg_pos, payload_uint8);
      break;
    case DBGLOG_HDR_PTYPE_MSG_INT32 :
      if (tvb_remaining < sizeof(guint32)){ res = RESULT_FAIL; break;}
      payload_uint32 = dbglog_hdr.encoding ? tvb_get_letohl(tvb, offset) : tvb_get_ntohl(tvb, offset);
      payload_len = 4;
      out_bw = g_snprintf(out_pos, (gulong)log_msg_rem, scd_msg_pos, payload_uint32);
      break;
    case DBGLOG_HDR_PTYPE_MSG_INT64 :
    case DBGLOG_HDR_PTYPE_MSG_MAC   :
    case DBGLOG_HDR_PTYPE_MSG_IPV6  :
      res = RESULT_FAIL;
      break;
  }

  /* Restore truncated string */
  if (delim_pos){ *delim_pos = '%'; }

  if (res == RESULT_FAIL){
    return -1;  /* Error - terminate packet processing */
  }

  if (out_bw < 0){
    out_bw = g_snprintf(out_pos, (gulong)log_msg_rem, "%%invalid qualifier%%");  
    return -1; 
  }

  offset += payload_len;  /* advance offset in case of recognized format qualifier */

  *p_offset = offset;
  *p_log_msg_pos += out_bw;
  *scd_msg = delim_pos; /* return NULL if SCD_MSG fully processed */

  return out_bw; /* if 0 std format not recognized - continue processing */
}      

#if 0
static int
dissect_log_msg(tvbuff_t *tvb, gint prev_offset, const char *scd_msg, proto_tree *tree, proto_item  *item){

  gint chars_written;
  gint offset;
  gint log_msg_rem;
 
  gchar *log_msg;

  gchar *scd_msg_pos;
  gchar *log_msg_pos;
  gchar *delim_pos;

  log_msg = g_malloc(SCD_LINE_SIZE);
  log_msg[0] = 0;

  offset = prev_offset;
  scd_msg_pos = scd_msg;
  log_msg_pos = log_msg;
  log_msg_rem = SCD_LINE_SIZE;

  /* Process SCD MSG till first '%' */
  delim_pos = find_next_fmt(scd_msg_pos);

  if (delim_pos){ *delim_pos = 0; }     /* Truncate on first %'*/
  chars_written = g_snprintf(log_msg_pos, (gulong)log_msg_rem, "%s", scd_msg_pos);
  if (chars_written < 0){
    chars_written = g_snprintf(log_msg_pos, (gulong)log_msg_rem, "%%invalid qualifier%%");  
  }
  log_msg_pos += chars_written;
  if (delim_pos){ *delim_pos = '%'; }   /* Restore truncated string */
  
  /* Process payload fields if exist */
  while(delim_pos){
    guint8 payload_type;

    log_msg_rem -= chars_written;

    if (tvb_length_remaining(tvb, offset)){
      payload_type = tvb_get_guint8(tvb, offset);
      offset += 1;
    }else{
      break;
    }

    /* Process SCD string starting from '%' */
    chars_written = process_nonstd_fmt(tvb, &offset, &log_msg_pos, log_msg_rem, payload_type, &delim_pos);
    if (chars_written == -1) break;
    if (chars_written > 0) continue;

    /* (res == 0) -> non std format not found */

    chars_written = process_std_fmt(tvb, &offset, &log_msg_pos, log_msg_rem, payload_type, &delim_pos);
    if (chars_written == -1) break;
    if (chars_written > 0) continue;

    /* (chars_written == 0) -> format not recognized or null string */
  }

  /* Set LOG message */
  proto_item_set_text(item, "LOG MSG: %s", log_msg);

  g_free(log_msg);

  return -1;
}

#endif

static void 
decorate_cols_by_scd_log_msg(packet_info *pinfo){

  col_set_str(pinfo->cinfo, COL_PROTOCOL, "dbglog");
  col_add_str(pinfo->cinfo, COL_INFO, scd_log_msg.info_str);

  return;
}

static void 
decorate_cols_by_msghdr(packet_info *pinfo){

  col_set_str(pinfo->cinfo, COL_PROTOCOL, "dbglog");
  col_add_str(pinfo->cinfo, COL_INFO, "Unknown message. EVT not found in SCD file");
  
  return;
}


typedef void (dies_itt_t)(dies_info_t*, dies_memb_t *dies, gint idx);

static gint
dies_iterrate_fields_by_die(dies_info_t  *dies_info, dies_memb_t *dies, gint idx, dies_itt_t *dies_itt) {

  if (dies_info->max_idx < dies_info->lvl_idx)
    dies_info->max_idx = dies_info->lvl_idx;

  while(1){
    gint is_last = dies[idx].flag & DIES_MEMB_FLAG_IS_LAST;


    if ( dies[idx].link_id != -1) {

        dies_info->lvl_idx ++;
        dies_itt(dies_info, dies, idx);

        dies[dies[idx].link_id].flag |= DIES_MEMB_FLAG_IS_REFER;
        dies_iterrate_fields_by_die(dies_info, dies, dies[idx].link_id, dies_itt);      /* Recursive call */
        idx++;

        dies_info->lvl_idx--;
    }
    else {
        gint is_refer = dies[idx].flag & DIES_MEMB_FLAG_IS_REFER;

        if (is_refer) {
            // Note: linked entry does not affect on call-depth level
            // Clear "is referenced" flag
            dies[idx].flag &= ~DIES_MEMB_FLAG_IS_REFER;
        }
        else {
            dies_info->lvl_idx ++;
            dies_itt(dies_info, dies, idx);
        }

        if (dies[idx].flag & DIES_MEMB_FLAG_IS_PARENT) {
            idx = dies_iterrate_fields_by_die(dies_info, dies, idx+1, dies_itt);      /* Recursive call */
        }

        if (is_refer) {
          break;
        }

        dies_info->lvl_idx--;
    }

    /* Proceed with next member on that level */
    if (is_last) {
      break;
    }
    
  } /* while(1)*/


  return idx;

}

void
dissect_dies_itt(dies_info_t  *dies_info, dies_memb_t *dies, gint idx) {

    dies_memb_t         *dies_memb;

    proto_tree  *tree_dies_memb;
    proto_item  *ti_dies_memb;

    tvbuff_t    *tvb            = dissect_dies_itt_info.tvb;
    gint        offset          = dissect_dies_itt_info.offset_lvl[dies_info->lvl_idx-1];
    proto_tree  *parent_tree    = dissect_dies_itt_info.parent_tree[dies_info->lvl_idx-1];
    gint        hf_id_idx       = dissect_dies_itt_info.hf_id_idx;

    dissect_dies_itt_info.hf_id_idx++;

    dies_memb = &dies[idx];

    offset +=  dies_memb->offset;

    // Add item 
    if (tvb_length(tvb) < (offset + dies_memb->size)){
      offset  = offset;     /* just place holder for break point */
      return;               /* some thing wrong. PC must not hit here */
    }

    ti_dies_memb = proto_tree_add_item(
      parent_tree,                      /* Parent tree */
      dies_info->hf_ids[hf_id_idx] ,    /* Item ID */
      tvb,                              /* Packet Buffer */
      offset,                           /* Start */
      dies_memb->size,                  /* Length */
      dbglog_hdr.encoding);             /* Encoding */

    proto_item_prepend_text(ti_dies_memb, "%s%s", 
      dies_memb->type_name, 
      get_padding(dies_memb->type_name));

    // If the DIE is linked, then get all required data from linked one.
    if (dies_memb->link_id != -1) {
        dies_memb = &dies[dies_memb->link_id];
    }

    if (dies_memb->flag & DIES_MEMB_FLAG_IS_PARENT) {
      dissect_dies_itt_info.offset_lvl[dies_info->lvl_idx] = offset;

      tree_dies_memb = proto_item_add_subtree(ti_dies_memb, dies_memb->ett_id);
      dissect_dies_itt_info.parent_tree[dies_info->lvl_idx] = tree_dies_memb;
    }
 
}

static void
dissect_log_msg(tvbuff_t *tvb, gint offset, scd_entry_t *scd, proto_tree *parent_tree, proto_item  *item){

    GSList  *list_entry;
    dies_pool_t *dies_pool;

    dies_pool = (dies_pool_t *)g_hash_table_lookup(dies_info.dies_pools_hash, (gpointer)&scd->eid);
    if (dies_pool->size == 0){
        return;
    }

    list_entry = g_slist_nth(scd->dies_list, 0);
    while (list_entry){
        dies_memb_t *scd_die;
        int die_id;

        die_id = (int)g_slist_nth_data(list_entry, 0);
        scd_die = &dies_pool->dies[die_id];

        dissect_dies_itt_info.tvb = tvb;

        memset(dissect_dies_itt_info.parent_tree, 0xFF, sizeof(dissect_dies_itt_info.parent_tree));
        dissect_dies_itt_info.parent_tree[0] = parent_tree;

        memset(dissect_dies_itt_info.offset_lvl, 0xFF, sizeof(dissect_dies_itt_info.offset_lvl));
        dissect_dies_itt_info.offset_lvl[0] = offset;

        dissect_dies_itt_info.hf_id_idx = scd_die->scd_hf_idx;
        scd_die->flag |= DIES_MEMB_FLAG_IS_LAST;

        // Traverse SCD dies 
        dies_info.lvl_idx = 0;
        dies_iterrate_fields_by_die(&dies_info, dies_pool->dies, scd_die->self_id, dissect_dies_itt);
            
        offset += (scd_die->size);

        list_entry = g_slist_next(list_entry);
    }


}

static int
dissect_dbglog(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, void *data)
{
  proto_tree    *tree_msghdr  = NULL;
  proto_item    *ti_msghdr    = NULL;

  gint          msghdr_len;

  scd_entry_t   *scd_evt;

  gint          temp;

  msghdr_len = dissect_msghdr(tvb);

  if (tree) {
    ti_msghdr   = proto_tree_add_item(tree, proto_dbglog, tvb, 0, msghdr_len, FALSE);
    tree_msghdr = proto_item_add_subtree(ti_msghdr, ett_msghdr);
    decorate_msghdr(tvb, tree_msghdr, ti_msghdr);
  }

  /* Try to find SCD message by E,F,L */
  scd_evt = scd_get_by_ids(scd_info, 
      dbglog_hdr.eid,
      dbglog_hdr.fid,
      dbglog_hdr.lid);

  if (NULL == scd_evt){
    decorate_cols_by_msghdr(pinfo);
    return tvb_length(tvb);
  }

  switch (scd_evt->tag){

    case SCD_ENTRY_TAG_MSG:
      if (tree) {
        proto_item  *ti_log_msg   = NULL;
        proto_tree  *tree_log_msg = NULL;
        gint item_offset;

        /* Set item offset to 0 for empty packets. Set offset to payload otherwise; */
        item_offset = tvb_length_remaining(tvb, msghdr_len) ? msghdr_len : 0;

        /* Add item without payload */
        ti_log_msg = proto_tree_add_item(tree, hf_dbglog_msg, tvb, item_offset, -1, FALSE);
        
        proto_item_set_text(ti_log_msg, "MSG: %s", scd_evt->str);

        tree_log_msg = proto_item_add_subtree(ti_log_msg, ett_dbglog_msg);

        dissect_log_msg(tvb, item_offset, scd_evt, tree_log_msg, ti_log_msg);

      }

      /* Generate formated INFO string */
      strncpy(scd_log_msg.info_str, scd_evt->str, sizeof(scd_log_msg.info_str));

      decorate_cols_by_scd_log_msg(pinfo);
      break;
    default:
      /* Payload unknown */
      decorate_cols_by_msghdr(pinfo);
      break;
  }

  temp = tvb_length(tvb);
  return temp;
}

void
dies_iterrate_fields(dies_info_t  *dies_info,  dies_itt_t *dies_itt){

  dies_pool_t *dies_pool;
  GHashTableIter iter;

  g_hash_table_iter_init(&iter, dies_info->dies_pools_hash);
  while (g_hash_table_iter_next(&iter, NULL, (gpointer *)&dies_pool))
  {
    if (dies_pool->size == 0){
      continue;
    }
    dies_info->lvl_idx = 0;
    dies_iterrate_fields_by_die(dies_info, dies_pool->dies, 0, dies_itt);
  }

}

void
calc_fields_num_itt(dies_info_t  *dies_info, dies_memb_t *dies, gint idx) {

    UNREFERENCED_PARAM(dies);
    UNREFERENCED_PARAM(idx);

    dies_info->fields_num++;
}

void
calc_fields_num(dies_info_t  *dies_info) {

    /* Calcs number of fields, store result in dies_info */
    dies_info->fields_num = 0;
    dies_info->max_idx = 0;

    dies_iterrate_fields(dies_info,  calc_fields_num_itt);
}

void
fillup_reg_info_itt(dies_info_t  *dies_info, dies_memb_t *dies, gint idx) {
 
    gchar               *abbr_name;
    gchar               *abbr_str;
    size_t              abbr_str_len, abbr_node_len;
    hf_register_info    *hf_curr;
    dies_memb_t         *dies_memb;


    if (dies[idx].flag & DIES_MEMB_FLAG_SCD_ROOT) {
        dies[idx].scd_hf_idx = dies_info->hf_idx;
    }
  
    dies_memb = &dies[idx];

    static hf_register_info hf_blank = 
        { NULL,
        { NULL, NULL,
            FT_NONE, BASE_NONE,
            NULL, 0x0,
            NULL, HFILL }};

    /* Use type name for root entries */
    if (dies_memb->field_name && strlen(dies_memb->field_name))
    {
        abbr_name =  dies_memb->field_name;
    }
    else
    {
        abbr_name = dies_memb->type_name;
    }

    /* Compose full abbrev */
    abbr_node_len = dies_info->abbr_lvl[dies_info->lvl_idx-1];
    dies_info->abbr[abbr_node_len] = 0;     /* Restore abbreviation */

    abbr_str_len = abbr_node_len + 1 + strlen(abbr_name) + 1; /* xxx.yyy/0 */
    abbr_str = g_malloc(abbr_str_len);

    dies_info->abbr_lvl[dies_info->lvl_idx] = abbr_str_len;
    
    g_snprintf(abbr_str, (gulong)abbr_str_len, "%s.%s", dies_info->abbr, abbr_name);

    /* Init with blank template*/
    dies_info->hf_ids[dies_info->hf_idx] = -1;

    hf_curr = &dies_info->hf_dies[dies_info->hf_idx];
    memcpy(hf_curr, &hf_blank, sizeof(hf_register_info));

    hf_curr->p_id = &dies_info->hf_ids[dies_info->hf_idx];
    hf_curr->hfinfo.name  = abbr_name;
    hf_curr->hfinfo.abbrev = abbr_str;
    hf_curr->hfinfo.blurb = abbr_name;


    dies_info->hf_idx++;

    // If the DIE is linked, then get all required data from linked one.
    if (dies_memb->link_id != -1) {
        dies_memb = &dies[dies_memb->link_id];
    }

    hf_curr->hfinfo.display = dies_memb->display;
    hf_curr->hfinfo.type = dies_memb->format;    
    hf_curr->hfinfo.strings = dies_memb->strings;

    if (dies_memb->flag & DIES_MEMB_FLAG_IS_PARENT) {
      strcpy(dies_info->abbr, abbr_str);     /* Prepare abbreviation for next level */
    }
 
}

static void
fillup_reg_info(dies_info_t *dies_info){

  dies_info->hf_idx = 0;

  strcpy(dies_info->abbr, "dl");

  memset(dies_info->abbr_lvl, -1, sizeof(dies_info->abbr_lvl));
  dies_info->abbr_lvl[0] = strlen(dies_info->abbr);

  dies_iterrate_fields(dies_info,  fillup_reg_info_itt);
}

static dies_memb_t *
calc_trees_by_die(dies_info_t  *dies_info, dies_memb_t *dies_memb){

  while(1){
    gint is_last;

    /* Skip SCD root entries as they always linked with regular ones */
    if (dies_memb->flag & DIES_MEMB_FLAG_SCD_ROOT)
    {
        dies_memb++;
        continue;
    }

    is_last = dies_memb->flag & DIES_MEMB_FLAG_IS_LAST;

    if (dies_memb->flag & DIES_MEMB_FLAG_IS_PARENT) {

      /* Struct or Array */
      dies_info->trees_num++;

      /* Proceed with 1st member of next level */
      dies_memb++;
      dies_memb = calc_trees_by_die(dies_info, dies_memb);      /* Recursive call */
    }

    /* Proceed with next member on that level */
    if (is_last){
      break;
    }
    
    dies_memb++;

  } /* while(1)*/

  return dies_memb;

}

void
calc_trees_num(dies_info_t  *dies_info){

  dies_pool_t *dies_pool;
  GHashTableIter iter;

  /* Calcs number of trees and fields, store result in dies_info */
  /* Number of trees is number of ALL roots + number of structs and arrays in pool */
  /* Number of fields is number of nonSTRUCT and nonARRAY members in pool */

  dies_info->trees_num = 0;

  g_hash_table_iter_init(&iter, dies_info->dies_pools_hash);
  while (g_hash_table_iter_next(&iter, NULL, (gpointer *)&dies_pool))
  {
    if (dies_pool->size == 0){
      continue;
    }
    calc_trees_by_die(dies_info, dies_pool->dies);

  }

}

static dies_memb_t *
fillup_tree_reg_info_by_die(gint **ett, dies_info_t *dies_info, dies_memb_t *dies_memb){

  while(1){
    gint is_last;

    /* Skip SCD root entries as they always linked with regular ones */
    if (dies_memb->flag & DIES_MEMB_FLAG_SCD_ROOT)
    {
        dies_memb++;
        continue;
    }

    is_last = dies_memb->flag & DIES_MEMB_FLAG_IS_LAST;
    
    if (dies_memb->flag & DIES_MEMB_FLAG_IS_PARENT) {

      /* Structure or Array */
      ett[dies_info->ett_idx++] = &dies_memb->ett_id;

      /* Proceed with 1st member of next level */
      dies_memb++;

      dies_memb = fillup_tree_reg_info_by_die(ett, dies_info, dies_memb);   /* Recursive call */
    }

    /* Proceed with next member on that level */
    if (is_last){
      break;
    }
    
    dies_memb++;

  } /* while(1)*/

  return dies_memb;
}

void
fillup_tree_reg_info (dies_info_t  *dies_info){

  dies_pool_t *dies_pool;
  GHashTableIter iter;

  dies_info->ett_idx = 0;

  g_hash_table_iter_init(&iter, dies_info->dies_pools_hash);
  while (g_hash_table_iter_next(&iter, NULL, (gpointer *)&dies_pool))
  {
    if (dies_pool->size == 0){
      continue;
    }

    fillup_tree_reg_info_by_die(dies_info->ett_dies, dies_info, dies_pool->dies);
  }


}

void
proto_register_dbglog(void)
{
    gint      res;

    res = RESULT_OK;

    proto_dbglog = proto_register_protocol(
        "DBG Log Viewer",           /* Name */
        "DBGLog",                   /* Short Name */
        "dl");                      /* Abbrev */

    /* Register packet processing function */
    new_register_dissector("dbglog", dissect_dbglog, proto_dbglog);

    /* Fixed Part */
    proto_register_field_array(proto_dbglog, log_event_hf, array_length(log_event_hf));
    proto_register_subtree_array(&ett_dbglog[0], array_length(ett_dbglog));

    res = scd_dwarf_parse(
        scd_filename,                 /* in - SCD file name */
        &scd_info,                    /* out - SCD info  */
        &dies_info.dies_pools_hash);  /* out - DIES pool */

    if (res == RESULT_FAIL) {
        goto end_of_proto_register_dbglog;
    }


    {
        FILE *scd_dwarf_dbg;
        scd_dwarf_dbg = fopen("scd_dwarf_dbg.txt", "w");
        print_ids(scd_dwarf_dbg, scd_info->scd_table, scd_info->scd_table_size);

        /* Printout DIEs pool */
        dies_pool_print(scd_dwarf_dbg, dies_info.dies_pools_hash);

        fclose(scd_dwarf_dbg);
    }

    /* Compose fields and trees arrays by DIES pool */
    calc_fields_num(&dies_info);
    calc_trees_num(&dies_info);

    dies_info.hf_ids = g_malloc_n(dies_info.fields_num, sizeof(int*));
    dies_info.hf_dies = g_malloc_n(dies_info.fields_num, sizeof(hf_register_info));
    dies_info.ett_dies = g_malloc_n(dies_info.trees_num, sizeof(gint*));

    fillup_reg_info(&dies_info);
    fillup_tree_reg_info(&dies_info);

    /* Register fields and trees. DIES depended part */
    proto_register_field_array(proto_dbglog, dies_info.hf_dies, dies_info.fields_num);
    proto_register_subtree_array(dies_info.ett_dies, dies_info.trees_num);

end_of_proto_register_dbglog:

    return;
}

void
proto_reg_handoff_dbglog(void)
{

    static gboolean initialized = FALSE;
    static dissector_handle_t dbglog_handle;

    if (!initialized) {

        /*  Use new_create_dissector_handle() to indicate that dissect_dbglog()
         *  returns the number of bytes it dissected (or 0 if it thinks the packet
         *  does not belong to PROTONAME).
         */
        dbglog_handle = new_create_dissector_handle(dissect_dbglog, proto_dbglog);
        initialized = TRUE;
        dissector_add_uint("wtap_encap", WTAP_ENCAP_USER1, dbglog_handle);

    }
    else {

        /*
          If you perform registration functions which are dependent upon
          prefs the you should de-register everything which was associated
          with the previous settings and re-register using the new prefs
          settings here. In general this means you need to keep track of
          the PROTOABBREV_handle and the value the preference had at the time
          you registered.  The PROTOABBREV_handle value and the value of the
          preference can be saved using local statics in this
          function (proto_reg_handoff).
          */
    }
}
