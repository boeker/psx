#ifndef UTIL_CUE_H
#define UTIL_CUE_H

#include <cstdint>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>

#include "exceptions/exceptions.h"

namespace util {

namespace cue {

struct Index {
    static constexpr std::string COMMAND = "INDEX";

    uint32_t number;
    uint32_t minute;
    uint32_t second;
    uint32_t sector;
};

struct Track {
    static constexpr std::string COMMAND = "TRACK";
    enum Mode {
        AUDIO,
        MODE2_2352
    };

    uint32_t number;
    Mode mode;
    std::vector<Index> indexes;
};

struct File {
    static constexpr std::string COMMAND = "FILE";
    enum Type {
        BINARY
    };

    std::string filename;
    Type type;
    std::vector<Track> tracks;
};

struct Sheet {
    std::vector<File> files;
};

std::ostream& operator<<(std::ostream &os, const Index &index);
std::ostream& operator<<(std::ostream &os, const Track &track);
std::ostream& operator<<(std::ostream &os, const File &file);
std::ostream& operator<<(std::ostream &os, const Sheet &sheet);

class Parser {
private:
    const std::string filename;
    std::ifstream file;
    std::string line;
    std::istringstream line_stream;
    std::string command;
    uint32_t line_num;

public:
    Parser(const std::string &filename);
    Sheet parse();

private:
    void read_line();
    bool eof();
    void assert_command(const std::string& expected);
    void exit_with_parsing_error(const std::string &message);

    template<typename T> std::vector<T> parse_block();
    template<typename T> T parse_command();
};

template<> File Parser::parse_command<File>();
template<> Track Parser::parse_command<Track>();
template<> Index Parser::parse_command<Index>();

}

}

#endif

