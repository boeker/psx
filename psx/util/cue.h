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
    uint32_t minutes;
    uint32_t seconds;
    uint32_t sectors;

    Index();
    Index(uint32_t minutes, uint32_t seconds, uint32_t sectors);
    void reset();

    friend bool operator==(const Index &l, const Index &r);
    friend bool operator!=(const Index &l, const Index &r);
    friend bool operator<(const Index &l, const Index &r);
    friend bool operator<=(const Index &l, const Index &r);
    friend bool operator>(const Index &l, const Index &r);
    friend bool operator>=(const Index &l, const Index &r);

    Index& operator++();

    Index& operator+=(const Index& rhs);
    friend Index operator+(Index lhs, const Index& rhs);

    Index& operator-=(const Index& rhs);
    friend Index operator-(Index lhs, const Index& rhs);

private:
    void handle_overflows();
    std::tuple<uint32_t, uint32_t, uint32_t> tie() const;
};

struct NumberedIndex {
    static constexpr std::string COMMAND = "INDEX";

    uint32_t number;
    uint32_t minute;
    uint32_t second;
    uint32_t sector;
};

struct Track {
    static constexpr std::string COMMAND = "TRACK";
    enum class Mode {
        AUDIO,
        MODE2_2352
    };
    static constexpr const char* MODE_STRINGS[] = { "AUDIO", "MODE2/2352" };
    static const char* mode_to_string(Mode mode) { return MODE_STRINGS[(int)mode]; }

    uint32_t number;
    Mode mode;
    std::vector<NumberedIndex> indexes;
};

struct File {
    static constexpr std::string COMMAND = "FILE";
    enum class Type {
        BINARY
    };
    static constexpr const char* TYPE_STRINGS[] = { "BINARY" };
    static const char* type_to_string(Type type) { return TYPE_STRINGS[(int)type]; }

    std::string filename;
    Type type;
    std::vector<Track> tracks;
};

struct Sheet {
    std::vector<File> files;
};

std::ostream& operator<<(std::ostream &os, const NumberedIndex &index);
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
    static Sheet parse(const std::string &filename);

private:
    Parser(const std::string &filename);
    Sheet parse();

    void read_line();
    bool eof();
    void assert_command(const std::string& expected);
    void exit_with_parsing_error(const std::string &message);

    template<typename T> std::vector<T> parse_block();
    template<typename T> T parse_command();
};

template<> File Parser::parse_command<File>();
template<> Track Parser::parse_command<Track>();
template<> NumberedIndex Parser::parse_command<NumberedIndex>();

}

}

#endif

