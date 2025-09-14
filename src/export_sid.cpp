#include "dialogExport.h"  // Furnace’s existing export dialog header
#include "chips/sid.h"
#include <fstream>
#include <vector>
#include <string>
#include <cstring>

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
    0x85,0xFE, 0xA2,0x00, 0x86,0xFB, 0x86,0xFC, 0x60,
    0xA0,0x00, 0xB1,0xFB, 0xC9,0xFF, 0xF0,0x1C, 0xAA, 0xC8,
    0xB1,0xFB, 0x85,0xFD, 0xC8, 0xB1,0xFB, 0xA6,0xFD, 0x9D,0x00,0xD4,
    0xC8, 0xCA, 0xD0,0xF4, 0x98, 0x18, 0x65,0xFB, 0x85,0xFB, 0x90,0x02,
    0xE6,0xFC, 0x60, 0x60
};

class ExporterSID : public Exporter {
public:
    ExporterSID() {}
    virtual ~ExporterSID() {}

    virtual const char* getName() const override {
        return "Commodore 64 SID (.sid)";
    }

    // Log SID writes
    virtual void log(uint8_t chipId, uint16_t addr, uint8_t data, uint32_t time) override {
        if (chipId == CHIP_SID && addr >= 0xD400 && addr <= 0xD418) {
            SIDWrite w;
            w.time  = time;
            w.reg   = addr - 0xD400;
            w.value = data;
            writes.push_back(w);
        }
    }

    // Self-test log buffer for GUI
    std::vector<std::string> sidLogBuffer;

    void logGUI(const std::string& text) {
        sidLogBuffer.push_back(text);
        if (sidLogBuffer.size() > 1000)
            sidLogBuffer.erase(sidLogBuffer.begin());
    }

    virtual bool exportSong(Song* song, const std::string& filename,
                            const ExportConfig& cfg) override {

        // === Automatic Self-Test (GUI) ===
        sidLogBuffer.clear();
        int numSubsongs = (int)song->subsongs.size();
        if (numSubsongs < 1) numSubsongs = 1;
        int startSong = cfg.sidStartSong - 1;
        int endSong   = cfg.sidExportAll ? numSubsongs - 1 : startSong;

        logGUI("=== SID Export Self-Test ===");
        logGUI("Total subsongs: " + std::to_string(numSubsongs));
        logGUI("Export range: " + std::to_string(startSong + 1) + " to " + std::to_string(endSong + 1));

        for (int s = startSong; s <= endSong; s++) {
            if (!cfg.sidExportAll && !cfg.sidSelectedSubsongs[s])
                continue;

            writes.clear();
            song->selectSubsong(s);
            song->renderAll();

            logGUI("Subsong " + std::to_string(s + 1) + ": total writes = " + std::to_string(writes.size()));

            for (size_t i = 0; i < writes.size() && i < 5; i++) {
                char buf[64];
                snprintf(buf, sizeof(buf), "  Frame %u: reg $%02X = $%02X",
                         writes[i].time, writes[i].reg, writes[i].value);
                logGUI(buf);
            }
            if (writes.size() > 5)
                logGUI("  ... (" + std::to_string(writes.size() - 5) + " more writes)");
        }
        logGUI("=== End of Self-Test ===");

        // === Write PSID file ===
        std::ofstream out(filename, std::ios::binary);
        if (!out.is_open()) return false;

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
