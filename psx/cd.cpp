#include "cd.h"

#include "util/log.h"

namespace PSX {

CD::CD(const std::string &filename)
    : filename(filename) {
    LOG_CDROM(std::format("Opening CD image \"{:s}\"", filename));
}

}

