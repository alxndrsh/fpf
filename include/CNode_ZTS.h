/*
FPF - Frame Processing Framework
See the file COPYING for copying permission.
*/
/*
CNode_ZTS -  node to append Zodiac timestamps to the TF

*/

#ifndef CNODE_ZTS_H
#define CNODE_ZTS_H

#include "fpf.h"

#define  FACTORY_CNODE_ZTS(c) FACTORY_ADD_NODE(c,CNode_ZTS)

struct PACKED_GCC ZTS { //Zodiac style timestamp, 16 bytes
    uint32_t  seconds;
    uint32_t  mkseconds;
    uint32_t    w1; //not yet known fields
    uint32_t    w2;
};

class CNode_ZTS : public INode
{
    public:
        CNode_ZTS();
        virtual ~CNode_ZTS();
        //INode
        bool init(t_ini& ini, string& name, CChain* chain);
        void start(void);
        void stop(void);
        void close(void);
        void take_frame(CFrame* pf);
    protected:
    private:
        unsigned long c_counter;
        CFrame new_frame; ///new frame structure to keep new output frame (is size extention takes place)
        CFrame* p_output_frame; /// pointer to output frame (either input one or new one)
        uint8_t *pframe_data;
        string str_base_time;
        time_t base_t;
        time_t base_utime;
        int base_mksec;
        int frame_period; ///microseconds per frame
        bool set_timestamp ; /// set new timestamp values, otherwise this is just the ts dump utility
        int dump_every;
        int base_year;
        size_t zts_offset;
        bool f_update_actime;
        //
        void do_frame_processing(CFrame* pf);

};
#endif // CNODE_ZTS_H
