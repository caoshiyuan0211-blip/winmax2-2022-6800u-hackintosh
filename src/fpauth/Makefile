# macOS build (cross from Linux: clang + ld64.lld + Xcode SDK from the macOS partition) and Linux test build.
SDK ?= /mnt/macos/root/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk
MAC = clang -target x86_64-apple-macos13 -isysroot $(SDK) -fuse-ld=lld -O3 -march=x86-64-v3 -ffast-math -Wall
CORE = fpflow.c fpcore.c fpmatch.c fpminu.c
OUT = ../../efi/EFI/OC/fpauth

all: mac/fpauth mac/pam_fpauth.so fpauth-linux fptest

mac/fpauth: fpauth.c $(CORE) fpusb_mac.c *.h
	mkdir -p mac
	$(MAC) -o $@ fpauth.c $(CORE) fpusb_mac.c -framework IOKit -framework CoreFoundation

mac/pam_fpauth.so: pam_fpauth.c $(CORE) fpusb_mac.c *.h
	mkdir -p mac
	$(MAC) -shared -fvisibility=default -o $@ pam_fpauth.c $(CORE) fpusb_mac.c -framework IOKit -framework CoreFoundation -lpam

fpauth-linux: fpauth.c $(CORE) fpusb_linux.c *.h
	gcc -O3 -march=x86-64-v3 -ffast-math -Wall -o $@ fpauth.c $(CORE) fpusb_linux.c -lm -lpthread

fptest: fptest.c fpmatch.c fpmatch.h
	gcc -O3 -march=x86-64-v3 -ffast-math -Wall -o $@ fptest.c fpmatch.c fpminu.c -lm

install: mac/fpauth mac/pam_fpauth.so install-mac.sh
	mkdir -p $(OUT)
	cp mac/fpauth mac/pam_fpauth.so install-mac.sh $(OUT)/
	md5sum $(OUT)/*

clean:
	rm -rf mac fpauth-linux fptest
