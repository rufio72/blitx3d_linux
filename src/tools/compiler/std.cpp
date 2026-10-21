
#include "std.h"

std::string shellQuote( const std::string &s ){
#ifdef _WIN32
    // cmd.exe has no POSIX quoting; keep the current double-quote behaviour.
    return "\""+s+"\"";
#else
    // POSIX sh: single quotes, with embedded ' turned into '\''
    std::string out="'";
    for( size_t i=0;i<s.size();++i ){
        if( s[i]=='\'' ) out+="'\\''";
        else out+=s[i];
    }
    return out+"'";
#endif
}
