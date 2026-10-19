#include <cassert>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

int version_major = 24;
struct OSObject {};
struct OSBoolean : OSObject {
    bool value;
    explicit OSBoolean(bool v) : value(v) {}
    bool isTrue() const { return value; }
};
#define OSDynamicCast(type, object) static_cast<type *>(object)
struct IOUSBHostDevice {
    std::string name = "Bluetooth 5.4 Radio";
    std::map<std::string, std::string> properties = {{"USB Product Name", "Bluetooth 5.4 Radio"},
        {"idVendor", "3034"}, {"idProduct", "42792"}};
    std::vector<std::string> writes;
    void setName(const char *s) { name = s; }
    void setProperty(const char *key, const char *value) { properties[key] = value; writes.emplace_back(key); }
    void setProperty(const char *key, bool value) { properties[key] = value ? "true" : "false"; writes.emplace_back(key); }
};
class RealtekBluetoothFirmware {
public:
    OSBoolean *external = nullptr;
    std::map<std::string, std::string> properties;
    OSObject *getProperty(const char *) { return external; }
    void setProperty(const char *key, const char *value) { properties[key] = value; }
    void publishReg(IOUSBHostDevice *, bool, const char *);
};
#include "publish_handoff_method.inc"

int main() {
    { IOUSBHostDevice device; RealtekBluetoothFirmware driver; OSBoolean yes(true); driver.external = &yes;
      driver.publishReg(&device, true, "rtl8761bu");
      assert(device.properties["USB Product Name"] == "Bluetooth 5.4 Radio");
      assert(device.name == "Bluetooth 5.4 Radio");
      assert(device.properties["idVendor"] == "3034" && device.properties["idProduct"] == "42792");
      assert(device.properties["FirmwareLoaded"] == "true");
      assert(driver.properties["fw_name"] == "rtl8761bu" && driver.properties["handoff_mode"] == "external");
      assert(device.writes.size() == 1 && device.writes.back() == "FirmwareLoaded");
      puts("PASS external personality preserves registry name, product property and USB IDs"); }
    { IOUSBHostDevice device; RealtekBluetoothFirmware driver;
      driver.publishReg(&device, true, "rtl8822cu");
      assert(device.name == "Bluetooth USB Host Controller");
      assert(device.properties["USB Product Name"] == "Bluetooth USB Host Controller");
      assert(driver.properties["handoff_mode"] == "legacy-internal-name");
      puts("PASS absent opt-in preserves other Sequoia personalities"); }
    { IOUSBHostDevice device; RealtekBluetoothFirmware driver; OSBoolean no(false); driver.external = &no;
      driver.publishReg(&device, true, "rtl8761bu");
      assert(device.name == "Bluetooth USB Host Controller");
      assert(device.properties["USB Product Name"] == "Bluetooth USB Host Controller");
      puts("PASS explicit false preserves legacy handoff"); }
    { version_major = 23; IOUSBHostDevice device; RealtekBluetoothFirmware driver;
      driver.publishReg(&device, true, "rtl8822cu");
      assert(device.name == "Bluetooth USB Host Controller");
      assert(device.properties["USB Product Name"] == "Bluetooth 5.4 Radio");
      puts("PASS pre-Sequoia legacy personality behavior preserved"); }
    { version_major = 24; IOUSBHostDevice device; RealtekBluetoothFirmware driver; OSBoolean yes(true); driver.external = &yes;
      driver.publishReg(&device, false, "rtl8761bu");
      assert(device.properties["FirmwareLoaded"] == "false");
      assert(device.name == "Bluetooth 5.4 Radio");
      assert(device.properties["USB Product Name"] == "Bluetooth 5.4 Radio");
      assert(driver.properties.count("fw_name") == 0);
      puts("PASS failed firmware is not advertised as ready"); }
    { version_major = 21; IOUSBHostDevice device; RealtekBluetoothFirmware driver; OSBoolean yes(true); driver.external = &yes;
      driver.publishReg(&device, true, "rtl8761bu");
      assert(device.name == "Bluetooth 5.4 Radio");
      assert(device.properties["USB Product Name"] == "Bluetooth 5.4 Radio");
      puts("PASS external personality also preserves names before Sequoia"); }
    puts("ALL HANDOFF TESTS PASSED (6 cases)");
}
