# VoodooPS2Keyboard 2.3.7 + RMB pseudo key e0f5 = Cmd+Shift+3. Toolchain as src/RembrandtGfx.
# Builds only the Keyboard plugin; installs it into efi VoodooPS2Controller.kext/PlugIns.
ROOT   := ../..
SDK    := ../MacKernelSDK
VPS2   := $(ROOT)/build/dl/vps2-237/VoodooPS2-2.3.7
LD     := $(ROOT)/build/cctools/bin/x86_64-apple-darwin19-ld
NAME   := VoodooPS2Keyboard
KFLAGS := -target x86_64-apple-macos10.15 -mkernel -nostdinc -fno-builtin -fno-common -mno-red-zone -O2 \
          -fno-asynchronous-unwind-tables -DKERNEL -DKERNEL_PRIVATE -DDRIVER_PRIVATE -DAPPLE -DNeXT \
          -I$(SDK)/Headers -I$(VPS2)/VoodooPS2Controller -I.
CXXFLAGS := $(KFLAGS) -std=c++17 -fapple-kext -nostdlib -fno-rtti -fno-exceptions \
            -fno-use-cxa-atexit -mllvm -disable-atexit-based-global-dtor-lowering \
            '-DLOGNAME="rmb"' -Wno-deprecated-declarations -Wno-everything
KEXT   := $(NAME).kext/Contents
DEST   := $(ROOT)/efi/EFI/OC/Kexts/VoodooPS2Controller.kext/Contents/PlugIns

all: $(KEXT)/MacOS/$(NAME)
VoodooPS2Keyboard.o: VoodooPS2Keyboard.cpp VoodooPS2Keyboard.h ApplePS2ToADBMap.h
	clang++ $(CXXFLAGS) -c $< -o $@
info.o: info.c
	clang $(KFLAGS) -c $< -o $@
$(NAME): VoodooPS2Keyboard.o info.o
	$(LD) -arch x86_64 -kext -platform_version macos 10.15 10.15 -no_data_const -o $@ $^ $(SDK)/Library/x86_64/libkmod.a 2>&1 | grep -v 'directory not found' || true
	@test -f $@
$(KEXT)/MacOS/$(NAME): $(NAME) Info.release.plist
	mkdir -p $(KEXT)/MacOS && cp $(NAME) $@ && cp Info.release.plist $(KEXT)/Info.plist
install: all
	rm -rf $(DEST)/$(NAME).kext && cp -r $(NAME).kext $(DEST)/
clean:
	rm -rf *.o $(NAME) $(NAME).kext
.PHONY: all install clean
