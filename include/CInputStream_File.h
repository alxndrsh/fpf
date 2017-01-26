/*
FPF - Frame Processing Framework
See the file COPYING for copying permission.
*/
/*
InputStream_File. - Input Stream reading bitstream from a file

History:
    created: 2015-11 - A.Shumilin.
*/
#ifndef CINPUTSTREAM_FILE_H
#define CINPUTSTREAM_FILE_H

#include <stdio.h>
#include "fpf.h"
#include "class_factory.h"

#define  FACTORY_CINPUTSTREAM_FILE(c) FACTORY_ADD_INPUTSTREAM(c,CInputStream_File)

class CInputStream_File : public IInputStream
{
    public:
        CInputStream_File();
        virtual ~CInputStream_File();
        bool init(t_ini& ini, string& name);
        void start(void);
        void stop(void);
        void close(void);
        //frame processing
        unsigned int read(BYTE* pbuff, size_t bytes_to_read, int& ierror);
    protected:
    private:
        string file_name;
        FILE *file;
        size_t  read_total;
        size_t  start_offset;
        size_t bytes_to_read;
        int rt_timeout;
        bool is_rt; //switch on real-time behaviour
};

#endif // CINPUTSTREAM_FILE_H
