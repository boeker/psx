#ifndef PSX_CD_H
#define PSX_CD_H

#include <string>

namespace PSX {

class CD {
private:
    std::string filename;

public:
    CD(const std::string &filename);
};

}

#endif

