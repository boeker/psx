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

Index::Index(uint8_t minutes, uint8_t seconds, uint8_t sectors)
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
    set_and_handle_overflows(minutes, seconds, sectors + 1);
    return *this;
}

Index& Index::operator+=(const Index& rhs) {
    set_and_handle_overflows(static_cast<int32_t>(minutes) + rhs.minutes,
                             static_cast<int32_t>(seconds) + rhs.seconds,
                             static_cast<int32_t>(sectors) + rhs.sectors);
    return *this;
}

Index operator+(Index lhs, const Index& rhs) {
    lhs += rhs;
    return lhs;
}

Index& Index::operator-=(const Index& rhs) {
    set_and_handle_underflows(static_cast<int32_t>(minutes) - rhs.minutes,
                              static_cast<int32_t>(seconds) - rhs.seconds,
                              static_cast<int32_t>(sectors) - rhs.sectors);
    return *this;
}

Index operator-(Index lhs, const Index& rhs) {
    lhs -= rhs;
    return lhs;
}

std::tuple<uint8_t, uint8_t, uint8_t> Index::tie() const {
    return std::tie(minutes, seconds, sectors);
}

void Index::set_and_handle_overflows(int32_t minutes, int32_t seconds, int32_t sectors) {
    seconds += sectors / 75;
    sectors = sectors % 75;

    minutes += seconds / 60;
    seconds = seconds % 60;

    this->minutes = minutes;
    this->seconds = seconds;
    this->sectors = sectors;
}

void Index::set_and_handle_underflows(int32_t minutes, int32_t seconds, int32_t sectors) {
    if (sectors < 0) {
        sectors += 75;
        --seconds;
    }

    if (seconds < 0) {
        seconds += 60;
        --minutes;
    }

    if (minutes < 0) {
        minutes += 74;
    }

    this->minutes = minutes;
    this->seconds = seconds;
    this->sectors = sectors;
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
