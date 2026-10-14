#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>

using IOReturn = uint32_t;
using AbsoluteTime = uint64_t;
constexpr IOReturn kIOReturnSuccess = 0, kIOReturnError = 1;
constexpr IOReturn kIOReturnTimeout = 2, kIOReturnAborted = 3;
constexpr IOReturn kIOReturnNoSpace = 4, kIOReturnUnderrun = 5;
constexpr IOReturn kIOUSBPipeStalled = 6, kIOReturnNotResponding = 7;
constexpr int THREAD_AWAKENED = 1, THREAD_TIMED_OUT = 2, THREAD_INTERRUPTED = 3;
constexpr int THREAD_INTERRUPTIBLE = 1, kMillisecondScale = 1;
constexpr int kRequestDirectionOut = 0, kRequestTypeClass = 1, kRequestRecipientDevice = 0;
constexpr uint16_t HCI_COMMAND_HDR_SIZE = 3;
#define XYLog(...) do {} while (0)
#define OSDynamicCast(type, object) static_cast<type *>(object)
using std::min;

struct OSObject {};
struct InterruptResp { IOReturn status; uint32_t dataLen; bool completed; };
struct IOBufferMemoryDescriptor {
    uint8_t bytes[4096] = {};
    size_t getLength() const { return sizeof(bytes); }
    void *getBytesNoCopy() { return bytes; }
};
struct IOUSBHostCompletion {
    void (*action)(void *, void *, IOReturn, uint32_t);
    void *owner;
    void *parameter;
};
struct IOUSBHostIOSource { enum { kAbortSynchronous = 1 }; };
struct IOLock {
    bool held = false;
    int sleeps = 0;
    int result = THREAD_AWAKENED;
    std::function<void()> onSleep;
};
static void IOLockLock(IOLock *lock) { assert(!lock->held); lock->held = true; }
static void IOLockUnlock(IOLock *lock) { assert(lock->held); lock->held = false; }
static void IOLockWakeup(IOLock *lock, void *, bool) { assert(lock->held); }
static int IOLockSleepDeadline(IOLock *lock, void *, AbsoluteTime, int) {
    assert(lock->held);
    ++lock->sleeps;
    assert(lock->sleeps < 4);
    IOLockUnlock(lock);
    if (lock->onSleep) lock->onSleep();
    IOLockLock(lock);
    return lock->result;
}
static void clock_interval_to_deadline(uint32_t ms, int, uint64_t *deadline) { *deadline = ms; }
static uint8_t makeDeviceRequestbmRequestType(int, int, int) { return 0x20; }
namespace StandardUSB {
struct DeviceRequest { uint8_t bmRequestType, bRequest; uint16_t wValue, wIndex, wLength; };
}
struct HciCommandHdr { uint16_t opcode; uint8_t len; };
struct IOUSBHostInterface {
    IOReturn status = kIOReturnSuccess;
    bool shortWrite = false;
    IOReturn deviceRequest(StandardUSB::DeviceRequest &req, HciCommandHdr *, uint32_t &actual, uint32_t) {
        actual = req.wLength - (shortWrite ? 1 : 0);
        return status;
    }
};
struct IOUSBHostPipe {
    IOUSBHostCompletion completion = {};
    bool pending = false, immediate = true, stallOnce = false;
    int submissions = 0, aborts = 0, clears = 0;
    IOReturn submitStatus = kIOReturnSuccess, completionStatus = kIOReturnSuccess;
    uint32_t length = 2;
    void finish(IOReturn status, uint32_t len) {
        assert(pending);
        pending = false;
        completion.action(completion.owner, completion.parameter, status, len);
    }
    IOReturn io(IOBufferMemoryDescriptor *buffer, uint32_t, IOUSBHostCompletion *c, uint32_t timeout) {
        assert(timeout == 0); // Interrupt endpoints forbid nonzero IO timeouts.
        ++submissions;
        if (stallOnce && submissions == 1) return kIOUSBPipeStalled;
        if (submitStatus != kIOReturnSuccess) return submitStatus;
        completion = *c;
        pending = true;
        buffer->bytes[0] = 0x0e;
        buffer->bytes[1] = 0x01;
        if (immediate) finish(completionStatus, length);
        return kIOReturnSuccess;
    }
    IOReturn clearStall(bool synchronous) { assert(synchronous); ++clears; return kIOReturnSuccess; }
    IOReturn abort(int options) {
        assert(options == IOUSBHostIOSource::kAbortSynchronous);
        ++aborts;
        if (pending) finish(kIOReturnAborted, 0);
        return kIOReturnSuccess;
    }
};
class USBDeviceController : public OSObject {
public:
    IOUSBHostPipe *m_pInterruptReadPipe;
    IOLock *_hciLock;
    IOBufferMemoryDescriptor *mReadBuffer;
    IOUSBHostInterface *m_pInterface;
    static void interruptHandler(void *, void *, IOReturn, uint32_t);
    IOReturn interruptPipeRead(void *, uint32_t, uint32_t *, uint32_t);
    IOReturn sendHCIRequest(HciCommandHdr *, uint32_t);
    const char *stringFromReturn(IOReturn) { return "mock"; }
};
#include "usb_interrupt_methods.inc"

struct Fixture {
    IOUSBHostPipe pipe;
    IOLock lock;
    IOBufferMemoryDescriptor buffer;
    IOUSBHostInterface interface;
    USBDeviceController driver;
    uint8_t output[16] = {};
    uint32_t size = 999;
    Fixture() { driver.m_pInterruptReadPipe = &pipe; driver._hciLock = &lock;
        driver.mReadBuffer = &buffer; driver.m_pInterface = &interface; }
    IOReturn read() { return driver.interruptPipeRead(output, sizeof(output), &size, 10); }
};
int main() {
    { Fixture f; assert(f.read() == kIOReturnSuccess); assert(f.size == 2);
      assert(f.output[0] == 0x0e); assert(f.lock.sleeps == 0);
      puts("PASS completion before wait (lost wake regression)"); }
    { Fixture f; f.pipe.immediate = false;
      f.lock.onSleep = [&] { if (f.lock.sleeps == 2) f.pipe.finish(kIOReturnSuccess, 2); };
      assert(f.read() == kIOReturnSuccess); assert(f.lock.sleeps == 2);
      puts("PASS spurious wake followed by completion"); }
    { Fixture f; f.pipe.completionStatus = kIOReturnNotResponding;
      assert(f.read() == kIOReturnNotResponding); assert(f.size == 0); assert(f.output[0] == 0);
      puts("PASS failed completion with nonzero length is propagated"); }
    { Fixture f; f.pipe.length = 0; assert(f.read() == kIOReturnError); assert(f.size == 0);
      puts("PASS zero-length completion rejected"); }
    { Fixture f; f.pipe.length = 17; assert(f.read() == kIOReturnNoSpace);
      assert(f.size == 0); assert(f.output[0] == 0);
      puts("PASS oversized completion rejected without truncation"); }
    { Fixture f; f.pipe.immediate = false; f.lock.result = THREAD_TIMED_OUT;
      assert(f.read() == kIOReturnTimeout); assert(f.pipe.aborts == 1); assert(!f.pipe.pending);
      puts("PASS timeout drains callback before stack return"); }
    { Fixture f; f.pipe.immediate = false; f.lock.result = THREAD_INTERRUPTED;
      assert(f.read() == kIOReturnAborted); assert(f.pipe.aborts == 1); assert(!f.pipe.pending);
      puts("PASS interrupted wait drains callback"); }
    { Fixture f; f.pipe.immediate = false; f.lock.result = THREAD_TIMED_OUT;
      f.lock.onSleep = [&] { f.pipe.finish(kIOReturnSuccess, 2); };
      assert(f.read() == kIOReturnSuccess); assert(f.pipe.aborts == 0);
      puts("PASS completion winning deadline race is accepted"); }
    { Fixture f; f.pipe.stallOnce = true; assert(f.read() == kIOReturnSuccess);
      assert(f.pipe.clears == 1); assert(f.pipe.submissions == 2);
      puts("PASS stalled submission retries once"); }
    { Fixture f; f.pipe.submitStatus = kIOReturnError; assert(f.read() == kIOReturnError);
      assert(f.lock.sleeps == 0); assert(f.size == 0);
      puts("PASS failed submission does not sleep"); }
    { Fixture f; HciCommandHdr cmd = {0x1001, 0};
      assert(f.driver.sendHCIRequest(&cmd, 10) == kIOReturnSuccess);
      f.interface.shortWrite = true; assert(f.driver.sendHCIRequest(&cmd, 10) == kIOReturnUnderrun);
      f.interface.status = kIOReturnError; assert(f.driver.sendHCIRequest(&cmd, 10) == kIOReturnError);
      puts("PASS complete, short, and failed HCI control transfers"); }
    puts("ALL USB INTERRUPT/CONTROL TESTS PASSED (11 cases)");
}
