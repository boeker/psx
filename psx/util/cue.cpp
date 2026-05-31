#include "cue.h"

#include <cassert>
#include <format>
#include <iomanip>

namespace util {

namespace cue {

Index::Index()
    : minutes(0),
      seconds(0),
      sectors(0) {
}

Index::Index(uint8_t minutes, uint8_t seconds, uint8_t sectors)
    : minutes(minutes),
      seconds(seconds),
      sectors(sectors) {
    // Do not enforce a bound on minutes
    assert(seconds < 60);
    assert(sectors < 75);
}

Index::Index(uint32_t total_sectors)
    : minutes((total_sectors / 75) / 60),
      seconds((total_sectors / 75) % 60),
      sectors(total_sectors % 75) {
}

uint32_t Index::total_sectors() const {
    uint32_t total = minutes;
    total *= 60;
    total += seconds;
    total *= 75;
    total += sectors;
    return total;
}

bool operator==(const Index &l, const Index &r) {
    return (l.minutes == r.minutes)
           && (l.seconds == r.seconds)
           && (l.sectors == r.sectors);
}

bool operator!=(const Index &l, const Index &r) {
    return !(l == r);
}

bool operator<(const Index &l, const Index &r) {
    return l.total_sectors() < r.total_sectors();
}

bool operator<=(const Index &l, const Index &r) {
    return l.total_sectors() <= r.total_sectors();
}

bool operator>(const Index &l, const Index &r) {
    return !(l <= r);
}

bool operator>=(const Index &l, const Index &r) {
    return !(l < r);
}

Index& Index::operator++() {
    *this = Index(total_sectors() + 1);
    return *this;
}

Index& Index::operator+=(const Index& rhs) {
    *this = Index(total_sectors() + rhs.total_sectors());
    return *this;
}

Index operator+(Index lhs, const Index& rhs) {
    lhs += rhs;
    return lhs;
}

Index& Index::operator-=(const Index& rhs) {
    uint32_t ts = total_sectors();
    uint32_t rhs_ts = rhs.total_sectors();
    *this = Index(rhs_ts > ts ? 0 : (ts - rhs_ts));
    return *this;
}

Index operator-(Index lhs, const Index& rhs) {
    lhs -= rhs;
    return lhs;
}

std::ostream& operator<<(std::ostream &os, const NumberedIndex &index) {
    return os << std::format("    INDEX {:02d} {:02d}:{:02d}:{:02d}",
                             index.number, index.index.minutes, index.index.seconds, index.index.sectors);
}

std::ostream& operator<<(std::ostream &os, const Track &track) {
    os << std::format("  TRACK {:02d} {:s}",
                      track.number, track.mode == Track::Mode::AUDIO ? "AUDIO" : "MODE2/2352");
    os << std::endl;
    bool first = true;
    for (const NumberedIndex &index : track.indexes) {
        if (first) {
            first = false;
        } else {
            os << std::endl;
        }
        os << index;
    }
    return os;
}

std::ostream& operator<<(std::ostream &os, const File &file) {
    os << std::format("FILE \"{:s}\" BINARY", file.filename);
    os << std::endl;
    bool first = true;
    for (const Track &track : file.tracks) {
        if (first) {
            first = false;
        } else {
            os << std::endl;
        }
        os << track;
    }
    return os;
}

std::ostream& operator<<(std::ostream &os, const Sheet &sheet) {
    bool first = true;
    for (const File &file : sheet.files) {
        if (first) {
            first = false;
        } else {
            os << std::endl;
        }
        os << file;
    }
    return os;
}

Sheet Parser::parse(const std::string &filename) {
    Parser parser(filename);
    return parser.parse();
}

Parser::Parser(const std::string &filename)
    : filename(filename),
      file(filename),
      line(),
      line_num(0) {
}

Sheet Parser::parse() {
    read_line();
    std::vector<File> files = parse_block<File>();
    if (!eof()) {
        exit_with_parsing_error(std::format("Parsing ended before end of file"));
    }
    return { files };
}

void Parser::read_line() {
    while (true) {
        line.clear();
        if (!std::getline(file, line)) {
            if (file.bad()) {
                throw exceptions::FileReadError("I/O error while reading \"" + filename + "\"");
            }
        }
        ++line_num;

        if (!std::all_of(line.begin(), line.end(), isspace) || file.eof()) {
            break;
        }
    }
    line_stream.str(line);
    command.clear();
    line_stream >> command;
}

bool Parser::eof() {
    return file.eof();
}

void Parser::assert_command(const std::string& expected) {
    if (command != expected) {
        exit_with_parsing_error(std::format("Unexpected command: Expected \"{:s}\", but got \"{:s}\"", expected, command));
    }
}

void Parser::exit_with_parsing_error(const std::string &message) {
    throw exceptions::FileReadError(std::format("Parsing error in line {:d}: {:s}: {:s}", line_num, message, line));
}

template<typename T>
std::vector<T> Parser::parse_block() {
    std::vector<T> commands;

    while (true) {
        if (eof()) {
            break;
        }
        if (command != T::COMMAND) {
            break;
        }
        commands.emplace_back(std::move(parse_command<T>()));
    }

    return commands;
}

template<>
File Parser::parse_command() {
    std::string filename;
    line_stream >> std::quoted(filename);

    std::string type;
    //line_stream >> std::quoted(type);
    line_stream >> type;

    if (type != "BINARY") {
        exit_with_parsing_error(std::format("Unexpected type: Expected \"BINARY\", but got \"{:s}\"", type));
    }

    File::Type file_type = File::Type::BINARY;

    read_line();

    std::vector<Track> tracks = parse_block<Track>();

    return { filename, file_type, tracks };
}

template<>
Track Parser::parse_command() {
    Track track;

    // TODO Sanitize
    line_stream >> track.number;

    std::string mode;
    line_stream >> mode;

    if (mode == "AUDIO") {
        track.mode = Track::Mode::AUDIO;

    } else if (mode == "MODE2/2352") {
        track.mode = Track::Mode::MODE2_2352;

    } else {
        exit_with_parsing_error(std::format("Unexpected type: Expected \"AUDIO\" or \"MODE2/2352\", but got \"{:s}\"", mode));
    }

    read_line();

    track.indexes = std::move(parse_block<NumberedIndex>());

    return track;
}

template<>
NumberedIndex Parser::parse_command() {
    NumberedIndex index;

    // TODO Sanitize
    line_stream >> index.number;

    uint32_t temp;
    line_stream >> temp;
    index.index.minutes = temp;

    char colon;
    line_stream >> colon;
    if (colon != ':') {
        exit_with_parsing_error(std::format("Unexpected character: Expected ':', but got '{}'", colon));
    }

    line_stream >> temp;
    index.index.seconds = temp;

    line_stream >> colon;
    if (colon != ':') {
        exit_with_parsing_error(std::format("Unexpected character: Expected ':', but got '{}'", colon));
    }
    line_stream >> temp;
    index.index.sectors = temp;

    read_line();

    return index;
}

}

}
