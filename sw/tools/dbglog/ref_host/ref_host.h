/* Application reads DBGLOG packets from input file and 
       a) write to file in USER pcap format
       b) send it wireshark encapsulated in USER protocol via pipe

*/

/* glibconfig.h */
typedef signed char gint8;
typedef unsigned char guint8;
typedef signed short gint16;
typedef unsigned short guint16;
typedef signed int gint32;
typedef unsigned int guint32;


/*
Global Header 	
 Packet Header 	
 Packet Data 	
 Packet Header 	
 Packet Data 	
 Packet Header 	
 Packet Data 	
 ...
*/

 /*
  This header starts the libpcap file and will be followed by the first packet header: 
 */
 #pragma pack(push,1)

typedef struct pcap_hdr_s {
    guint32 magic_number;   /* magic number */
    guint16 version_major;  /* major version number */
    guint16 version_minor;  /* minor version number */
    gint32  thiszone;       /* GMT to local correction */
    guint32 sigfigs;        /* accuracy of timestamps */
    guint32 snaplen;        /* max length of captured packets, in octets */
    guint32 network;        /* data link type */
} pcap_hdr_t;

/*
magic_number: used to detect the file format itself and the byte ordering. 
              The writing application writes 0xa1b2c3d4 with it's native 
              byte ordering format into this field. The reading application 
              will read either 0xa1b2c3d4 (identical) or 0xd4c3b2a1 (swapped). 
              If the reading application reads the swapped 0xd4c3b2a1 value,
              it knows that all the following fields will have to be swapped
              too.

version_major 
version_minor: the version number of this file format (current version is 2.4) 

thiszone: the correction time in seconds between GMT (UTC) and the local 
          timezone of the following packet header timestamps. 
          Examples: If the timestamps are in GMT (UTC), thiszone is simply 0. 
          If the timestamps are in Central European time (Amsterdam, Berlin, ...) 
          which is GMT + 1:00, thiszone must be -3600. 
          In practice, time stamps are always in GMT, so thiszone is always 0. 

sigfigs: in theory, the accuracy of time stamps in the capture; in practice,
         all tools set it to 0 

snaplen: the "snapshot length" for the capture (typically 65535 or even more, 
         but might be limited by the user), see: incl_len vs. orig_len below 

network: link-layer header type, specifying the type of headers at the beginning
         of the packet (e.g. 1 for Ethernet, see tcpdump.org's link-layer header
         types page for details); this can be various types such as 802.11, 802.11
          with various radio information, PPP, Token Ring, FDDI, etc. 

*/


/*
Record (Packet) Header
Each captured packet starts with (any byte alignment possible): 
*/


typedef struct pcaprec_hdr_s {
    guint32 ts_sec;         /* timestamp seconds */
    guint32 ts_usec;        /* timestamp microseconds */
    guint32 incl_len;       /* number of octets of packet saved in file */
    guint32 orig_len;       /* actual length of packet */
} pcaprec_hdr_t;

/*
ts_sec: the date and time when this packet was captured. This value is in seconds since January 1, 1970 00:00:00 GMT; this is also known as a UN*X time_t. You can use the ANSI C time() function from time.h to get this value, but you might use a more optimized way to get this timestamp value. If this timestamp isn't based on GMT (UTC), use thiszone from the global header for adjustments. 

ts_usec: in regular pcap files, the microseconds when this packet was captured, as an offset to ts_sec. In nanosecond-resolution files, this is, instead, the nanoseconds when the packet was captured, as an offset to ts_sec  Beware: this value shouldn't reach 1 second (in regular pcap files 1 000 000; in nanosecond-resolution files, 1 000 000 000); in this case ts_sec must be increased instead! 

incl_len: the number of bytes of packet data actually captured and saved in the file. This value should never become larger than orig_len or the snaplen value of the global header. 

orig_len: the length of the packet as it appeared on the network when it was captured. If incl_len and orig_len differ, the actually saved packet size was limited by snaplen.
*/

typedef struct dbglog_target_hdr_v0_s {
    guint8  mark;
    guint8  pck_len;
    guint8  fid;
    guint16 lid;
} dbglog_target_hdr_v0_t;

typedef struct dbglog_host_hdr_s {
    guint8  ver;
    guint8  flag;
    guint8  mark;
    guint8  eid;
    guint16 fid;
    guint16 lid;
    guint16 data_len;
} dbglog_host_hdr_t;

typedef struct dbglog_pcap_pck_s {

    pcaprec_hdr_t       pcap_hdr;
    dbglog_host_hdr_t   dbglog_hdr;
    uint8_t             data[4096];
} dbglog_pcap_pck_t;

#pragma pack(pop)
