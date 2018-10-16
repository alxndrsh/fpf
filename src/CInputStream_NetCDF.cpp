/*
FPF - Frame Processing Framework
See the file COPYING for copying permission.
*/
/*
InputStream_NetCDF. - Input Stream reading bitstream from a NetCDF Variable

History:
    created: 2018-10 - Stefan Codrescu
*/
#include <stdio.h>
#include <cerrno>
#include <unistd.h>
#include <cstdlib> /* strtol */
#include <cstring> /* strerror */
#include <vector>
#include <algorithm>    // std::min
#include "CInputStream_NetCDF.h"

#define MY_CLASS_NAME   "CInputStream_NetCDF"

#define INI_NC_VAR_NAME "netcdf_variable"

CInputStream_NetCDF::CInputStream_NetCDF() {
    ncid = -1;
    varid = -1;
    url = "";
    stream_pos = 0;
    read_total = 0;
}

CInputStream_NetCDF::~CInputStream_NetCDF() {
    if (ncid >= 0) { nc_close(ncid); ncid = -1;}
}

bool CInputStream_NetCDF::init(t_ini& ini, string& init_name) {
     *fpf_debug << "~ " MY_CLASS_NAME "::init("<<init_name<<")\n";
    name = init_name;
    fpf_last_error.clear();
    is_initialized = false;
    // figure out a file name
    if (ini.find(init_name) == ini.end()) {
        *fpf_error<<"ERROR: no ini section for InputStream ["<<init_name<<"]\n";
        return false;
    }
    t_ini_section conf = ini[init_name];
    // -- choose an input netcdf file name
    if (conf.find(INI_COMMON_INPUT_FILE) != conf.end()) {
        file_name = conf[INI_COMMON_INPUT_FILE];
    } else {
        *fpf_error<<"ERROR: no input file name for InputStream ["<<init_name<<"]\n";
        return false;
    }
    // -- choose variable name to read from input netcdf
    if (conf.find(INI_NC_VAR_NAME) != conf.end()) {
        nc_var_name = conf[INI_NC_VAR_NAME];
    } else {
        *fpf_error<<"ERROR: netcdf variable name not specified for InputStream ["<<init_name<<"]\n";
        return false;
    }

    //-- open input netcdf file: 
    // https://www.unidata.ucar.edu/software/netcdf/docs/group__datasets.html#ga44b4199fe8cabca419e1cf5f3a169353
    int status = nc_open(file_name.c_str(), NC_NOWRITE, &ncid);
    if (status != NC_NOERR) {
        *fpf_error<<"ERROR: CInputStream_NetCDF::init("<<name<<")  netcdf file open failed" 
            << "\n   File:" <<file_name <<"\n   Error [" << status <<"]:" << nc_strerror(status) << endl;
        return false;
    }
    // ensure that the variable exists:
    status = nc_inq_varid(ncid, nc_var_name.c_str(), &varid);
    if (status != NC_NOERR) {
        *fpf_error<<"ERROR: CInputStream_NetCDF::init("<<name<<") error with netcdf variable " << nc_var_name 
            << "\n   File:" <<file_name <<"\n   Error [" << status <<"]:" << nc_strerror(status) << endl;
        return false;
    }

    id = file_name; //strip off the path and extension
    size_t s_pos = id.find_last_of("\\/"); if (string::npos != s_pos) {  id.erase(0, s_pos + 1); }
    s_pos = id.find_last_of("."); if (string::npos != s_pos) {  id.erase(s_pos); }
    url = file_name;
    read_total = 0;
    stream_pos = 0;
    is_initialized = true;
    return true;
}

void CInputStream_NetCDF::start(void) { } 

void CInputStream_NetCDF::stop(void) { }

void CInputStream_NetCDF::close(void) {
    if (ncid >= 0) { nc_close(ncid); ncid = -1;}
    is_initialized = false;
    *fpf_trace<<"<= " MY_CLASS_NAME "("<<name<<") closed, "<<read_total<<" bytes read\n";
}


unsigned int CInputStream_NetCDF::read(BYTE* pbuff, size_t read_bytes, int& ierror) {
    FPF_ASSERT((ncid >= 0),"CInputStream_NetCDF::read, netcdf not opened");
    FPF_ASSERT((pbuff != NULL),"CInputStream_NetCDF::read to NULL buffer");

    // inquire for the dimensions of the variable, only support 1D variable...
    // ie. the variable should just be a 1D array of bytes.
    int ndims;
    int status = nc_inq_varndims(ncid, varid, &ndims);
    if (status != NC_NOERR) {
        *fpf_error << "ERROR: " MY_CLASS_NAME "("<<name<<") error finding netcdf dimensions for" << nc_var_name 
            << "\n   File:" << file_name << "\n   Error ["<<status<<"]:" << nc_strerror(status) << endl;
        ierror = 1;
        return 0;
    }
    FPF_ASSERT((ndims == 1), "CInputStream_NetCDF::read assumes variable has 1 dimension");

    // get the size of the dimension, will have to read from stream_pos up to
    // either read_bytes or dims_size, whichever is smaller.
    int dimid;
    status = nc_inq_vardimid(ncid, varid, &dimid);
    if (status != NC_NOERR) {
        *fpf_error<<"ERROR: " MY_CLASS_NAME "("<<name<<") error finding netcdf dimension id" << nc_var_name 
            << "\n   File:" << file_name << "\n   Error ["<<status<<"]:" << nc_strerror(status) << endl;
        ierror = 1;
        return 0;
    }
    size_t dimlen;
    status = nc_inq_dimlen(ncid, dimid, &dimlen);
    if (status != NC_NOERR) {
        *fpf_error<<"ERROR: " MY_CLASS_NAME "("<<name<<") error reading netcdf dimension len" << nc_var_name 
            << "\n   File:" << file_name << "\n   Error ["<<status<<"]:" << nc_strerror(status) << endl;
        ierror = 1;
        return 0;
    }

    // remaining to read is (dimlen - stream_pos), can read up to read_bytes
    size_t start[] = {stream_pos};
    size_t count[] = {std::min(dimlen - stream_pos, read_bytes)};
    
    //size_t readed = fread (pbuff,1,read_bytes,file);
    status = nc_get_vara(ncid, varid, start, count, pbuff);
    if ( status != NC_NOERR) {
        *fpf_error<<"ERROR: " MY_CLASS_NAME "("<<name<<") error reading netcdf variable values for" << nc_var_name 
            << "\n   File:" << file_name << "\n   Error ["<<status<<"]:" << nc_strerror(status) << endl;
        ierror = 1;
        return 0;
    }
        
    read_total += count[0];
    stream_pos += count[0];
    ierror = 0;
    *fpf_trace << "NETCDF returning << " << count[0] << ", start: " << start[0] << ", stream_pos: " << stream_pos << "\n" ;
    return count[0];
}

