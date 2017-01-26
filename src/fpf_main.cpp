/*
FPF - Frame Processing Framework
See the file COPYING for copying permission.
*/
/*
fpf_main.c - main(),  generic chain driver executable for FPF
History:
    created: 2015-11 - A.Shumilin.
*/
#include <iostream>
#include <stdlib.h>
#include <unistd.h>
//
#include <ini.h>
#include <fpf.h>
#include "class_factory.h"
//
using namespace std;

//
#define INI_MAIN_SECTION "_main_"
#define INI_MAIN_FRAMESOURCE  "frame_source"
//common substitutes
#define  PARAM_INPUT_FILE   "INPUTFILE"
#define  PARAM_OUTPUT_FILE  "OUTPUTFILE"


//exit codes
#define EXIT_ERR_CONFIG_INCOMPLETE      -1
#define EXIT_ERR_INITIALIZATION_FAILED  -2
#define EXIT_ERR_FPF_ASSERT_FAILED      EXIT_FPF_ASSERT
#define EXIT_ERR_INVALID_ARG             -4

// Globals
t_ini g_ini;
ostream stream_null(NULL);

int main(int argc, char* argv[])
{
    cerr<< "*** Frame Processing Framework engine. v 0.6. build: " << __DATE__ <<" *** "<<endl;

    string ini_fn;
    map<string,string> map_parameters;
    //------- get command line options ----------------
    int carg;
    size_t eqpos;string sx;
    while ((carg = getopt (argc, argv, "o:D:i:tcs")) != -1)
    switch (carg)
      {
      case 'i': // config ini file
        ini_fn = optarg;
        break;
      case 'o' : //output file
        map_parameters[PARAM_OUTPUT_FILE] = optarg;
      case 'D': // definition of custom parameters
        sx = optarg;
        eqpos = sx.find_first_of("=");
        if (string::npos != eqpos) // P=V line
            {
                map_parameters[sx.substr(0,eqpos)] = sx.substr(eqpos+1,sx.length()-eqpos);
            }
        else {  map_parameters[sx] = "yes"; } //take it a s a simple flag
        break;
      case '?':
        if (optopt == 'i')
          *fpf_error<<"ERROR: Option -"<<optopt<<" requires an argument.\n";
        else if (isprint (optopt))
          *fpf_error<<"ERROR: Unknown Option -"<<optopt<<"\n";
        else
          *fpf_error<<"ERROR: Unknown option character -"<<optopt<<"\n";
        return EXIT_ERR_INVALID_ARG;
      default:
         break;
      }// switch (c)
    // further arguments are taken as INPUT_FILE
    //  for (index = optind; index < argc; index++)
    if (optind < argc)
    {
        map_parameters[PARAM_INPUT_FILE] = (char*)  argv[optind];
    }

    //----------------------------------------------
    // init fpf
    fpf_last_error.clear();
    // default ini file, look by executable file name
    if (ini_fn.empty())
    {
       ini_fn = argv[0]; //take an  exe file name
        string::size_type dot = ini_fn.find_last_of("./\\"); //find the dot or slash, what is the rightmost
        if ((dot != string::npos) && (ini_fn[dot] == '.'))
        { ini_fn.erase(dot, string::npos); }
        ini_fn += ".ini";
    }
    *fpf_info<< "* Using INI config from: "<<ini_fn<<endl;

    //* read config file *
    read_config_file(ini_fn,g_ini,&map_parameters);
    ///dump_map_of_maps(g_ini,cout);
    //check _main_ ini section and accept generic settings
    if (g_ini.find(INI_MAIN_SECTION) == g_ini.end())
    {
        *fpf_error << "\n#ERROR: no [" << INI_MAIN_SECTION << "] section in the ini file\n";
        //TO DO - fill dummy main section to work further!
        // if no default here, terminate the programm
        exit(EXIT_ERR_CONFIG_INCOMPLETE);
    }//
    //Construct the root framer
    string s_root_framer = g_ini[INI_MAIN_SECTION][INI_MAIN_FRAMESOURCE];
    if (s_root_framer.empty())
    {
        *fpf_error << "\n#ERROR: no [" << INI_MAIN_FRAMESOURCE << "] set in main section to define root framer\n";
        exit(EXIT_ERR_CONFIG_INCOMPLETE);
    }
    if (g_ini.find(s_root_framer) == g_ini.end())
    {
        *fpf_error << "\n#ERROR: no [" << INI_MAIN_FRAMESOURCE << "] section to be taken as a root framer\n";
        exit(EXIT_ERR_CONFIG_INCOMPLETE);
    }
    string s_fs_class = g_ini[s_root_framer][INI_COMMON_CLASS];
    *fpf_info<<"* Using ["<<s_fs_class<<"] as a root frame source\n";
    IFrameSource* frame_source = new_frame_source(s_fs_class);
    if (NULL == frame_source)
    {
        *fpf_error<<"\n#ERROR: failed to instantiate frame souce.\n";
        exit(EXIT_ERR_CONFIG_INCOMPLETE);
    }
    // run it
    if (frame_source->init(g_ini,s_root_framer))
    {
        cout << "\n===>\n";
        frame_source->start();
        // close it after termination
        cout << "\n<===\n\n";
        frame_source->close();
        //and kill
        delete frame_source; frame_source = NULL;
    }else
    {
        *fpf_error << "\n#ERROR: framer initialization failed.\n";
        exit(EXIT_ERR_INITIALIZATION_FAILED);
    }

    //OK, bye-bye
    *fpf_info<<"* OK, normal termination.\n";
    return 0;
}
