/*
FPF - Frame Processing Framework
See the file COPYING for copying permission.
*/
/*
FrameSource_PDS.h - CCSDS source packet stream ( EOS PDS like format) framer object

History:
    created: 2015-11 - A.Shumilin.
*/
#include <cstdlib> /* strtol */
#include <cstring> /*memcpy */
#include "CFrameSource_PDS.h"
#include "ccsds.h"

#define MY_CLASS_NAME   "CFrameSource_PDS"


/*  Stream input buffer size.
    Interpreted as bytes or kibibytes is "k" suffix appended */
#define INI_BUFF_SIZE       "buff_size"
#define INI_VALID_APIDS    "valid_apids"
#define INI_VALID_SIZES    "valid_sizes"
#define INI_OBT_EPOCH      "obt_epoch"

CFrameSource_PDS::CFrameSource_PDS()
{
    is_initialized = false;
    //
    input_buffer = NULL;
    input_stream = NULL;
    count_lost_syncs = 0;
    count_read = 0;
    count_frames = 0;
    count_invalid_sizes = 0;
    count_invalid_apids = 0;
    count_skipped_packets = 0;
    //
    frame_size = 0;
    buff_size = 128 * 1024;
    obt_epoch = EOS_TIME_EPOCH;
}

CFrameSource_PDS::~CFrameSource_PDS()
{
    if (input_buffer != NULL) { delete input_buffer; }
}

bool  CFrameSource_PDS::init(t_ini& ini, string& init_name)
{
     *fpf_debug << "~ " MY_CLASS_NAME "::init("<<init_name<<")\n";
    is_initialized = false;
    if (ini.find(init_name) == ini.end())
    {
        *fpf_error<<"ERROR: "MY_CLASS_NAME"::init(): init failed, no section ["<<init_name<<"] in the ini file\n";
        return false;
    }
    name = init_name;
    id = name;

try
{
    t_ini_section conf = ini[name];
    //initialize parameters

    //
    if (conf.find(INI_BUFF_SIZE) != conf.end())
    {
        buff_size = strtol(conf[INI_BUFF_SIZE].c_str(),NULL,10); //interpret as hexadecimal
        if (conf[INI_BUFF_SIZE].find_first_of("kK") > 0) { buff_size *= 1024; }
    }
    if (! conf[INI_VALID_APIDS].empty())
    {
        parse_to_int_vector(&valid_apids,conf[INI_VALID_APIDS]);
    }
    if (! conf[INI_VALID_SIZES].empty())
    {
        parse_to_int_vector(&valid_sizes,conf[INI_VALID_SIZES]);
    }
    obt_epoch = EOS_TIME_EPOCH;
    if (! conf[INI_OBT_EPOCH].empty())
    {
		obt_epoch =  strtol(conf[INI_OBT_EPOCH].c_str(),NULL,10);
	}
    // -- connect to input stream
    if (conf.find(INI_COMMON_INPUT) != conf.end())
    {
        input_stream = new_input_stream(ini[conf[INI_COMMON_INPUT]][INI_COMMON_CLASS]); // NULL is allowed here
        if ( input_stream != NULL)
        {
            if (input_stream->init(ini,conf[INI_COMMON_INPUT]))
            {
                stream.id = input_stream->id;
                stream.url = input_stream->url;
            }else{
                //init failed in the chain
                is_initialized = false;
                return false;
            }
        }
    }else { input_stream = NULL; }
    //

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
    input_buffer = new BYTE[buff_size];
    if (input_buffer == NULL)
    {
        *fpf_warn<< "!! "MY_CLASS_NAME"::init(): failed allocation of frame buffer of size "<<buff_size<<" bytes\n";
        return false;
    }
    // reset  counters
    count_lost_syncs = 0;
    count_read = 0;
    count_frames = 0;
    count_skipped_packets = 0;

        //
}catch (const std::exception& ex)
{
        *fpf_error<<"ERROR: "MY_CLASS_NAME"::ini() failed with exception:\n"<<ex.what()<<endl;
        input_stream->close();
        delete input_stream;
        input_stream = NULL;
        return false;
}
catch(...)
{
        *fpf_error<<"ERROR: "MY_CLASS_NAME"::ini() failed with nonstd exception\n";
        input_stream->close();
        delete input_stream;
        input_stream = NULL;
        return false;
}


    *fpf_trace<<"=> " MY_CLASS_NAME "::init("<<name<<") initialized\n";
    is_initialized = true;
    return true;
}

void CFrameSource_PDS::start(void)
{
    FPF_ASSERT( is_initialized, "CFrameSource_PDS::start on non initialized object");
    //
    run_framing();
    // close the chain after run

}

void CFrameSource_PDS::close(void)
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
    *fpf_trace<<"<= " MY_CLASS_NAME "("<<name<<") closed, "<<count_read<< " bytes in, "<<count_frames<<" packages out\n";
}

//////////////
bool CFrameSource_PDS::good_sp_header(BYTE* phead)
{
    bool ok;
    if ( 0 != SP_GET_VER(phead) ) { return false; }
    int packet_len = SP_GET_LENGTH(phead);
    if (packet_len  > 16000) { count_invalid_sizes++; return false; }

    if (valid_sizes.size()> 0)
    {
        ok = false;
        for(vector<int>::size_type i = 0; i != valid_sizes.size(); i++)
        {
            if (packet_len == valid_sizes[i])
                {
                    ok=true;
                    break;
                }
        }
        if (!ok) {
                count_invalid_sizes++;
                return false;
        }
    }
    //
    if (valid_apids.size()> 0)
    {
        ok = false;
        int apid = SP_GET_APID(phead);
        for(vector<int>::size_type i = 0; i != valid_apids.size(); i++)
        {
            if (apid == valid_apids[i])
            {
                    ok=true;
                    break;
            }
        }
        if (!ok) { count_invalid_apids++; return false; }
    }
    //

    //all checks are ok
    return true;
}

void CFrameSource_PDS::run_framing(void)
{
    //fixed and initial frame settings
    CFrame frame;

    frame.frame_type = FT_CCSDS_SourcePacket;
    frame.pstream = &stream;
    stream.url = input_stream->url;
    stream.id = input_stream->id;
    frame.cframe = 0;
    frame.vcid = 0;
    frame.scid = 0;
    //
    *fpf_trace<<"CFrameSource_PDS{"<<name<<")::run_framing starts framing...\n";

    size_t readed;
    int ioerror;

     t_file_pos frame_input_pos = 0;
     //t_file_pos input_base_pos = 0 ;
     BYTE* end_of_data = input_buffer;
     BYTE* ph;
     int sp_len;
    while(true)
    { //read cycle
        size_t to_read = buff_size - (end_of_data - input_buffer);
        readed = input_stream->read(end_of_data, to_read,ioerror);
        if (readed < 1)
        {
            break;
        }
        end_of_data += readed;
        count_read += readed;
        // expect packet at 0
        ph = input_buffer;

        //TODO check sp_len
        while (ph+CCSDS_SP_HEADER_SIZE < end_of_data) //we still have at least header
        {
            sp_len = SP_GET_LENGTH(ph) +CCSDS_SP_HEADER_SIZE+1; //!! +1
            if (ph+sp_len < end_of_data)
            {//we have a full packet
                //check header validity and decide if to take it
                //TO DO...
                good_sp_header(ph);
                //-ok, emit the packet
                stream.c_frames++;
                int tday = SP_GET_EOS_TS_DAYS(ph);
                int tmsec = SP_GET_EOS_TS_MSEC(ph);
                if (stream.c_frames ==1)
                {//init on first packet
                    stream.obt_base = (tday*FPF_TIME_SEC_PER_DAY + obt_epoch) + int(tmsec / 1000);
                    stream.obt_base_usec = (SP_GET_EOS_TS_MSEC(ph) % 1000) * 1000 + SP_GET_EOS_TS_USEC(ph);
                }
                //
                frame.frame_size = sp_len;
                frame.cframe++;
                frame.pdata = ph;
                frame.vcid = SP_GET_APID(ph);
                frame.wctime = time(NULL); frame.wctime_usec = 0UL;
                frame.obtime = (tday*FPF_TIME_SEC_PER_DAY + obt_epoch) + int(tmsec / 1000);
                frame.obtime_usec = (tmsec % 1000) * 1000 + SP_GET_EOS_TS_USEC(ph);
                frame.stream_pos = frame_input_pos;
                frame.crc_ok = FRAME_CRC_NOTCHECKED;
                frame.bit_errors = FRAME_CRC_NOTCHECKED;
                count_frames++;
                if (pnext_node!= NULL) { pnext_node->take_frame(&frame); }
                // to next packet
                ph += sp_len;//to next packet
                frame_input_pos += sp_len;
            }//full packet processing
            else { break; }
        }
        // move remainder to the beginning and fetch new data
        int bytes_moved = 0;
        for (bytes_moved=0; ph<end_of_data; ph++,bytes_moved++) { input_buffer[bytes_moved] = *ph;}
        end_of_data = input_buffer+bytes_moved;
        //and red the buffer again
    } //read cycle
    //
    *fpf_trace<<"CFrameSource_PDS::run_framing framing finished, "<<count_frames<< " frames in "<<count_read<<" bytes\n";
}

