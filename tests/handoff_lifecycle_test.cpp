#include <cassert>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>

int version_major = 24;
int myTwoStates[2] = {};
struct OSObject { virtual ~OSObject() = default; };
struct OSBoolean : OSObject { bool isTrue() const { return true; } };
class IOService : public OSObject {
public:
    virtual bool start(IOService *) { return true; }
    void joinPMtree(IOService *) {}
};
class IOUSBHostDevice : public IOService {
public:
    bool deviceOpen = false, interfaceOpen = false, allowOpen = true, setupOK = true;
    std::string name = "Bluetooth 5.4 Radio";
    std::map<std::string, std::string> properties = {{"USB Product Name", "Bluetooth 5.4 Radio"}};
    bool open(IOService *) { deviceOpen = allowOpen; return allowOpen; }
    bool isOpen(IOService *) { return deviceOpen; }
    void close(IOService *) { assert(!interfaceOpen); deviceOpen = false; }
    void setName(const char *s) { name = s; }
    void setProperty(const char *key, const char *value) { properties[key] = value; }
    void setProperty(const char *key, bool value) {
        if (value) assert(!deviceOpen && !interfaceOpen);
        properties[key] = value ? "true" : "false";
    }
};
class BtRtl {
    IOUSBHostDevice *device = nullptr;
public:
    ~BtRtl() { if (device) device->interfaceOpen = false; }
    bool initWithDevice(IOService *, IOUSBHostDevice *d) { device = d; d->interfaceOpen = true; return true; }
    bool setup() { return device->setupOK; }
    bool getFirmwareName(char *p, size_t len) { assert(len >= 10); std::strcpy(p, "rtl8761bu"); return true; }
};
#define XYLog(...) do {} while (0)
#define OSDynamicCast(type, object) dynamic_cast<type *>(object)
#define OSSafeReleaseNULL(p) do { delete (p); (p) = nullptr; } while (0)
#define super IOService
class RealtekBluetoothFirmware : public IOService {
public:
    IOUSBHostDevice *m_pDevice = nullptr;
    BtRtl *m_pBTRtl = nullptr;
    OSBoolean external;
    bool stopped = false;
    std::map<std::string, std::string> properties;
    void PMinit() {}
    void registerPowerDriver(IOService *, int *, int) {}
    void makeUsable() {}
    void stop(IOService *) { stopped = true; }
    OSObject *getProperty(const char *) { return &external; }
    void setProperty(const char *key, const char *value) { properties[key] = value; }
    bool start(IOService *) override;
    void publishReg(IOUSBHostDevice *, bool, const char *);
    void cleanUp();
};
#include "handoff_lifecycle_methods.inc"

int main() {
    { IOUSBHostDevice device; RealtekBluetoothFirmware driver;
      assert(driver.start(&device));
      assert(!device.deviceOpen && !device.interfaceOpen);
      assert(driver.m_pBTRtl == nullptr && driver.m_pDevice == nullptr);
      assert(device.properties["FirmwareLoaded"] == "true");
      assert(device.name == "Bluetooth 5.4 Radio");
      assert(driver.properties["handoff_mode"] == "external");
      puts("PASS actual start/cleanup publishes readiness after closing USB ownership"); }
    { IOUSBHostDevice device; device.setupOK = false; RealtekBluetoothFirmware driver;
      assert(!driver.start(&device));
      assert(!device.deviceOpen && !device.interfaceOpen && driver.stopped);
      assert(device.properties.count("FirmwareLoaded") == 0);
      puts("PASS failed setup releases USB and does not publish firmware success"); }
    { IOUSBHostDevice device; device.allowOpen = false; RealtekBluetoothFirmware driver;
      assert(!driver.start(&device));
      assert(!device.deviceOpen && !device.interfaceOpen && driver.stopped);
      assert(device.properties.count("FirmwareLoaded") == 0);
      puts("PASS failed device open does not publish readiness"); }
    puts("ALL HANDOFF LIFECYCLE TESTS PASSED (3 cases)");
}
