#ifndef UTIL_CUE_H
#define UTIL_CUE_H

#include <cstdint>
#include <format>
#include <fstream>
#include <iomanip>
#include <string>
#include <sstream>
#include <vector>

#include "exceptions/exceptions.h"

namespace util {

namespace cue {

struct Index {
    uint32_t number;
    uint32_t minute;
    uint32_t second;
    uint32_t sector;
};

std::ostream& operator<<(std::ostream &os, const Index &index) {
    return os << std::format("    INDEX {:02d} {:02d}:{:02d}:{:02d}",
                             index.number, index.minute, index.second, index.sector);
}

struct Track {
    enum Mode {
        AUDIO,
        MODE2_2352
    };

    uint32_t number;
    Mode mode;
    std::vector<Index> indexes;
};

std::ostream& operator<<(std::ostream &os, const Track &track) {
    os << std::format("  TRACK {:02d} {:s}",
                      track.number, track.mode == Track::Mode::AUDIO ? "AUDIO" : "MODE2/2352");
    os << std::endl;
    bool first = true;
    for (const Index &index : track.indexes) {
        if (first) {
            first = false;
        } else {
            os << std::endl;
        }
        os << index;
    }
    return os;
}

struct File {
    enum Type {
        BINARY
    };

    std::string filename;
    Type type;
    std::vector<Track> tracks;
};

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

struct Sheet {
    std::vector<File> files;
};

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

class Parser {
private:
    const std::string filename;
    std::ifstream file;
    std::string line;
    std::istringstream line_stream;
    std::string command;
    uint32_t line_num;

public:
    Parser(const std::string &filename)
        : filename(filename),
          file(filename),
          line(),
          line_num(0) {
    }

    Sheet parse() {
        read_line();
        Sheet sheet = parse_files();
        return sheet;
    }

private:
    void read_line() {
        if (!std::getline(file, line)) {
            if (file.bad()) {
                throw exceptions::FileReadError("I/O error while reading \"" + filename + "\"");
            }
        }
        ++line_num;
        line_stream.str(line);
        line_stream >> command;
    }

    bool eof() {
        return file.eof();
    }

    void assert_command(const std::string& expected) {
        if (command != expected) {
            exit_with_parsing_error(std::format("Unexpected command: Expected \"{:s}\", but got \"{:s}\"", expected, command));
        }
    }

    void exit_with_parsing_error(const std::string &message) {
        throw exceptions::FileReadError(std::format("Parsing error in line {:d}: {:s}: {:s}", line_num, message, line));
    }

    Sheet parse_files() {
        std::vector<File> files;

        while (true) {
            if (eof()) {
                break;
            }
            assert_command("FILE");
            files.emplace_back(std::move(parse_file()));
        }

        return {files};
    }

    File parse_file() {
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

        std::vector<Track> tracks = parse_tracks();

        return { filename, file_type, tracks };
    }

    std::vector<Track> parse_tracks() {
        std::vector<Track> tracks;

        while (true) {
            if (eof()) {
                break;
            }
            //assert_command("TRACK");
            if (command != "TRACK") {
                break;
            }
            tracks.emplace_back(std::move(parse_track()));
        }

        return tracks;
    }

    Track parse_track() {
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

        track.indexes = std::move(parse_indexes());

        return track;
    }

    std::vector<Index> parse_indexes() {
        std::vector<Index> indexes;

        while (true) {
            if (eof()) {
                break;
            }
            //assert_command("INDEX");
            if (command != "INDEX") {
                break;
            }
            indexes.emplace_back(std::move(parse_index()));
        }

        return indexes;
    }

    Index parse_index() {
        Index index;

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
};

}

}

#endif

