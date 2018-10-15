/*
FPF - Frame Processing Framework
See the file COPYING for copying permission.
*/
/*
InputStream_File. - Input Stream reading bitstream from a file

History:
    created: 2018-10 - Stefan Codrescu.
*/
#ifndef CINPUTSTREAM_NETCDF_H
#define CINPUTSTREAM_NETCDF_H

#include <stdio.h>
#include "fpf.h"
#include "class_factory.h"

#ifndef USE_NETCDF

// dummy factory to work without the object if compiled without netcdf support.
#define  FACTORY_CINPUTSTREAM_NETCDF(c) 
#else // USE_NETCDF

// Support compile without NetCDF
#include "netcdf.h"
#define  FACTORY_CINPUTSTREAM_NETCDF(c) FACTORY_ADD_INPUTSTREAM(c,CInputStream_NetCDF)

class CInputStream_NetCDF : public IInputStream
{
    public:
        CInputStream_NetCDF();
        virtual ~CInputStream_NetCDF();
        bool init(t_ini& ini, string& name);
        void start(void);
        void stop(void);
        void close(void);
        //frame processing
        unsigned int read(BYTE* pbuff, size_t bytes_to_read, int& ierror);
    protected:
    private:
        string file_name;
        int ncid, varid;
        size_t  read_total;
        string nc_var_name;
};

#endif // USE_NETCDF
#endif // CINPUTSTREAM_NETCDF_H
