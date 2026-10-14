#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

constexpr uint32_t RTL_FRAG_LEN = 252;
constexpr uint16_t RTL_OP_DOWNLOAD = 0xfc20;
constexpr int HCI_INIT_TIMEOUT = 10000;
struct __attribute__((packed)) RtlDownloadCmd { uint8_t index, data[RTL_FRAG_LEN]; };
struct HciRpReadLocalVersion { uint16_t hci_rev = 0, lmp_subver = 0; };
#define XYLog(...) do {} while (0)
class BtRtl {
public:
    bool versionOK = true, fragmentOK = true;
    int versionReads = 0;
    std::vector<uint8_t> indices, lengths;
    bool downloadFirmware(const uint8_t *, uint32_t);
    bool readLocalVersion(HciRpReadLocalVersion *) { ++versionReads; return versionOK; }
    bool hciCmd(uint16_t opcode, const void *p, uint8_t len, const uint8_t **params, uint32_t *plen, int timeout) {
        assert(opcode == RTL_OP_DOWNLOAD && timeout == HCI_INIT_TIMEOUT);
        indices.push_back(static_cast<const RtlDownloadCmd *>(p)->index);
        lengths.push_back(len);
        static const uint8_t response[] = {0, 0};
        *params = response; *plen = 2;
        return fragmentOK;
    }
};
#include "download_firmware_method.inc"

int main() {
    uint8_t data[504] = {};
    { BtRtl bt; assert(bt.downloadFirmware(data, 253));
      assert((bt.indices == std::vector<uint8_t>{0, 0x81}));
      assert((bt.lengths == std::vector<uint8_t>{253, 2})); assert(bt.versionReads == 1);
      puts("PASS fragment/end marker sequence and post-download read"); }
    { BtRtl bt; assert(bt.downloadFirmware(data, 252));
      assert((bt.indices == std::vector<uint8_t>{0, 0x81}));
      assert((bt.lengths == std::vector<uint8_t>{253, 1}));
      puts("PASS exact-multiple firmware sends zero-data final fragment"); }
    { BtRtl bt; bt.fragmentOK = false; assert(!bt.downloadFirmware(data, 253));
      assert(bt.versionReads == 0 && bt.indices.size() == 1);
      puts("PASS failed fragment stops download"); }
    { BtRtl bt; bt.versionOK = false; assert(!bt.downloadFirmware(data, 253));
      assert(bt.versionReads == 1);
      puts("PASS failed post-download version cannot report success"); }
    puts("ALL DOWNLOAD COMPLETION TESTS PASSED (4 cases)");
}
