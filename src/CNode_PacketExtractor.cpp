/*
FPF - Frame Processing Framework
See the file COPYING for copying permission.
*/
/*
CNode_PacketExtractor - node for extracting source packets from transfer frames.
 Produces side frame stream of different frame kind.

History:
    created: 2015-11 - A.Shumilin.
*/

#include <cstdlib> /* atoi */
#include <cstring>  /* memcpy */

#include "fpf.h"
#include "ccsds.h"
#include "CNode_PacketExtractor.h"
#include "class_factory.h"

#define MY_CLASS_NAME   "CNode_PacketExtractor"

#define INI_VCID    "vcid"
#define INI_SPCHAIN_NODE   "packet_chain_node"
#define INI_VALID_APIDS    "valid_apids"
#define INI_VALID_SIZES    "valid_sizes"
#define INI_VALID_SIZES    "valid_sizes"
#define INI_VCDU_INSERT_BYTES    "vcdu_insert"  //size of VCDU insert zone, on METOP = 2,
#define INI_OBT_EPOCH      "obt_epoch"

CNode_PacketExtractor::CNode_PacketExtractor()
{
    is_initialized = false;
    pnext_node = NULL;
    sp_chain_node = NULL;
    c_input_frames = 0;
    has_packet_sync = false;
    sp_buffer_fill = 0UL;
    sp_len = 0L;
    c_bad_headers = 0UL;
    c_zero_sizes = 0L;
    c_invalid_sizes = 0L;
    vcdu_insert_bytes = 0; //defalut, no insert zone
    c_frames_without_headers = 0L;
    PH_OFFSET_IN_CADU = CADU_DATA_OFFSET;
    MPDU_DATA_SIZE = CCSDS_MPDU_DATA_SIZE;
    obt_epoch = EOS_TIME_EPOCH;
}

CNode_PacketExtractor::~CNode_PacketExtractor()
{
    if (pnext_node != NULL) { *fpf_warn<<"!! CNode_PacketExtractor::~CNode_PacketExtractor(): not detached next node"; }
    if (sp_chain_node != NULL) { *fpf_warn<<"!! CNode_PacketExtractor::~CNode_PacketExtractor(): not detached sp_chain_node"; }
}

bool CNode_PacketExtractor::init(t_ini& ini, string& init_name, CChain* pchain_arg)
{
     *fpf_debug << "~ " MY_CLASS_NAME "::init("<<init_name<<")\n";
    is_initialized = false;
    //
    t_ini_section conf = ini[init_name];
    name = init_name;
    id = name;
    pchain = pchain_arg;
    //
    c_input_frames = 0UL;
    c_counter_gap = 0UL;
    c_invalid_header_offset = 0UL;
    sp_buffer_fill = 0UL;
    c_bad_headers = 0UL;
    c_zero_sizes = 0L;
    c_invalid_sizes = 0L;
    c_frames_without_headers = 0L;
    //
    obt_epoch = EOS_TIME_EPOCH;
    if (! conf[INI_OBT_EPOCH].empty())
    {
		obt_epoch =  strtol(conf[INI_OBT_EPOCH].c_str(),NULL,10);
	}
	//
    VCID =  strtoul(conf[INI_VCID].c_str(),NULL,10);
    if ((VCID<1) || (VCID > 63))
    {
        *fpf_error<<"ERROR: CNode_PacketExtractor("<<name<<")::init(): invalid VCID: "<<VCID<<endl;
        return false;
    }
    if (! conf[INI_VCDU_INSERT_BYTES].empty())
    {
		vcdu_insert_bytes =  strtoul(conf[INI_VCDU_INSERT_BYTES].c_str(),NULL,10);
		*fpf_trace << "=> CNode_PacketExtractor::init("<<name<<") using VCDU insert "<<vcdu_insert_bytes<<" bytes\n";
	}
	// correct field length and offsets for the insert zone
	PH_OFFSET_IN_CADU = CADU_DATA_OFFSET + vcdu_insert_bytes;
	MPDU_DATA_SIZE = CCSDS_MPDU_DATA_SIZE - vcdu_insert_bytes;
	//
    if (! conf[INI_VALID_APIDS].empty())
    {
        parse_to_int_vector(&valid_apids,conf[INI_VALID_APIDS]);
    }
    if (! conf[INI_VALID_SIZES].empty())
    {
        parse_to_int_vector(&valid_sizes,conf[INI_VALID_SIZES]);
    }
    //build next node
    bool no_next_node;
    pnext_node = get_next_node(ini,name,&no_next_node);
	if (pnext_node == NULL)
	{
        if (!no_next_node) //look like a config error
		{ 	*fpf_error << "ERROR: CNode_PacketExtractor::init("<<init_name<<") failed to create next node ["<< conf[INI_COMMON_NEXT_NODE] <<"]\n";		return false;}
	}
    else //init next node
    { if (! pnext_node->init(ini,conf[INI_COMMON_NEXT_NODE],pchain_arg))  { return false;}  }

    //build chain for SP
    string sp_node_name = conf[INI_SPCHAIN_NODE];
    if (sp_node_name.empty())
    { //this is not an error
       *fpf_trace << "=> CNode_PacketExtractor::init("<<name<<") no packet chain is attached\n";
    }else
    {
        if (ini.find(sp_node_name) == ini.end())
        {
            *fpf_error << "ERROR: CNode_PacketExtractor::init("<<name<<") no ini section for packet node["<<sp_node_name<<"]\n";
            return false;
        }
        t_ini_section section = ini[sp_node_name];
        if (ini[sp_node_name].find(INI_COMMON_CLASS) == ini[sp_node_name].end())
        {
            *fpf_error << "ERROR: CNode_PacketExtractor::init("<<name<<") no 'class' tag in ini section ["<<sp_node_name<<"]\n";
            return false;
        }
        string sp_node_classname = ini[sp_node_name][INI_COMMON_CLASS];
        sp_chain_node = new_node(sp_node_classname);
        if (sp_chain_node == NULL)
        {
           *fpf_error << "ERROR: CNode_PacketExtractor::init("<<name<<") failed to create packet node class ["<<sp_node_classname<<"::"<< sp_node_name <<"]\n";
           return false;
        }
        *fpf_trace<<"=> CNode_PacketExtractor::init("<<name<<") attaches "<<sp_node_name<<" to packets chain\n";
        { if (! sp_chain_node->init(ini,conf[INI_SPCHAIN_NODE],&sp_chain))  { return false;}  }
    }
    //
    *fpf_trace<<"=> " MY_CLASS_NAME "::init("<<name<<") intialized with VCID="<< (int) VCID<<"\n";
    is_initialized = true;
    return true;
}

void CNode_PacketExtractor::start(void)
{
}

void CNode_PacketExtractor::stop(void)
{
}

void CNode_PacketExtractor::close(void)
{
    if (pnext_node != NULL)
    {
        pnext_node->close();
        delete pnext_node;
        pnext_node = NULL;
    }
    if (sp_chain_node != NULL)
    {
        sp_chain_node->close();
        delete sp_chain_node;
        sp_chain_node = NULL;
    }
    is_initialized = false;
    *fpf_trace<<"<= " MY_CLASS_NAME "("<<name<<") closed, "<<sp_stream.c_frames<<" packets from "<<c_input_frames<<" input frames\n";
}

void CNode_PacketExtractor::take_frame(CFrame* pf)
{
    FPF_ASSERT( (pf!= NULL),"NULL frame pointer")
    FPF_ASSERT(is_initialized,"Node not initialized")
    // custom node actions
    do_frame_processing(pf);

    //pass the frame further by chain
    if (pnext_node != NULL) { pnext_node->take_frame(pf); }
}

bool CNode_PacketExtractor::good_sp_header(BYTE* phead)
{
    bool ok;
  //  if ( 0 != SP_GET_VER(phead) ) { return false; }
    int packet_len = SP_GET_LENGTH(phead);
    if (packet_len  > CCSDS_MAX_PACKET_SIZE) { return false; }
    if (packet_len  == 0) { c_zero_sizes++;  return false; }

    static int last_valid_size = -1;
    //-- fast size check
    if ((last_valid_size>0) && (packet_len == last_valid_size))
    {
        ok=true;
    }else
    {
        //--full size check
        if (valid_sizes.size()> 0)
        {
            ok = false;
            for(vector<int>::size_type i = 0; i != valid_sizes.size(); i++)
            {
                if (packet_len == valid_sizes[i])
                {
                    ok=true;
                    last_valid_size = packet_len;
                    break;
                }
            }
            if (!ok) {
                return false;
            }
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
        if (!ok) { return false; }
    }
    //

    //all checks are ok
    return true;
}

void CNode_PacketExtractor::do_frame_processing(CFrame* pf)
{
    c_input_frames++;
    FPF_ASSERT((FT_CCSDS_CADU == pf->frame_type),"CNode_PacketExtractor accepts only FT_CCSDS_CADU frame types")
    BYTE* pcadu = pf->pdata;

    //
    if (c_input_frames ==1)
    {
        sp_stream.id = pf->pstream->id;
        sp_stream.url = pf->pstream->url;
    }

    //ignore not relevant VC
    if (CADU_GET_VCID(pcadu) != VCID) { return; }
    //check VC counters
    t_uint32 new_vc_count = CADU_GET_VCCOUNTER(pcadu);//ccsds_24to32(cadu->vcdu_count);
    if (new_vc_count-1 != prev_vc_count)  //TODO - add overlap case
    {
       if (!(prev_vc_count == MAX_VC_COUNTER) && (new_vc_count == 0))
       {
        c_counter_gap++;
        sp_buffer_fill = 0; //reset SP buffer, some data is discarded, sync lost
        has_packet_sync = false;
       }
    }
    prev_vc_count = new_vc_count;
    //

    size_t next_header_offset = CADU_GET_PACKET_OFFSET(pcadu+vcdu_insert_bytes);

    //
    if (next_header_offset == 0x7FF) //no header in this frame
     {
        if (has_packet_sync && (sp_buffer_fill <= sp_len)) //collectin valid long packet
        {
            next_header_offset = 0; //but actually we have no packet here, take the all data
            memcpy(sp_buffer+sp_buffer_fill,pcadu + PH_OFFSET_IN_CADU,MPDU_DATA_SIZE);
            sp_buffer_fill += MPDU_DATA_SIZE;
            if (sp_buffer_fill < sp_len)
            {   //not full packet yet
                return; //go to collect the next frame
            }//else - proceed to issue this packet
        }else
        {
            c_frames_without_headers++;
            return;
        }
     }else
     {
        if (next_header_offset > MPDU_DATA_SIZE)
        {
            c_invalid_header_offset++;
            has_packet_sync = false;
            sp_buffer_fill = 0; //reset SP buffer, some data is discarded, sync lost
            return;
        }
        if (!has_packet_sync)// for resync copy starting from the new packet header
        { // drop some initial data and start filling buffer from the next packet header
            int add_bytes = MPDU_DATA_SIZE - next_header_offset;
            memcpy(sp_buffer,pcadu + PH_OFFSET_IN_CADU + next_header_offset,add_bytes);
            sp_buffer_fill = add_bytes;
        }else
        { // append full frame data content to the existing data
            memcpy(sp_buffer+sp_buffer_fill,pcadu + PH_OFFSET_IN_CADU,MPDU_DATA_SIZE);
            sp_buffer_fill += MPDU_DATA_SIZE;
        }
    }
    //

    while (sp_buffer_fill > CCSDS_SP_HEADER_SIZE)
    { //we have at least header to check
        if (good_sp_header(sp_buffer))
        {
            has_packet_sync = true;
            sp_len = SP_GET_LENGTH(sp_buffer) +CCSDS_SP_HEADER_SIZE+1; //!!, including prim.header
            int segm = SP_GET_SEGMENTATION(sp_buffer);
            if (segm != 3)
            {
                   segm = SP_GET_SEGMENTATION(sp_buffer);//?

            }
            if (sp_len <= sp_buffer_fill) // we have full packet in the buffer
            {
                int apid = SP_GET_APID(sp_buffer);
                //int  p_count =SP_GET_COUNT(sp_buffer);
                //emit it
                int tday = SP_GET_EOS_TS_DAYS(sp_buffer);
                int tmsec = SP_GET_EOS_TS_MSEC(sp_buffer);
                time_t tm = (tday*FPF_TIME_SEC_PER_DAY + obt_epoch) + int(tmsec / 1000);
                unsigned long tmu = (tmsec % 1000) * 1000 + SP_GET_EOS_TS_USEC(sp_buffer);

                sp_stream.curr_pos += sp_len;
                sp_stream.c_frames++;
                if (sp_stream.c_frames ==1)
                {//init on first packet
                   sp_stream.obt_base = tm;
                }
                packet_frame.cframe = sp_stream.c_frames;
                packet_frame.stream_pos = sp_stream.curr_pos;
                packet_frame.crc_ok = FRAME_CRC_NOTCHECKED;
                packet_frame.bit_errors = FRAME_CRC_NOTCHECKED;
                packet_frame.frame_size = sp_len;
                packet_frame.pdata = sp_buffer;
                packet_frame.scid = pf->scid;
                packet_frame.vcid = apid;
                packet_frame.frame_type = FT_CCSDS_SourcePacket;
                packet_frame.pstream = &sp_stream;
                packet_frame.obtime = tm;
                packet_frame.obtime_usec = tmu;
                if (sp_stream.obt_base == 0 )
                { sp_stream.obt_base = tm; sp_stream.obt_base_usec = tmu; }
                if (pf->pstream->obt_base == 0 )
                { pf->pstream->obt_base = tm; pf->pstream->obt_base_usec = tmu; }
                packet_frame.wctime = time(NULL);
                packet_frame.actime = pf->actime; packet_frame.actime_usec = pf->actime_usec;
                // update OBT in the CADU frames as well
                pf->obtime = tm; pf->obtime_usec = tmu;
                //
                if (sp_chain_node != NULL) { sp_chain_node->take_frame(&packet_frame); }
                //shift next packet to the buffer start
                FPF_ASSERT(sp_buffer_fill >= sp_len,"CNode_PacketExtractor::do_frame_processing): sp_buffer_fill > sp_len");
                size_t new_sp_buffer_fill = sp_buffer_fill - sp_len;

                //memcpy(sp_buffer, sp_buffer+sp_len-1, new_sp_buffer_fill);
                for (size_t i =0; i < new_sp_buffer_fill; i++) {*(sp_buffer+i) = *(sp_buffer+sp_len+i);}
                sp_buffer_fill = new_sp_buffer_fill;
            }else{
                //get the next frame
                if (0) //next_header_offset > 0) // this means that the next packet is already in the buffer
                {//something wrong with the size we got
                    c_invalid_sizes++;
                    has_packet_sync = false; // shift new packet to the beginning of the buffer and resync
                    int add_bytes = MPDU_DATA_SIZE - next_header_offset;
                    memcpy(sp_buffer,pcadu + PH_OFFSET_IN_CADU + next_header_offset,add_bytes);
                    sp_buffer_fill = add_bytes;
                    next_header_offset = 0 ; //otherwise we will get here again in the cycle
                }else
                {
                    return;
                }
            }
        } // if (good_sp_header(sp_buffer))
        else
        {
            c_bad_headers ++;
            has_packet_sync = false;
            return;
        }
    }

}

