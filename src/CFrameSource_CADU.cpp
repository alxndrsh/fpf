/*
FPF - Frame Processing Framework
See the file COPYING for copying permission.
*/
/*
FrameSource_CADU - CADU (CCSDS transfer frame) framer object

History:
    created: 2015-11 - A.Shumilin.
*/
#include <cstdlib> /* strtol */
#include <cstring> /*memcpy */
#include <ctime>
#include "CFrameSource_CADU.h"


#define MY_CLASS_NAME   "CFrameSource_CADU"

#define SYNC_MARKER_SIZE    4

/// -- //
/* Custom ASM(CADU sync marker), if not given standard CCSDS marker will be used.
   Must be given as a 4 byte integer in hexadecimal notation.   */
#define  INI_SYNC_MARKER    "sync_marker"
#define  INI_FRAMESIZE    "frame_size"

/*  Stream input buffer size.
    Interpreted as bytes or kibibytes is "k" suffix appended */
#define INI_BUFF_SIZE       "buff_size"
#define INI_DISCARD_FILL    "discard_fill"
#define INI_TRACE_SYNC   "trace_sync"
#define INI_FIX_IQ_AMBIGUITY   "fix_iq"

const char* QPSK_AMBIGUITY_NAMES[] = { "IQ", "I~Q","~IQ","~I~Q", "QI", "Q~I", "~QI" , "~Q~I" };

inline BYTE fix_ambiguity_byte(BYTE b,int amb_case)
{
        BYTE I; BYTE Q;
    switch(amb_case)
    {
        case 0: //IQ, do nothing
            return b;
        case 1: // I~Q
            return b ^ BYTE(0x55);
        case 2: // ~IQ
            return b ^ BYTE(0xAA);
        case 3: // ~I~Q
            return ~b ;
        case 4: // QI
            I = b & 0xAA;   I >>= 1;
            Q = b & 0x55;   Q <<= 1;
            return I + Q;
        case 5: // Q~I
            I = b & 0xAA; I ^= 0xAA;  I >>= 1;
            Q = b & 0x55;   Q <<= 1;
            return I + Q;
        case 6: // Q~I
            I = b & 0xAA;   I >>= 1;
            Q = b & 0x55;  Q ^= 0x55; Q <<= 1;
            return I + Q;
        case 7: // Q~I
            I = b & 0xAA;   I >>= 1;
            Q = b & 0x55;   Q <<= 1;
            return ~(I + Q);
    }
    return b; //to  prevent warning
}

void fix_ambiguity_bytes(BYTE* pb,int amb_case, size_t nbytes)
{
    if (amb_case == 0) { return; } //nothing to do
    for (size_t i = 0; i < nbytes; i++)
    {
        *pb = fix_ambiguity_byte(*pb,amb_case);
        pb++;
    }
}

CFrameSource_CADU::CFrameSource_CADU()
{
    is_initialized = false;
    sync_marker = 0;


    input_buffer = NULL;
    input_stream = NULL;
    bit_shift = 0;
    sync_max_badasm = 4;
    count_lost_syncs = 0;
    discard_fill = false;
    trace_sync = false;
    fix_iq = false;
    ambiguity_case = 0;
    //
    frame_size = CCSDS_CADU_SIZE;
    //
    BUFF_PREFIX = 8 + CCSDS_CADU_SIZE;
    buff_size = frame_size * 512; //align by frame size
}

CFrameSource_CADU::~CFrameSource_CADU()
{
    if (input_buffer != NULL) { delete input_buffer; }
}

bool  CFrameSource_CADU::init(t_ini& ini, string& init_name)
{
     *fpf_debug << "~ " MY_CLASS_NAME "::init("<<init_name<<")\n";
    if (ini.find(init_name) == ini.end())
    {
        *fpf_error<<"ERROR: " MY_CLASS_NAME "::init(): init failed, no section ["<<init_name<<"] in the ini file\n";
        return false;
    }
    name = init_name;
    is_initialized = false;

try
{
    t_ini_section conf = ini[name];
    //initialize parameters

    if (conf.find(INI_SYNC_MARKER) != conf.end())
    {
        sync_marker = strtol(conf[INI_SYNC_MARKER].c_str(),NULL,16); //interpret as hexadecimal
    }else { sync_marker = CCSDS_CADU_ASM; }
//
#ifdef FPF_LITTLE_ENDIAN
        fpf_swap4bytes((BYTE*)&sync_marker);
#endif // FPF_LITTLE_ENDIAN
    //
    discard_fill = false;
    if (conf[INI_DISCARD_FILL] == INI_COMMON_VAL_YES) {discard_fill=true;}
    //
    trace_sync = false;
    if (conf[INI_TRACE_SYNC] == INI_COMMON_VAL_YES) {trace_sync=true;}
    //
    fix_iq = false;
    if (conf[INI_FIX_IQ_AMBIGUITY] == INI_COMMON_VAL_YES) {fix_iq=true;}
    //
    if (conf.find(INI_FRAMESIZE) != conf.end())
    {
            frame_size = strtoul(conf[INI_FRAMESIZE].c_str(),NULL,10);
            //adjust buffer size as well
            BUFF_PREFIX = 8 + frame_size;
            buff_size = frame_size * 512;
    }
    //
    if (conf.find(INI_BUFF_SIZE) != conf.end())
    {
        buff_size = strtol(conf[INI_BUFF_SIZE].c_str(),NULL,10);
        if (conf[INI_BUFF_SIZE].find_first_of("kK") > 0) { buff_size *= 1024; }
    }
    // -- connect to input stream
    if (conf.find(INI_COMMON_INPUT) != conf.end())
    {
        input_stream = new_input_stream(ini[conf[INI_COMMON_INPUT]][INI_COMMON_CLASS]); // NULL is allowed here
        if ( input_stream != NULL)
        {
            if (input_stream->init(ini,conf[INI_COMMON_INPUT]))
            {
                //NOP
            }else{
                //init failed in the chain
                is_initialized = false;
                return false;
            }
        }
    }else { input_stream = NULL; }
    //
    ///// connect and init further cahin
    // connect the first node
    bool no_next_node;
    pnext_node = get_next_node(ini,name,&no_next_node);
	if (pnext_node == NULL)
	{
		if (!no_next_node) //look like a config error
		{
			*fpf_error << "ERROR: CNode_Counter::init("<<init_name<<") failed to create next node ["<< conf[INI_COMMON_NEXT_NODE] <<"]\n";
			input_stream->close();
			delete input_stream;
			input_stream = NULL;
			return false;
		}
	}
    else //init next node
    {
            if (! pnext_node->init(ini,conf[INI_COMMON_NEXT_NODE],pchain))  { return false;}
    }
    //
    input_buffer = new BYTE[buff_size+BUFF_PREFIX];
    if (input_buffer == NULL)
    {
        *fpf_warn<< "!! " MY_CLASS_NAME "::init(): failed allocation of frame buffer of size "<<buff_size<<" bytes\n";
        return false;
    }
    // reset  counters
    count_lost_syncs = 0;
    count_frames = 0;
    count_read = 0;
    sync_pos_first = 0;
    sync_pos_last = 0;
    c_fillers = 0;
        //
}catch (const std::exception& ex)
{
        *fpf_error<<"ERROR: " MY_CLASS_NAME "::ini() failed with exception:\n"<<ex.what()<<endl;
        input_stream->close();
        delete input_stream;
        input_stream = NULL;
        return false;
}
catch(...)
{
        *fpf_error<<"ERROR: " MY_CLASS_NAME "::ini() failed with nonstd exception\n";
        input_stream->close();
        delete input_stream;
        input_stream = NULL;
        return false;
}


    *fpf_trace<<"=> " MY_CLASS_NAME "::init("<<name<<") framer initialized, frame size="<<frame_size<<"\n";
    is_initialized = true;
    return true;
}

void CFrameSource_CADU::start(void)
{
    FPF_ASSERT( is_initialized, "CFrameSource_CADU::start on non initialized object");
    //
    if (input_stream == NULL)
    {
        *fpf_error<<"WARNING: " MY_CLASS_NAME "::start("<<name<<") can not start without valid input stream !!\n";
        return;
    }
    run_framing();
    // close the chain after run

}

void CFrameSource_CADU::close(void)
{
    if (input_buffer != NULL) { delete input_buffer; input_buffer = NULL; }
    if (pnext_node != NULL)
    {
        pnext_node->close();  delete pnext_node;  pnext_node = NULL;
    }
    if (input_stream != NULL)
    {
        input_stream->close();  delete input_stream; pnext_node = NULL;
    }
    is_initialized = false;
    *fpf_trace<<"<= " MY_CLASS_NAME "("<<name<<") closed, "<<count_read<<" bytes went in, "<<count_frames<<" frames out\n";
}

//////////////
BYTE* CFrameSource_CADU::acquire_sync(BYTE* buff,size_t bsize)
/* search for and return a first position of the ASM */
{
    return acquire_sync(buff,buff + bsize);
}

BYTE* CFrameSource_CADU::acquire_sync(BYTE* buff,BYTE* buff_end)
/* search for and return a first position of the ASM */
{
    FPF_ASSERT(buff_end > buff," CFrameSource_CADU::acquire_sync() Invalid buffer pointers!!")
    t_uint32 *pL;
    BYTE* pos = buff;
    for (int bit =0; bit<8; bit++)
    {// search ASM at this left bit shift
        if (bit>0)
        {
             BYTE  lbit;
             BYTE* pb = buff;
             for (int b = 0; pb < buff_end; b++ ,pb++) //shift be 1 bit left
             {
                lbit = *pb & '\x80';//gets either 80 or 0
                if (b>0)
                {// set it to the the previous byte
                    if (lbit == '\x00') {*(pb-1) &= '\xFE'; } // set lsb to 0
                    else { *(pb-1) |= '\x01'; } //set lsb to 1
                }
                /* Shift the current byte to the left */
                *pb = *pb << 1;
            }
        }
        //check full buffer again at new bit shift

        for (int try_amb_case = 0; try_amb_case< 8;try_amb_case++)
        {
            t_uint32 try_sync_marker = sync_marker;
            fix_ambiguity_bytes((BYTE*)&try_sync_marker,try_amb_case,SYNC_MARKER_SIZE);
            pos = buff;//repeat through full buffer
            for (size_t i=0; pos<buff_end-4; i++, pos++)
            {
                pL = (t_uint32 *)  pos;
                if (*pL == try_sync_marker)
                {
                    bit_shift = bit;
                    if (trace_sync && (ambiguity_case != try_amb_case))
                    {
                         *fpf_trace<<"= " MY_CLASS_NAME "("<<name<<") change IQ phase from "<<QPSK_AMBIGUITY_NAMES[ambiguity_case]<<" to "<<QPSK_AMBIGUITY_NAMES[try_amb_case]<<endl;
                    }
                    ambiguity_case = try_amb_case;
                    //iq fix should be applied here, the same as the bit shift
                    fix_ambiguity_bytes(pos,ambiguity_case,buff_end-pos);
                    if (trace_sync)
                    {
                        *fpf_trace<<"= " MY_CLASS_NAME "("<<name<<") sync acquired at "<<(pos-buff)<<", bit shift= "<<bit_shift<<", IQ="<<QPSK_AMBIGUITY_NAMES[ambiguity_case]<<endl;
                    }
                    return  pos;
                }
                // pos++;
            }
            if (!fix_iq) { break; } // do not try other cases
        }//ambiguity_case cycle


    }
    return NULL;// means no sync found till bsize-sizeof(syncmarker) offsets
}

void left_bit_shift(BYTE* buff,size_t buffsize,int nbits)
{
     BYTE  lbit;
     for (int bit =0; bit<nbits; bit++)
     {

        for (size_t b = 0; b < buffsize; b++)
         {
            lbit = buff[b] & '\x80';
            {
                if (lbit == '\x00') {buff[b - 1] &= '\xFE'; }
                 else { buff[b - 1] |= '\x01'; }
            }
            buff[b] = buff[b] << 1;
        }

     }
}

void CFrameSource_CADU::run_framing(void)
{
    //fixed and initial frame settings
    CFrame frame;
    frame.frame_size = frame_size;
    frame.frame_type = FT_CCSDS_CADU;
    frame.pstream = &stream;
    FPF_ASSERT(input_stream," framing started on NULL input object!")
    stream.url = input_stream->url;
    stream.id = input_stream->id;
    frame.cframe = 0;
    frame.vcid = 0;
    frame.scid = 0;
    //
    *fpf_info  << "= " MY_CLASS_NAME "("<<name<<")  starting framing\n";
    *fpf_info  << "= input stream ID = "<<stream.id<<", url = "<<stream.url<<"\n";
    //
    size_t readed;
    int ioerror;
    bool has_sync = false;
     BYTE* syncp;
     BYTE* input_buffer_end = input_buffer+BUFF_PREFIX+buff_size;
     t_uint32* pasm;
     int cFailedASM=0;
     count_read = 0l;
     int tail_size = 0;
     t_file_pos frame_input_pos = 0;
     t_file_pos input_base_pos = 0 ;
     t_file_pos input_stream_start_offset = input_stream->stream_pos;//take initial stream position
     BYTE* buffer_base = input_buffer+BUFF_PREFIX;
    while(true)
    { //read cycle
        readed = input_stream->read(buffer_base,buff_size,ioerror);
        input_base_pos = count_read;
        count_read += readed;
        if (readed < 1)
        {
            break;
        }
        //update stream info after each read
        if (stream.url != input_stream->url)
        {
           *fpf_info  << "= " MY_CLASS_NAME "("<<name<<")  input stream url changed to :"<<stream.url<<"\n";
            stream.url = input_stream->url;
        }
        if (stream.id != input_stream->id)
        {
           *fpf_info  << "= " MY_CLASS_NAME "("<<name<<")  input stream ID changed to :"<<stream.id<<"\n";
            stream.id = input_stream->id;
        }
        //TO DO - want to read at least a few frames
        //
        BYTE* frame_pos = buffer_base -tail_size;
        frame_input_pos = input_base_pos + (frame_pos - buffer_base);
        input_buffer_end = buffer_base +readed;
        //-- attach to previous buffer tail
        BYTE* pprev_byte = buffer_base-1;
        if (bit_shift > 0)
        {//then bit shift this new content to attach to the previous tail in the prefix space

            BYTE merged_byte = *(pprev_byte);//save the last partial byte to merge with new content
            left_bit_shift(pprev_byte,readed+1,bit_shift);
            //
            unsigned char merge_mask = 0x00; for (int ib=0;ib<bit_shift;ib++){ merge_mask |= (0x01 << ib); }
            *(pprev_byte) = (merged_byte & (~merge_mask)) | (  *(pprev_byte) & merge_mask);

        }
        if (fix_iq) // fix new readed block
        {
            fix_ambiguity_bytes(pprev_byte,ambiguity_case,readed);//this leaves the last, may be not full byte, unfixed
        }
        //
        while (frame_pos < input_buffer_end - frame_size -1)
        {
            //check if we still on sync
            if (has_sync)
            {
                pasm = (t_uint32*)  frame_pos;
                if (*pasm != sync_marker)
                {
                    if (++cFailedASM > sync_max_badasm)
                    {
                        count_lost_syncs++;
                        has_sync = false;
                    };
                }else
                {
                    sync_pos_last = input_base_pos +(frame_pos - buffer_base);
                    cFailedASM = 0; //reset failed counter
                } //last confirmed sync marker
            }
            //
            if (!has_sync)
            {
                //try to acquire sync in the remaininig part of the buffer
                syncp = acquire_sync(frame_pos,input_buffer_end); //also shifts the buffer and sets bit_shift
                if (syncp == NULL)
                {
                    tail_size = 0; //start next sync from new data
                    goto to_refill_buffer; // fetch next buffer
                }
                //got sync
                cFailedASM=0;
                frame_pos = syncp;
                frame_input_pos = input_base_pos + (frame_pos - buffer_base);
                if(sync_pos_first == 0) { sync_pos_first = frame_input_pos; }
                has_sync = true;
            }
            //filter out some frames
            int vcid = CADU_GET_VCID(frame_pos);
            if (vcid == VCID_FILL)
            {
                c_fillers++;
                if (discard_fill && (vcid == VCID_FILL))
                {
                  goto to_next_frame;
                }
            }

            // setup and emit frame
            count_frames++;
            frame.cframe=count_frames;
            frame.pdata = frame_pos;
            frame.stream_pos = frame_input_pos + input_stream_start_offset;
            frame.crc_ok = FRAME_CRC_NOTCHECKED;
            frame.bit_errors = FRAME_CRC_NOTCHECKED;
            frame.vcid = vcid;
            frame.scid = CADU_GET_SPACECRAFT(frame_pos);
            frame.wctime = time(NULL); frame.wctime_usec = 0UL;
            frame.actime = 0UL; frame.actime_usec = 0UL;
            frame.obtime = 0UL; frame.obtime_usec = 0UL;
            if (pnext_node!= NULL) { pnext_node->take_frame(&frame); }

            // step to next frame
  to_next_frame:
           frame_pos += frame_size;
            frame_input_pos += frame_size;
        }//frames in buff cycle

        // -- move the remaining tail to the prefix space,
        if (has_sync)
        {
            tail_size = input_buffer_end - frame_pos;
            FPF_ASSERT((tail_size < BUFF_PREFIX),"tail does not fit in the prefix space!");
            if (tail_size > 0)
            {
                memcpy(buffer_base-tail_size,frame_pos,tail_size);
            }
        }else {tail_size = 0;} //if not synced, drop the tail and start from the beginning of next stream chunk
        //
to_refill_buffer: ;

    } //read cycle
    //
    *fpf_trace<<"\n<= CFrameSource_CADU::run_framing finished, created "<<count_frames<< " frames received in "<<count_read<<" bytes\n\n";
}

