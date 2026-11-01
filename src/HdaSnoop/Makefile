# Freestanding UEFI boot-service driver (PE32+, subsystem 11). `make install` copies into the bare-metal OC tree.
EFI = HdaSnoop.efi
CFLAGS = -target x86_64-unknown-windows -ffreestanding -fno-stack-protector -fshort-wchar -mno-red-zone \
         -fno-builtin -O2 -Wall -Wextra
$(EFI): HdaSnoop.c
	clang $(CFLAGS) -c HdaSnoop.c -o HdaSnoop.o
	lld-link /nologo /subsystem:efi_boot_service_driver /entry:efi_main /nodefaultlib /dll /out:$(EFI) HdaSnoop.o
install: $(EFI)
	cp $(EFI) ../../efi/EFI/OC/Drivers/
clean:
	rm -f HdaSnoop.o $(EFI) HdaSnoop.lib
