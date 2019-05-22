/*
FPF - Frame Processing Framework
See the file COPYING for copying permission.
*/
/*
CNode_FileWriter - node  writing frame sequence to output file

History:
    created: 2015-11 - A.Shumilin.
*/

#ifndef CNODE_FILEWRITER_H
#define CNODE_FILEWRITER_H

#include "fpf.h"
#include <stdio.h>

#define  FACTORY_CNODE_FILEWRITER(c) FACTORY_ADD_NODE(c,CNode_FileWriter)

class CNode_FileWriter : public INode
{
    public:
        CNode_FileWriter();
        virtual ~CNode_FileWriter();
        //INode
        bool init(t_ini& ini, string& name, CChain* chain);
        void start(void);
        void stop(void);
        void close(void);
        void take_frame(CFrame* pf);
    protected:
    private:
        unsigned long c_counter;
        bool lazy_create;
        size_t total_written;
        size_t chunk_size;
        size_t frames_in_chunk;
        int c_chunk;
        string file_name;
        FILE* file;
        //
        void do_frame_processing(CFrame* pf);
        bool open_file();
};
#endif // CNODE_FILEWRITER_H
