# BrightnessKeys 1.0.3 + RMB ACPI _BCL/_BCM panel fallback. Toolchain as src/RembrandtGfx.
ROOT   := ../..
SDK    := ../MacKernelSDK
LILU   := ../Lilu-1.7.2/Lilu
LD     := $(ROOT)/build/cctools/bin/x86_64-apple-darwin19-ld
NAME   := BrightnessKeys
VER    := 1.0.3
KFLAGS := -target x86_64-apple-macos10.15 -mkernel -nostdinc -fno-builtin -fno-common -mno-red-zone -O2 \
          -fno-asynchronous-unwind-tables -DKERNEL -DKERNEL_PRIVATE -DDRIVER_PRIVATE -DAPPLE -DNeXT \
          -I$(SDK)/Headers -I$(LILU)
CXXFLAGS := $(KFLAGS) -std=c++17 -fapple-kext -nostdlib -fno-rtti -fno-exceptions \
            -fno-use-cxa-atexit -mllvm -disable-atexit-based-global-dtor-lowering \
            -DPRODUCT_NAME=$(NAME) -DMODULE_VERSION=$(VER) -Wno-deprecated-declarations
OBJS   := BrightnessKeys.o info.o
KEXT   := $(NAME).kext/Contents
EFIBM  := $(ROOT)/efi/EFI/OC/Kexts

all: $(KEXT)/MacOS/$(NAME)
BrightnessKeys.o: BrightnessKeys.cpp BrightnessKeys.hpp
	clang++ $(CXXFLAGS) -c $< -o $@
info.o: info.c
	clang $(KFLAGS) -c $< -o $@
$(NAME): $(OBJS)
	$(LD) -arch x86_64 -kext -platform_version macos 10.15 10.15 -no_data_const -o $@ $^ $(SDK)/Library/x86_64/libkmod.a 2>&1 | grep -v 'directory not found' || true
	@test -f $@
$(KEXT)/MacOS/$(NAME): $(NAME) Info.release.plist
	mkdir -p $(KEXT)/MacOS && cp $(NAME) $@ && cp Info.release.plist $(KEXT)/Info.plist
install: all
	rm -rf $(EFIBM)/$(NAME).kext && cp -r $(NAME).kext $(EFIBM)/
clean:
	rm -rf *.o $(NAME) $(NAME).kext
.PHONY: all install clean
