/*
FPF - Frame Processing Framework
See the file COPYING for copying permission.
*/
/*
fpf.h - primary header file to include FPF framework.
 Contains declarations for base classes and structures.

History:
    created: 2016-06 - A.Shumilin.
*/

#ifndef FPF_H
#define FPF_H

#include <time.h>
#include <ctime>
#include <iostream>
#include <stdexcept>
#include <stdint.h>
#include <stdlib.h>
//
#include "ini.h"

//
#define FPF_LITTLE_ENDIAN   1

// some system specific settings
#ifndef NULL
#define NULL ((void*) 0)
#endif

#define EXIT_FPF_ASSERT -5
#ifdef DEBUG
#define FPF_ASSERT(x,msg) if (!(x)) { *fpf_error<<"!!!!!\nFPF_ASSERT("<< #x << ") failed at "<<__FILE__<<":"<<__LINE__<<"\nReason: "<< msg<<"\n!!!!\n"; throw std::runtime_error(string("FPF_ASSERT Exception:")+ __FILE__ ); }
#else
#define FPF_ASSERT(x,msg) if (!(x)) { *fpf_error<<"!!!!!\nFPF_ASSERT("<< #x << ") failed.\nReason: "<< msg<<"\n!!!!\n"; throw std::runtime_error(string("FPF_ASSERT Exception:")+ __FILE__ ); }
#endif
// data types

#define PACKED_GCC  __attribute__((packed))

enum t_frame_type { FT_NOTYPE = 0, FT_CCSDS_CADU =1, FT_CCSDS_SourcePacket =2 };
enum t_stream_type { ST_NOTYPE = 0 };

typedef long long t_stream_pos ;
typedef uint64_t t_file_pos ;
typedef uint8_t   BYTE ;
typedef uint8_t t_uint8;
typedef int8_t t_int8;
typedef uint16_t t_uint16;
typedef int16_t t_int16;
typedef uint32_t t_uint32;
typedef int32_t t_int32;

// working output streams, may be changed in rt
extern ostream* fpf_error;
extern ostream* fpf_warn;
extern ostream* fpf_info;
extern ostream* fpf_debug;
extern ostream* fpf_trace;

extern string fpf_last_error;
// -- configuration tools

// tags that may be filled from the main's argv into system ARGV section
#define INI_COMMON_SECTION_ARGV    "__ARGV__"
#define INI_ARGV_INPUT_FILE         "input_file"


// common names for INI tags
#define INI_COMMON_CLASS    "class"
#define INI_COMMON_CONFIGID    "_section_"
#define INI_COMMON_NEXT_NODE    "next_node"
#define INI_COMMON_ID    "id"
#define INI_COMMON_URL    "url"
#define INI_COMMON_INPUT      "input"
#define INI_COMMON_INPUT_FILE   "input_file"
#define INI_COMMON_VAL_YES  "yes"
#define INI_COMMON_VAL_NO  "no"
//



class CStream
{

    public:
        CStream();
        string id;
        string url;
        t_stream_pos  curr_pos;
        //-- timestamping
        time_t    wct_base; // seconds, unix epoch
        long int  wct_base_usec;
        time_t    acqt_base;
        long int  acqt_base_usec;
        time_t    obt_base;
        long int  obt_base_usec;
        //-- counters
        unsigned long int  c_frames;
};

#define FRAME_CRC_NOTCHECKED -1
#define FRAME_CRC_OK          0
#define FRAME_CRC_FAILED      1

class CFrame
{

    public:
        CFrame();
        BYTE* pdata;       //pointer to the frame data buffer
        CStream* pstream;  //pointer to the Stream
        unsigned int frame_size; // frame size
        t_frame_type frame_type; // frame type
        t_stream_pos stream_pos; //position of the frame in the stream
           t_uint32  cframe;    // frame counter as issued by framer
                int  vcid;      // channel ID (VCID,APID,etc)
                int  scid;      // spacecraft or source ID
                int  crc_ok;    // frame passed error correction
                int  bit_errors; //bit errors estimation
             time_t  wctime;     //wall clock time, seconds, unix epoch
           long int  wctime_usec;
             time_t  actime;     //acquisition clock time, seconds, unix epoch
           long int  actime_usec;
             time_t  obtime;     //on-board clock time, seconds, unix epoch
           long int  obtime_usec;

};



class CChain
{
    public:
};

class INode   // chain node interface class, 
{
    public:
        INode();
        virtual ~INode();
        //
        string name;
        string id;
        bool is_initialized;
        INode* pnext_node;  //-> ref. to next Node  
        //state control methods
        virtual bool init(t_ini& ini, string& name, 
                          CChain* pchain);
        virtual void start(void);
        virtual void stop(void);
        virtual void close(void);
        //frame processing
        virtual void take_frame(CFrame* pf);
    protected:
        CChain* pchain;
};

class IInputStream   // chain node interface class, to be inherited by all the nodes
{
    public:
        IInputStream ();
        virtual ~IInputStream()=0;
        //
        string name;
        string id;
        string url;
        bool is_initialized;
        size_t  stream_pos;
        //state control methods
        virtual bool init(t_ini& ini, string& name);
        virtual void start(void);
        virtual void stop(void);
        virtual void close(void);
        //frame processing
        virtual  unsigned int read(BYTE* pbuff, size_t bytes_to_read, int& ierror);
    protected:
    private:
};

class IFrameSource // class instantiating the chain and emitting frames
{
    public:
        IFrameSource();
        virtual ~IFrameSource()=0;
        //
        string name;
        string id;
        bool is_initialized;
        CChain* pchain;
        CStream stream;
        INode* pnext_node;
        long int  cframes; //counters of frames generated
        //
        virtual bool init(t_ini& ini, string& name);
        virtual void start(void);
        virtual void close(void);

};


// -- some common utilities --
void fpf_swap4bytes(BYTE* );

#endif // FPF_H
