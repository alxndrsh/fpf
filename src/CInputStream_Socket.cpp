#include <cstdlib> /* strtol */
#include <string.h>
#include <unistd.h>
#include <cerrno>
#include <sys/types.h>
#include <winsock2.h>

#ifdef HAS_SOCKET_H
#include <netinet/in.h>
#include <netdb.h>	//hostent
#include <sys/socket.h>
#include <arpa/inet.h>
#endif // HAS_SOCKET_H

#include "CInputStream_Socket.h"

#define MY_CLASS_NAME   "CInputStream_Socket"

#define INI_START_OFFSET    "start_offset"
#define INI_BYTES_TO_READ   "read_bytes"
#define INI_PORT   "port"
#define INI_FORKING  "forking"

#define INI_SUBS_CLIENT_IP  "CLIENTIP"
#define INI_SUBS_CLIENT_DN  "CLIENTDN"
#define INI_SUBS_WALLCLOCK  "WALLCLOCK"
#define INI_SUBS_NAME       "NAME"


#define DEFAULT_PORT    5572

static int g_reuseaddr = 1; /* Reuse socket address */
const int NO_SOCKET = -1; //invalid socket value

CInputStream_Socket::CInputStream_Socket()
{
    stream_pos = 0;
    start_offset = 0;
    bytes_to_read = 0;
    read_total = 0;
    port = DEFAULT_PORT;
    accepted_socket = NO_SOCKET;
    listen_socket = NO_SOCKET;
    do_forking = false;
    url = "NET_%CLIENTIP%";
    id = "%CLIENTIP%";
}

CInputStream_Socket::~CInputStream_Socket()
{
    if (accepted_socket != NO_SOCKET) { ::close(accepted_socket); accepted_socket = NO_SOCKET;}
    if (listen_socket != NO_SOCKET) { ::close(listen_socket); listen_socket = NO_SOCKET;}
}

bool CInputStream_Socket::init(t_ini& ini, string& init_name)
{
     *fpf_debug << "~ " MY_CLASS_NAME "::init("<<init_name<<")\n";
    name = init_name;
    fpf_last_error.clear();
    is_initialized = false;
    if (accepted_socket) { ::close(accepted_socket); accepted_socket = NO_SOCKET;}
    // figure out a file name
    if (ini.find(init_name) == ini.end())
    {
        *fpf_error<<"ERROR: no ini section for InputStream ["<<init_name<<"]\n";
        return false;
    }
    t_ini_section conf = ini[init_name];
    //
    if ( conf[INI_FORKING] == INI_COMMON_VAL_YES) {do_forking = true;}
    if (conf.find(INI_START_OFFSET) != conf.end())
    {
        string s = conf[INI_START_OFFSET];
        start_offset = strtoll(s.c_str(),NULL,10);
        if (s.find_first_of("kK") != string::npos) { start_offset *= 1024; }
        else { if (s.find_first_of('M') != string::npos) { start_offset *= 1024*1024; } }
    }
    if (conf.find(INI_BYTES_TO_READ) != conf.end())
    {
        string s = conf[INI_BYTES_TO_READ];
        bytes_to_read = strtoll(s.c_str(),NULL,10);
        if (s.find_first_of("kK") != string::npos) { bytes_to_read *= 1024; }
        else { if (s.find_first_of('M') != string::npos) { bytes_to_read *= 1024*1024; } }
    }
    //
    if (conf.find(INI_PORT) != conf.end())
    {
        port = strtol(conf[INI_PORT].c_str(),NULL,10);
    }
    // -- choose an input file name
    if (conf.find(INI_COMMON_ID) != conf.end())
    {
        id = conf[INI_COMMON_ID];
    }
    if (conf.find(INI_COMMON_URL) != conf.end())
    {
        url = conf[INI_COMMON_URL];
    }
    // open and bind socket
    struct sockaddr_in serv_addr;
    listen_socket = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR, &g_reuseaddr, sizeof(g_reuseaddr));
#ifdef SO_REUSEPORT
    setsockopt(listen_socket, SOL_SOCKET, SO_REUSEPORT, &g_reuseaddr, sizeof(g_reuseaddr));
#endif
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(port);
     if (bind(listen_socket, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
     {
         *fpf_error<<"ERROR: " MY_CLASS_NAME "::init("<<name<<") socket bind on port "<<port<<" failed, errno= "<<errno<<"\n";
         return false;
     }
     listen(listen_socket,5);
    //

    read_total = 0;
    stream_pos = 0;
    //
     *fpf_trace<<"=> " MY_CLASS_NAME "::init("<<name<<") input socket opened on port "<<port<<"\n";
    is_initialized = true;
    return true;
}

void CInputStream_Socket::start(void)
{
}

void CInputStream_Socket::stop(void)
{
}

void CInputStream_Socket::close(void)
{
    if (accepted_socket != NO_SOCKET) { ::close(accepted_socket); accepted_socket = NO_SOCKET;}
    if (listen_socket != NO_SOCKET) { ::close(listen_socket); listen_socket = NO_SOCKET;}
    is_initialized = false;
    *fpf_trace<<"<= " MY_CLASS_NAME "("<<name<<") closed, "<<read_total<<" bytes readed\n";
}

#if USE_FORK
#include <signal.h>
#include <sys/wait.h>
static void wait_for_child(int sig)
{/* Signal handler to reap zombie processes */
    while (waitpid(-1, NULL, WNOHANG) > 0);
}
#endif // USE_FORK

unsigned int CInputStream_Socket::read(BYTE* pbuff, size_t read_bytes, int& ierror)
{
    FPF_ASSERT((pbuff != NULL),"CInputStream_Socket::read to NULL buffer");
    FPF_ASSERT((listen_socket != NO_SOCKET),"CInputStream_Socket::read called with uninitialized server socket");
    //
    if (accepted_socket == NO_SOCKET) // if not yet connectrd, wait for the client to connect
    {
        struct sockaddr_in cli_addr;
        socklen_t size_addr = sizeof(struct sockaddr_in);

ACCEPT_CONNECTION:
        *fpf_trace << "= " MY_CLASS_NAME "("<<name<<") listening on port "<<port <<"...\n";
        accepted_socket = accept(listen_socket, (struct sockaddr*)&cli_addr, &size_addr);
        if (accepted_socket == -1) {
            *fpf_error << "ERROR: " MY_CLASS_NAME "("<<name<<") socket failes to accept connection\n";
            ierror = 1;
            return 0;
        }
        //got new client connection...
        time_t tt = time(NULL);
        char sz_conn_ts[40]; tm* ptm = gmtime(&tt);
        strftime (sz_conn_ts,sizeof(sz_conn_ts),"%H:%M:%S",ptm);
        //
        client_address = inet_ntoa(cli_addr.sin_addr);
        struct hostent *phostent;
        phostent = gethostbyaddr(&(cli_addr.sin_addr), sizeof(cli_addr.sin_addr), AF_INET);
        client_dn = phostent->h_name;
        //

        build_names(); // update id and url
        *fpf_info << "= "<<sz_conn_ts<<" = " MY_CLASS_NAME "("<<name<<") accepted connection from "<<client_dn<<"("<<client_address <<":"<<htons(cli_addr.sin_port)<<")\n";

        if (do_forking)
        {
#if USE_FORK
#include <unistd.h>
#include <signal.h>
        /* Set up the signal handler */
        struct sigaction sa;
        sa.sa_handler = wait_for_child;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = SA_RESTART;
        sigaction(SIGCHLD, &sa, NULL);
        //
        int fork_pid;
        fork_pid = fork();
        if (fork_pid == 0) // In child process
        {
            ::close(listen_socket); listen_socket = NO_SOCKET;
            *fpf_info <<  "= " MY_CLASS_NAME "("<<name<<") forked child process, pid="<<getpid()<<"\n";
            //and proceed further with reading from client_socket
        }
        else // Parent process - return to accepting new connections
        {
            if (fork_pid == -1)
            {
                *fpf_error << "ERROR: " MY_CLASS_NAME "("<<name<<") fork new process failed, err="<<errno<"\n";
                //ierror = 1;
                return 0;
            }
            else
            {
                ::close(accepted_socket); accepted_socket = NO_SOCKET;
                *fpf_info <<  "= " MY_CLASS_NAME "("<<name<<") main process resumes after forking child="<<fork_pid<<"\n";
                goto ACCEPT_CONNECTION;
            }
        }
#else //no forking
        static bool warning_printed = false;
        if (!warning_printed)
        {
            *fpf_warn << "WARNING :" MY_CLASS_NAME "("<<name<<") forking is not compiled in, using single connection mode\n\n";
            warning_printed = true;
        }

#endif // USE_FORK
        }//if (do_forking)


    }//accept connection
    //--- check limit on data to read
    if ((bytes_to_read > 0) && (read_total > bytes_to_read))
    {
        *fpf_warn<<"\n!! " MY_CLASS_NAME "("<<name<<")::read reached limit of bytes to read ("<<bytes_to_read<<")\n\n";
        return 0;
    }
    //
    int readed = ::read(accepted_socket, pbuff, read_bytes);
    if (readed < 0)
    {//Read error
       *fpf_warn<<"ERROR: " MY_CLASS_NAME "("<<name<<")::read socket read failed, errno="<<errno<<"\n";
        ierror = 2;
        return 0;
    }
    else if (readed == 0)  return 0; //end of data stream
    else  //read data block
    {
        read_total += readed;
        ierror = 0;
        return readed;
    }
    //
}

void CInputStream_Socket::build_names() //resolve id and url, call delayed till client IP will be available
{
    map<string,string> subs_map;
    subs_map[INI_SUBS_NAME] = name;
    subs_map[INI_SUBS_CLIENT_IP] = client_address;
    subs_map[INI_SUBS_CLIENT_DN] = client_dn;
    time_t tt = time(NULL);
    char sz[40]; tm* ptm = gmtime(&tt);
    strftime (sz,sizeof(sz),"%Y%m%d%H%M%S",ptm);
    subs_map[INI_SUBS_WALLCLOCK] = string(sz);
    //
    make_substitutions(id, &subs_map,'%');
    make_substitutions(url, &subs_map,'%');
}
