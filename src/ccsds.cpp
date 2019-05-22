#include "ccsds.h"

const std::map<int, string>::value_type x[] =
{
    std::make_pair( 11, "METOPA" ),
    std::make_pair( 12, "METOPB" ),
    std::make_pair( 13, "METOPC" ),
    std::make_pair( 42, "TERRA" ),
    std::make_pair( 154, "AQUA" )
};
std::map<int, string> SPACECRAFT_NAMES(x, x + sizeof x / sizeof x[0]);


const std::map<int, string>::value_type vcidt[] =
{
    std::make_pair( 63, "FILL" ),
    std::make_pair( 42, "MODIS-T" )
};
std::map<int, string> VCID_NAMES(vcidt, vcidt + sizeof vcidt / sizeof vcidt[0]);
