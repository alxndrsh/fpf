/*
FPF - Frame Processing Framework
See the file COPYING for copying permission.
*/
/*
CNode_EOSPdump - node  dumping header of source packets (EOS style headers in first place)

History:
    created: 2015-11 - A.Shumilin.
*/
#ifndef CNode_EOSPdump_H
#define CNode_EOSPdump_H

#include <fstream>
#include "fpf.h"

#define  FACTORY_CNODE_EOSPDUMP(c) FACTORY_ADD_NODE(c,CNode_EOSPdump)

class CNode_EOSPdump : public INode
{
    enum t_dump_formats { FORMAT_LINES_SHORT = 1 , FORMAT_LINES_LONG, FORMAT_BLOCK };

    public:
        CNode_EOSPdump();
        virtual ~CNode_EOSPdump();
        //INode
        bool init(t_ini& ini, string& name, CChain* chain);
        void start(void);
        void stop(void);
        void close(void);
        void take_frame(CFrame* pf);
    protected:
    private:
        ostream* output;
        ofstream outfile;
        string outfile_name;
        int dump_every;
        unsigned long c_dumped;
        unsigned long c_counter;
         t_dump_formats dump_format ;
         int obt_epoch;

        //
        void do_frame_processing(CFrame* pf);

};
#endif // CNode_EOSPdump_H
