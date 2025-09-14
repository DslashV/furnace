#include "export.h"
#include "chips/sid.h"
#include <fstream>
#include <cstring>
#include <vector>

struct SIDWrite {
    uint32_t time;
    uint8_t reg;
    uint8_t value;
};

struct PSIDHeader {
    char magic[4];
    uint16_t version;
    uint16_t dataOffset;
    uint16_t loadAddr;
    uint16_t initAddr;
    uint16_t playAddr;
    uint16_t songs;
    uint16_t startSong;
    uint32_t speed;
    char name[32];
    char author[32];
    char released[32];
};

// Minimal 6502 player
static const uint8_t sidPlayer[] = {
    0x85,0xFE,       // STA $FE
    0xA2,0x00,       // LDX #$00
    0x86,0xFB,       // STX $FB
    0x86,0xFC,       // STX $FC
    0x60,            // RTS

    0xA0,0x00,       // LDY #$00
    0xB1,0xFB,       // LDA ($FB),Y
    0xC9,0xFF,       // CMP #$FF
    0xF0,0x1C,       // BEQ DONE
    0xAA,            // TAX
    0xC8,            // INY

    0xB1,0xFB,       // LDA ($FB),Y
    0x85,0xFD,       // STA $FD
    0xC8,            // INY
    0xB1,0xFB,       // LDA ($FB),Y
    0xA6,0xFD,       // LDX $FD
    0x9D,0x00,0xD4,  // STA $D400,X
    0xC8,            // INY
    0xCA,            // DEX
    0xD0,0xF4,       // BNE NEXTWRITE

    0x98,            // TYA
    0x18,            // CLC
    0x65,0xFB,       // ADC $FB
    0x85,0xFB,       // STA $FB
    0x90,0x02,       // BCC SKIP
    0xE6,0xFC,       // INC $FC
    0x60,            // RTS

    0x60             // DONE RTS
};

class ExporterSID : public Exporter {
public:
    ExporterSID() {}
    virtual ~ExporterSID() {}

    virtual const char* getName() const override {
        return "Commodore 64 SID (.sid)";
    }

    virtual void log(uint8_t chipId, uint16_t addr, uint8_t data, uint32_t time) override {
        if (chipId == CHIP_SID && addr >= 0xD400 && addr <= 0xD418) {
            SIDWrite w;
            w.time  = time;
            w.reg   = addr - 0xD400;
            w.value = data;
            writes.push_back(w);
        }
    }

    virtual bool exportSong(Song* song, const std::string& filename,
                            const ExportConfig& cfg) override {

        std::ofstream out(filename, std::ios::binary);
        if (!out.is_open()) return false;

        int numSubsongs = (int)song->subsongs.size();
        if (numSubsongs < 1) numSubsongs = 1;

        int startSong = cfg.sidStartSong - 1;
        int endSong   = cfg.sidExportAll ? numSubsongs - 1 : startSong;

        PSIDHeader header;
        memset(&header, 0, sizeof(header));
        memcpy(header.magic, "PSID", 4);
        header.version    = htons(2);
        header.dataOffset = htons(0x0076);
        header.loadAddr   = htons(0x1000);
        header.initAddr   = htons(0x1000);
        header.playAddr   = htons(0x1005);
        header.songs      = htons(endSong - startSong + 1);
        header.startSong  = htons(cfg.sidStartSong);
        header.speed      = htonl(cfg.sidUseCIA ? 0xFFFFFFFF : 0x00000000);
        strcpy(header.name,     "Furnace Export");
        strcpy(header.author,   "Furnace Tracker");
        strcpy(header.released, "2025");

        out.write(reinterpret_cast<char*>(&header), sizeof(header));
        out.write((const char*)sidPlayer, sizeof(sidPlayer));

        std::vector<uint16_t> subsongOffsets;
        std::streampos tablePos = out.tellp();
        for (int s = startSong; s <= endSong; s++) {
            subsongOffsets.push_back(0);
            out.put(0); out.put(0);
        }

        for (int s = startSong; s <= endSong; s++) {
            if (!cfg.sidExportAll && !cfg.sidSelectedSubsongs[s])
                continue;

            writes.clear();
            song->selectSubsong(s);
            song->renderAll();

            std::streampos dataPos = out.tellp();
            subsongOffsets[s - startSong] = (uint16_t)(dataPos - 0x1000);

            uint32_t frame = 0;
            size_t pos = 0;
            while (pos < writes.size()) {
                std::vector<SIDWrite> frameWrites;
                while (pos < writes.size() && writes[pos].time == frame) {
                    frameWrites.push_back(writes[pos]);
                    pos++;
                }
                out.put((uint8_t)frameWrites.size());
                for (auto& w : frameWrites) {
                    out.put(w.reg);
                    out.put(w.value);
                }
                frame++;
            }
            out.put(0xFF);
        }

        std::streampos endPos = out.tellp();
        out.seekp(tablePos);
        for (auto offs : subsongOffsets) {
            out.put(offs & 0xFF);
            out.put((offs >> 8) & 0xFF);
        }
        out.seekp(endPos);

        out.close();
        return true;
    }

private:
    std::vector<SIDWrite> writes;
};

REGISTER_EXPORTER("SID", ExporterSID);
