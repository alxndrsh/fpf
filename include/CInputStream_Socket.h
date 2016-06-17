#ifndef CINPUTSTREAM_SOCKET_H
#define CINPUTSTREAM_SOCKET_H

#ifdef NO_SOCKETS
// this is the dummy staff to work without this object at all

#include "fpf.h"
#include "class_factory.h"

#define  FACTORY_CINPUTSTREAM_SOCKET(c)

#else //NO_SOCKETS

#include "fpf.h"
#include "class_factory.h"

#define  FACTORY_CINPUTSTREAM_SOCKET(c) FACTORY_ADD_INPUTSTREAM(c,CInputStream_Socket)

class CInputStream_Socket : public IInputStream
{
    public:
        CInputStream_Socket();
        virtual ~CInputStream_Socket();
        bool init(t_ini& ini, string& name);
        void start(void);
        void stop(void);
        void close(void);
        //frame processing
        unsigned int read(BYTE* pbuff, size_t bytes_to_read, int& ierror);
    protected:
    private:
        string file_name;

        size_t  read_total;
        size_t  start_offset;
        size_t bytes_to_read;

        int listen_socket;
        int accepted_socket;
        int port;
        string client_address;
        string client_dn;
        bool do_forking;

        void build_names(void);

};

#endif //NO_SOCKETS
#endif // CINPUTSTREAM_SOCKET_H
