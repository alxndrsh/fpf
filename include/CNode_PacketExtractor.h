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

#ifndef CNODE_PACKETEXTRACTOR_H
#define CNODE_PACKETEXTRACTOR_H
#include <vector>
#include "fpf.h"

#define  FACTORY_CNODE_PACKETEXTRACTOR(c) FACTORY_ADD_NODE(c,CNode_PacketExtractor)

#define PACKET_BUFFER_SIZE    CCSDS_MAX_PACKET_SIZE+CCSDS_MPDU_DATA_SIZE

class CNode_PacketExtractor : public INode
{
    public:
        CNode_PacketExtractor();
        virtual ~CNode_PacketExtractor();
        //INode
        bool init(t_ini& ini, string& name, CChain* chain);
        void start(void);
        void stop(void);
        void close(void);
        void take_frame(CFrame* pf);
    protected:
    private:
        t_uint8    VCID; // VCID to work on
        t_uint32   vc_counter;

        INode*      sp_chain_node;
        //
        CChain  sp_chain;
        CStream sp_stream;
        BYTE sp_buffer[PACKET_BUFFER_SIZE];
         size_t sp_buffer_fill; //number of bytes in the packet buffer
         size_t sp_len; // size of the currently collected packet (must be kept between frames)
         size_t vcdu_insert_bytes;
         size_t PH_OFFSET_IN_CADU; //need correction for  vcdu_insert_bytes
         size_t MPDU_DATA_SIZE;//not including MPDU header (2 bytes) need correction for  vcdu_insert_bytes
          int obt_epoch;
        //
        unsigned long  c_counter_gap;
        unsigned long  c_input_frames;
        unsigned long  c_bad_headers;
        unsigned long  c_zero_sizes;
        unsigned long  c_invalid_sizes;
        unsigned long  c_invalid_header_offset;
        unsigned long c_frames_without_headers;
        t_uint32   prev_vc_count;
        vector<int> valid_sizes;
        vector<int> valid_apids;
        //
        bool has_packet_sync;

        CFrame packet_frame;
        //
        void do_frame_processing(CFrame* pf);
        bool good_sp_header(BYTE* phead);


};

#endif // CNODE_PACKETEXTRACTOR_H
