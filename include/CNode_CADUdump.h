/*
FPF - Frame Processing Framework
See the file COPYING for copying permission.
*/
/*
CNode_CADUdump - dumper of TF (CADU) frame structures

History:
    created: 2015-11 - A.Shumilin.
*/

#ifndef CNODE_CADUDUMP_H
#define CNODE_CADUDUMP_H
#include <fstream>
#include "fpf.h"

#define  FACTORY_CNODE_CADUDUMP(c) FACTORY_ADD_NODE(c,CNode_CADUdump)


class CNode_CADUdump : public INode
{
enum t_dump_formats { FORMAT_LINES_SHORT = 1 , FORMAT_LINES_LONG, FORMAT_BLOCK };

    public:
        CNode_CADUdump();
        virtual ~CNode_CADUdump();
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
        int trace_every;
        unsigned long c_counter;
        t_dump_formats dump_format ;
        bool use_hex;
        bool dump_times;
        int tf_version;
        string outfile_name;
        unsigned long c_dumped;
        //
        void do_frame_processing(CFrame* pf);

};
#endif // CNODE_CADUDUMP_H
