#include "cue.h"

#include <format>
#include <iomanip>

namespace util {

namespace cue {

Index::Index()
    : minutes(0),
      seconds(0),
      sectors(0) {
}

Index::Index(uint32_t minutes, uint32_t seconds, uint32_t sectors)
    : minutes(minutes),
      seconds(seconds),
      sectors(sectors) {
}

void Index::reset() {
    minutes = 0;
    seconds = 0;
    sectors = 0;
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
    return l.tie() < r.tie();
}

bool operator<=(const Index &l, const Index &r) {
    return l.tie() <= r.tie();
}

bool operator>(const Index &l, const Index &r) {
    return l.tie() > r.tie();
}

bool operator>=(const Index &l, const Index &r) {
    return l.tie() >= r.tie();
}

Index& Index::operator++() {
    ++sectors;
    handle_overflows();
    return *this;
}

Index& Index::operator+=(const Index& rhs) {
    minutes += rhs.minutes;
    seconds += rhs.seconds;
    sectors += rhs.sectors;
    handle_overflows();
    return *this;
}

Index operator+(Index lhs, const Index& rhs) {
    lhs += rhs;
    return lhs;
}

Index& Index::operator-=(const Index& rhs) {
    if (minutes < rhs.minutes) {
        minutes = 0;
        seconds = 0;
        sectors = 0;
        return *this;
    }
    minutes -= rhs.minutes;

    if (seconds < rhs.seconds) {
        seconds = 0;
        sectors = 0;
        return *this;
    }
    seconds -= rhs.seconds;

    if (sectors < rhs.sectors) {
        sectors = 0;
        return *this;
    }
    sectors -= rhs.sectors;

    return *this;
}

Index operator-(Index lhs, const Index& rhs) {
    lhs -= rhs;
    return lhs;
}

std::tuple<uint32_t, uint32_t, uint32_t> Index::tie() const {
    return std::tie(minutes, seconds, sectors);
}

void Index::handle_overflows() {
    seconds += sectors / 75;
    sectors = sectors % 75;

    minutes += seconds / 60;
    seconds = seconds % 60;
}

std::ostream& operator<<(std::ostream &os, const NumberedIndex &index) {
    return os << std::format("    INDEX {:02d} {:02d}:{:02d}:{:02d}",
                             index.number, index.minute, index.second, index.sector);
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

    line_stream >> index.minute;
    char colon;
    line_stream >> colon;
    if (colon != ':') {
        exit_with_parsing_error(std::format("Unexpected character: Expected ':', but got '{}'", colon));
    }
    line_stream >> index.second;
    line_stream >> colon;
    if (colon != ':') {
        exit_with_parsing_error(std::format("Unexpected character: Expected ':', but got '{}'", colon));
    }
    line_stream >> index.sector;

    read_line();

    return index;
}

}

}
