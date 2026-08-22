#ifndef PSX_CD_H
#define PSX_CD_H

#include <cstdint>
#include <fstream>
#include <memory>
#include <string>

#include "util/cue.h"

namespace PSX {

#define CD_MODE2_SYNC_BYTES 0xC
#define CD_MODE2_HEADER 0x4
#define CD_MODE2_SUB_HEADER 0x4
#define CD_MODE2_SUB_HEADER_COPY 0x4

#define CD_MODE2_HEADER_OFFSET 0xC
#define CD_MODE2_DATA_OFFSET 0x18

#define CD_SECTORS_PER_SECOND 75
#define CD_TWO_SECONDS 2 * CD_SECTORS_PER_SECOND

class CD {
private:
    class SectorFile {
    protected:
        const uint32_t total_sectors;

    public:
        SectorFile(uint32_t total_sectors) : total_sectors(total_sectors) {}

        virtual void reset() = 0;
        virtual void seek_by(uint32_t sectors) = 0;
        virtual void seek_to_end() { seek_by(get_remaining_sectors()); }
        virtual void read_sector(uint8_t* buffer) = 0;
        virtual uint32_t get_read_sectors() = 0;
        uint32_t get_remaining_sectors() { return get_total_sectors() - get_read_sectors(); }
        uint32_t get_total_sectors() const { return total_sectors; }
    };

    class BinaryFile : public SectorFile {
    private:
        // The stream position is used to keep track of which sector we are currently at
        std::ifstream stream;

    public:
        BinaryFile(std::ifstream&& stream, uint32_t sectors);
        void reset() override;
        void seek_by(uint32_t sectors) override;
        void read_sector(uint8_t* buffer) override;
        uint32_t get_read_sectors() override;
    };

    class Gap : public SectorFile {
    private:
        uint32_t read_sectors;

    public:
        Gap(uint32_t sectors);
        void reset() override;
        void seek_by(uint32_t sectors) override;
        void read_sector(uint8_t* buffer) override;
        uint32_t get_read_sectors() override;
    };


    struct TrackOnDisc {
        using Mode = util::cue::Track::Mode;

        Mode mode;
        uint32_t number;
        uint32_t position_on_disc; // in sectors
    };

    struct IndexOnDisc {
        const TrackOnDisc& track;
        uint32_t number;
        uint32_t position_in_track; // in sectors
        uint32_t length; // in sectors

        std::shared_ptr<SectorFile> file;
        uint32_t offset_in_file; //TODO: Not used currently
    };

    std::vector<std::shared_ptr<SectorFile>> files;
    std::vector<TrackOnDisc> tracks;

    std::vector<IndexOnDisc> indexes;
    std::vector<IndexOnDisc>::iterator current_index;

public:
    static const uint32_t SECTOR_SIZE = 2352; // 0x930
    using Index = util::cue::Index;

    CD(const std::string &filename);
    void reset();

    void open_cue_sheet(const std::string &filename);
    void seek_to_bcd(uint8_t bcd_minutes, uint8_t bcd_seconds, uint8_t bcd_sectors);
    bool read_sector_and_advance(uint8_t *buffer);

    bool at_end_of_disc() const;
    TrackOnDisc::Mode get_current_track_mode() const;
    uint32_t get_current_track_number() const;
    uint32_t get_current_index_number() const;
    Index get_current_position_in_track() const;
    Index get_current_position_on_disc() const;

private:
    void seek_to(uint32_t sectors);
    void seek_by(uint32_t sectors);

    void reset_position();
};

}

#endif

