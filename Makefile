# Makefile — builds RealtekBluetoothFirmware.kext without Xcode.
#
# Usage:
#   make            # build the .kext
#   make fw         # regenerate include/FwBinary.cpp from fw/*.bin
#   make clean

KEXT_NAME    := RealtekBluetoothFirmware
BUNDLE_ID    := org.thegwchr.RealtekBluetoothFirmware
VERSION      := 1.0.3d2
ARCH         := x86_64

SDK          := $(shell xcrun --sdk macosx --show-sdk-path)
KH           := $(SDK)/System/Library/Frameworks/Kernel.framework
CXX          := $(shell xcrun -f clang++)
CC           := $(shell xcrun -f clang)
SRCDIR       := RealtekBluetoothFirmware
BUILD        := build
OBJ          := $(BUILD)/obj
KEXT         := $(BUILD)/$(KEXT_NAME).kext

CPPSRCS      := $(SRCDIR)/RealtekBluetoothFirmware.cpp \
                $(SRCDIR)/BtRtl.cpp \
                $(SRCDIR)/USBDeviceController.cpp \
                include/FwBinary.cpp
CXXONLY      := $(SRCDIR)/zutil.c
CSRCS        := $(SRCDIR)/kmod_info.c

OBJS         := $(OBJ)/RealtekBluetoothFirmware.o $(OBJ)/BtRtl.o \
                $(OBJ)/USBDeviceController.o $(OBJ)/FwBinary.o \
                $(OBJ)/zutil.o $(OBJ)/kmod_info.o

INCLUDES     := -I$(KH)/Headers -I$(KH)/PrivateHeaders -Iinclude -I$(SRCDIR)

CXXFLAGS     := -arch $(ARCH) -isysroot $(SDK) $(INCLUDES) \
                -fno-builtin -fno-common -mkernel -fapple-kext \
                -fno-exceptions -fno-rtti -std=gnu++14 \
                -DKERNEL -DKERNEL_PRIVATE -DDRIVER_PRIVATE -DAPPLE -DNeXT \
                -Wno-inconsistent-missing-override -Wno-unknown-pragmas -Os

CFLAGS       := -arch $(ARCH) -isysroot $(SDK) $(INCLUDES) \
                -fno-builtin -fno-common -mkernel -fapple-kext \
                -DKERNEL -DKERNEL_PRIVATE -DDRIVER_PRIVATE -DAPPLE -DNeXT -Os

LDFLAGS      := -arch $(ARCH) -isysroot $(SDK) -nostdlib -fapple-kext \
                -Xlinker -kext -L$(SDK)/usr/lib -lkmodc++ -lkmod

.PHONY: all clean fw test
all: $(KEXT)

# Host-side unit test of the firmware parser (userland, not a kext).
test: tests/fw_parse_test.cpp RealtekBluetoothFirmware/RtlFwParse.h tests/usb_interrupt_test.cpp tests/publish_handoff_test.cpp tests/download_completion_test.cpp tests/handoff_lifecycle_test.cpp scripts/extract_usb_test_methods.py $(SRCDIR)/USBDeviceController.cpp $(SRCDIR)/RealtekBluetoothFirmware.cpp $(SRCDIR)/BtRtl.cpp
	@mkdir -p $(BUILD)
	xcrun clang++ -std=c++14 -Wall -Wextra -O2 -o $(BUILD)/fw_parse_test tests/fw_parse_test.cpp
	@./$(BUILD)/fw_parse_test fw
	python3 scripts/extract_usb_test_methods.py $(SRCDIR)/USBDeviceController.cpp $(BUILD)/usb_interrupt_methods.inc
	xcrun clang++ -std=c++14 -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined -I$(BUILD) -o $(BUILD)/usb_interrupt_test tests/usb_interrupt_test.cpp
	@./$(BUILD)/usb_interrupt_test
	python3 scripts/extract_usb_test_methods.py $(SRCDIR)/RealtekBluetoothFirmware.cpp $(BUILD)/publish_handoff_method.inc
	xcrun clang++ -std=c++14 -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined -I$(BUILD) -o $(BUILD)/publish_handoff_test tests/publish_handoff_test.cpp
	@./$(BUILD)/publish_handoff_test
	python3 scripts/extract_usb_test_methods.py $(SRCDIR)/BtRtl.cpp $(BUILD)/download_firmware_method.inc
	xcrun clang++ -std=c++14 -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined -I$(BUILD) -o $(BUILD)/download_completion_test tests/download_completion_test.cpp
	@./$(BUILD)/download_completion_test
	python3 scripts/extract_usb_test_methods.py $(SRCDIR)/RealtekBluetoothFirmware.cpp $(BUILD)/handoff_lifecycle_methods.inc --lifecycle
	xcrun clang++ -std=c++14 -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined -I$(BUILD) -o $(BUILD)/handoff_lifecycle_test tests/handoff_lifecycle_test.cpp
	@./$(BUILD)/handoff_lifecycle_test

$(OBJ):
	@mkdir -p $(OBJ)

$(OBJ)/RealtekBluetoothFirmware.o: $(SRCDIR)/RealtekBluetoothFirmware.cpp | $(OBJ)
	$(CXX) $(CXXFLAGS) -c $< -o $@
$(OBJ)/BtRtl.o: $(SRCDIR)/BtRtl.cpp | $(OBJ)
	$(CXX) $(CXXFLAGS) -c $< -o $@
$(OBJ)/USBDeviceController.o: $(SRCDIR)/USBDeviceController.cpp | $(OBJ)
	$(CXX) $(CXXFLAGS) -c $< -o $@
$(OBJ)/FwBinary.o: include/FwBinary.cpp | $(OBJ)
	$(CXX) $(CXXFLAGS) -c $< -o $@
$(OBJ)/zutil.o: $(SRCDIR)/zutil.c | $(OBJ)
	$(CXX) $(CXXFLAGS) -x c++ -c $< -o $@
$(OBJ)/kmod_info.o: $(SRCDIR)/kmod_info.c | $(OBJ)
	$(CC) $(CFLAGS) -x c -c $< -o $@

$(BUILD)/$(KEXT_NAME): $(OBJS)
	$(CXX) $(LDFLAGS) -o $@ $(OBJS)

$(KEXT): $(BUILD)/$(KEXT_NAME) $(SRCDIR)/Info.plist
	@rm -rf $(KEXT)
	@mkdir -p $(KEXT)/Contents/MacOS
	@cp $(SRCDIR)/Info.plist $(KEXT)/Contents/Info.plist
	@cp $(BUILD)/$(KEXT_NAME) $(KEXT)/Contents/MacOS/$(KEXT_NAME)
	@/usr/libexec/PlistBuddy -c "Print" $(KEXT)/Contents/Info.plist >/dev/null
	@echo "Built $(KEXT)"
	@kextlibs -undef-symbols -xml $(KEXT) 2>&1 | head -20 || true

fw:
	python3 -c 'import sys; sys.path.append("scripts"); from zlib_compress_fw import process_files; process_files("include/FwBinary.cpp", "fw")'

clean:
	rm -rf $(BUILD)
