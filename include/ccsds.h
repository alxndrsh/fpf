/*
FPF - Frame Processing Framework
See the file COPYING for copying permission.
*/
/*
ccsds.h - Generic definitions for CCSDS specific staff

History:
    created: 2015-11 - A.Shumilin.
*/

#ifndef CCSDS_H_INCLUDED
#define CCSDS_H_INCLUDED
#include "fpf.h"
using namespace std;

//// ------- CADU ---------
#define CCSDS_HEADERPOINTER_FILLER  0x7FE
#define CCSDS_HEADERPOINTER_NOHEADERS  0x7FF

struct PACKED_GCC SP
{
        t_uint8 x;
};

struct PACKED_GCC CCSDS_PACKET_SECHEADER_EOS
{

    t_uint16    time_days;
    t_uint32    time_msec;
    t_uint16    time_usec;
    t_uint8     ql_flag :1;
    t_uint8     packet_type :3 ;
    t_uint8     scan_count :3 ;
    t_uint8     mirror_side :1 ;
};

#define CCSDS_SP_HEADER_SIZE  6
#define CCSDS_SP_SECHEADER_EOS_SIZE  9
#define CCSDS_MAX_PACKET_COUNTER  16383
#define CCSDS_MAX_PACKET_SIZE  0xffff

struct PACKED_GCC  CCSDS_PACKET
{
    // primary header, 6 bytes
    t_uint8     version :3 ;
    t_uint8     type :1 ;
    t_uint8     sec_header_flag :1 ;
    t_uint16    apid :11;
    t_uint8     seq_flags :2 ;
    t_uint16    packet_count :14 ;
    t_uint16    packet_length :16 ;
    // secondary header or data
    union data
    {
        CCSDS_PACKET_SECHEADER_EOS sec_header;
        BYTE fisrt_data_byte;
    };
    //
};


static inline t_uint8  SP_GET_VER(BYTE* p) { return (t_uint8)( (p[0] & 0xE0U) >>5 );}
static inline t_uint8  SP_GET_SECHEADER(BYTE* p) { return (t_uint8)( (  p[0] & 0x10U) >>4 );}
static inline t_uint16  SP_GET_APID(BYTE* p) { return (t_uint16)( ( ( p[0] & 0x07U) << 8 ) | (p[1]) );}
static inline t_uint16  SP_GET_COUNT(BYTE* p) { return (t_uint16)( ( ( p[2] & 0x3FU) << 8 ) | (p[3]) );}
static inline t_uint8  SP_GET_SEGMENTATION(BYTE* p) { return (t_uint8)(  ( p[2] & 0xC0) >>6); }
static inline t_uint16  SP_GET_LENGTH(BYTE* p) { return (t_uint16)( p[4] << 8 | p[5] ); }

// EOS secondary header fields
static inline t_uint16  SP_GET_EOS_TS_DAYS(BYTE* p) { return (t_uint16)( p[6] << 8 | p[7] ); }
static inline t_uint32  SP_GET_EOS_TS_MSEC(BYTE* p) { return (t_uint32)( 0UL | (p[8] << 24)  | (p[9] << 16)  | (p[10] << 8)  | p[11] ); }
static inline t_uint16  SP_GET_EOS_TS_USEC(BYTE* p) {  return (t_uint16)( p[12] << 8 | p[13] );  }

#define CCSDS_CADU_ASM     0x1ACFFC1D
#define CCSDS_CADU_SIZE     1024
#define CCSDS_CADU_ASM_SIZE  4
#define CCSDS_MPDU_DATA_SIZE 884  /* without MPDU header */
#define CADU_DATA_OFFSET    12
#define CADU_RS_OFFSET      896
#define MAX_VC_COUNTER   0xFFFFFF
struct PACKED_GCC CADU
    {
        // Attached Sync Marker, 4 bytes
        t_uint32    ASM;
         // VCDU (transfer frame) primary header, 6 bytes
        t_uint8     version :2 ;
        t_uint8     spacecraft :8;
        t_uint8     vcid :6;
        t_uint32    vcdu_count : 24;
        t_uint8     replay : 1;  //signaling fields
        t_uint8     fccuf : 1;
        t_uint8     spare1 : 2;
        t_uint8     count_cycle : 4;
        // M_PDU header
        t_uint8     spare2 :5;
        t_uint16    packet_offset: 11;
        //
        BYTE         packet_data[CCSDS_MPDU_DATA_SIZE];

        //RS code bits
        t_uint8     rs[128];

    };

static inline t_uint16  CADU_GET_PACKET_OFFSET(BYTE* p) { return (t_uint16)( ( ( p[10] & 0x07) << 8 ) | (p[11]) );}
static inline t_uint16  CADU_GET_SPACECRAFT(BYTE* p) { return (t_uint16)( ( ( p[4] & 0x3FUL) << 2 ) | ((p[5] & 0xC0) >> 6) );}
static inline t_uint16  CADU_GET_VCID(BYTE* p) { return (t_uint16)( p[5] & 0x3F  );}
static inline t_uint32  CADU_GET_VCCOUNTER(BYTE* p) { return (t_uint32)( 0UL | (p[6] << 16)  | (p[7] << 8)  | p[8]); }


static inline t_uint16  TFV1_GET_PACKET_OFFSET(BYTE* p) { return (t_uint16)( ( ( p[10] & 0x07) << 8 ) | (p[11]) );}
static inline t_uint16  TFV1_GET_SPACECRAFT(BYTE* p) { return (t_uint16)( ( ( p[4] & 0x3FUL) << 2 ) | ((p[5] & 0xC0) >> 6) );}
static inline t_uint16  TFV1_GET_VCID(BYTE* p) { return (t_uint16)( p[6] );}
static inline t_uint32  TFV1_GET_VCCOUNTER(BYTE* p) { return (t_uint32)( p[7]); }


union   PACKED_GCC  uCADU
{
    BYTE bytes[CCSDS_CADU_SIZE];
    CADU cadu;
};


static inline t_uint32 ccsds_24to32(t_uint32 i24) //unpack 24bit length integer to LE int32
{
    BYTE* pin=(BYTE*)&i24;
    t_uint32 i32 = 0L | (pin[0]<<16) |  (pin[1]<<8) | pin[2];
    return i32;
}
static inline t_uint32 ccsds_16to32(t_uint16 i16,int bit_len) //unpack 16bit length integer to LE int32
{
    BYTE* pin=(BYTE*)&i16;
    t_uint32 i32 = 0L ;
     i32  |= (pin[0]<<(bit_len-8)) ;
      i32 |=  (pin[1] & ( 0xFF >> (16-bit_len) ) );
    return i32;
}
//
extern std::map<int, string> SPACECRAFT_NAMES;
#define SPACECRAFT_ID_TERRA     42
#define SPACECRAFT_ID_AQUA     154
#define SPACECRAFT_ID_AQUA_SCRAMBLES     166
#define SPACECRAFT_ID_METOPA     11
#define SPACECRAFT_ID_METOPB     12
#define SPACECRAFT_ID_METOPC     13


// -  VCID definitions
#define VCID_FILL   63
#define MAX_VCID_VALUE  63
extern std::map<int, string> VCID_NAMES;

// --- time scales and utilities
#define FPF_TIME_SEC_PER_DAY 86400
// MODIS epoch is 01-Jan-1958 (relative to unix time)
#define  EOS_TIME_EPOCH  -378691200
// METOP epoch is 01-Jan-2000
#define  METOP_TIME_EPOCH  946684800


#endif // CCSDS_H_INCLUDED
