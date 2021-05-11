/*
FPF - Frame Processing Framework
See the file COPYING for copying permission.
*/
/*
CNode_RS - RS (Reed-Solomon) decoder node

History:
    created: 2015-11 - A.Shumilin.
*/
#ifndef CNODE_RS_H
#define CNODE_RS_H

#include "fpf.h"

#define  FACTORY_CNODE_RS(c) FACTORY_ADD_NODE(c,CNode_RS)

class CNode_RS : public INode
{
    public:
        CNode_RS();
        virtual ~CNode_RS();
        //INode
        bool init(t_ini& ini, string& name, CChain* chain);
        void start(void);
        void stop(void);
        void close(void);
        void take_frame(CFrame* pf);
    protected:
    private:
        unsigned long c_counter;
        int interleaving_n;
        unsigned long c_uncorrectable;
        unsigned long c_corrected;
		unsigned long c_corrected_bits;
		unsigned long c_clean;
		unsigned long c_dropped_frames;
        bool block_uncorrected_frames;
        bool fix_data;
        //
        bool do_frame_processing(CFrame* pf);
        void UpdateFrameAttributes(CFrame* pf);


};

#endif // CNODE_RS_H
