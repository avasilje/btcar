/* Application reads DBGLOG packets from input file and 
       a) write to file in USER pcap format
       b) send it wireshark encapsulated in USER protocol via pipe

*/

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "ref_host.h"

guint32 curr_ts = 0;

FILE *in_file;
FILE *out_file;

FILE *dbg_file;

#define WTAP_ENCAP_USER1                         46
pcap_hdr_t  pcap_hdr = {
    0xa1b2c3d4,        /* magic number */
    2,4,               /* major, minor version number */
    0,                 /* GMT to local correction */
    0,                 /* accuracy of timestamps */
    4096,              /* max length of captured packets, in octets */
    148   /* data link type */
};

dbglog_pcap_pck_t   dbglog_pcap_pck;
dbglog_target_hdr_v0_t  dbglog_target_hdr_v0;

int main(int argc, char* argv[])
{
    size_t b_read, b_written, payload_len;

    if (argc < 2)
    {
        printf("Please specify input file.\n");
        exit(EXIT_FAILURE);
    }

    in_file = fopen(argv[1], "rb");
    if (NULL == in_file)
    {
        printf("Can't open input file: %s.\n", argv[1]);
        exit(EXIT_FAILURE);
    }

    dbg_file = fopen("dbg.txt", "w+");

    out_file = fopen("out.pcap", "wb+");
    if (NULL == out_file)
    {
        printf("Can't open output file: %s.\n", "out.pcap");
        exit(EXIT_FAILURE);
    }


    /* Initialize global PCAP header */
    fwrite(&pcap_hdr, sizeof(pcap_hdr_t), 1, out_file);

    // Read from input file dbglog entry
    while ( 1)
    {
        b_read = fread( &dbglog_target_hdr_v0, 
                        sizeof(dbglog_target_hdr_v0_t),
                        1,
                        in_file);
        if (b_read != 1)
        {
            if (feof(in_file))
            {
                printf("End of file reached\n");
            }

            if (ferror(in_file))
            {
                printf("Error reading from file\n");
            }

            break;
        }
    
        fprintf(dbg_file, "mark(8)=%02X, pck_len(8)=%d, fid(8)=%d, lid(16)=%d\n",
            dbglog_target_hdr_v0.mark,
            dbglog_target_hdr_v0.pck_len,
            dbglog_target_hdr_v0.fid,
            dbglog_target_hdr_v0.lid);

        payload_len = dbglog_target_hdr_v0.pck_len - sizeof(dbglog_target_hdr_v0_t);
        memset(&dbglog_pcap_pck.data, 0xCC, sizeof(dbglog_pcap_pck.data));

        // Read payload data from input file to output pcap packet
        {
            b_read = fread( &dbglog_pcap_pck.data, 
                           1,
                           payload_len,
                           in_file);
            if (b_read != payload_len)
            {
                if (feof(in_file))
                {
                    printf("End of file reached unexpectedely\n");
                }

                if (ferror(in_file))
                {
                    printf("Error reading data from file\n");
                }

                break;
            }
        }

        // Convert from target format to host format
        {
            dbglog_host_hdr_t   *dbglog_ptr;

            dbglog_ptr = &dbglog_pcap_pck.dbglog_hdr;
            dbglog_ptr->ver = 0;
            dbglog_ptr->flag = 1;
            dbglog_ptr->eid = 1;

            dbglog_ptr->mark     = dbglog_target_hdr_v0.mark;
            dbglog_ptr->data_len = payload_len;
            dbglog_ptr->fid      = dbglog_target_hdr_v0.fid;
            dbglog_ptr->lid      = dbglog_target_hdr_v0.lid;
        }

        // Init pcap header
        {
            pcaprec_hdr_t      *pcap_ptr;
            pcap_ptr = &dbglog_pcap_pck.pcap_hdr;
            pcap_ptr->ts_sec = curr_ts++;
            pcap_ptr->ts_usec = 0;
            pcap_ptr->incl_len = payload_len + sizeof(dbglog_host_hdr_t);
            pcap_ptr->orig_len = pcap_ptr->incl_len;
        }

        // write dbglog pcap packet to output file
        b_written = fwrite(&dbglog_pcap_pck, dbglog_pcap_pck.pcap_hdr.incl_len + sizeof(pcaprec_hdr_t), 1, out_file);
        if (b_written != 1)
        {
            printf("Error writing to out file\n");
            break;
        }

    }

    if (in_file) fclose(in_file);
    if (out_file) fclose(out_file);
    if (dbg_file) fclose(dbg_file);

}

