#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

using mach_vm_address_t = uintptr_t;
constexpr uint64_t kIOPCIConfigVendorID = 0, kIOPCIConfigDeviceID = 2, kIOPCIConfigRevisionID = 8;
#define LOG(...) ((void)0)
#define FunctionCast(fn, addr) reinterpret_cast<decltype(&fn)>(addr)
#include "actual-pci-hooks.inc"

struct Config { uint8_t bytes[64] {}; };
static uint8_t read8(void *dev, uint64_t off) { return static_cast<Config *>(dev)->bytes[off]; }
static uint16_t read16(void *dev, uint64_t off) {
	uint16_t value; std::memcpy(&value, static_cast<Config *>(dev)->bytes + off, sizeof(value)); return value;
}
static uint32_t read32(void *dev, uint64_t off) {
	uint32_t value; std::memcpy(&value, static_cast<Config *>(dev)->bytes + off, sizeof(value)); return value;
}
static Config config(uint16_t vendor, uint16_t device, uint32_t revisionClass) {
	Config c;
	std::memcpy(c.bytes, &vendor, 2); std::memcpy(c.bytes + 2, &device, 2);
	std::memcpy(c.bytes + 8, &revisionClass, 4); c.bytes[4] = 6; c.bytes[12] = 0x5a;
	return c;
}
static unsigned checks;
static void expect(const char *name, uint64_t actual, uint64_t wanted) {
	if (actual != wanted) {
		std::fprintf(stderr, "%s: %llx != %llx\n", name,
		             static_cast<unsigned long long>(actual), static_cast<unsigned long long>(wanted));
		std::exit(1);
	}
	++checks;
}
int main() {
	orgPciRead8 = reinterpret_cast<uintptr_t>(read8);
	orgPciRead16 = reinterpret_cast<uintptr_t>(read16);
	orgPciRead32 = reinterpret_cast<uintptr_t>(read32);
	auto gpu = config(0x1002, 0x1681, 0x03000002);
	expect("mapped PCI16 device ID", wrapPciRead16(&gpu, 2), 0x73ff);
	expect("mapped PCI32 vendor/device", wrapPciRead32(&gpu, 0), 0x73ff1002);
	expect("mapped PCI8 revision", wrapPciRead8(&gpu, 8), 0xc7);
	expect("mapped PCI16 revision", wrapPciRead16(&gpu, 8), 0xc7);
	expect("mapped PCI32 revision/class", wrapPciRead32(&gpu, 8), 0x030000c7);
	expect("raw config remains original", read32(&gpu, 0), 0x16811002);
	expect("raw revision remains original", read8(&gpu, 8), 2);
	for (uint64_t off : {4, 9, 10, 12}) {
		expect("unrelated PCI8 offset", wrapPciRead8(&gpu, off), read8(&gpu, off));
		expect("unrelated PCI16 offset", wrapPciRead16(&gpu, off), read16(&gpu, off));
		expect("unrelated PCI32 offset", wrapPciRead32(&gpu, off), read32(&gpu, off));
	}
	auto upper = config(0x1002, 0x1681, 0x0300a502);
	expect("PCI16 preserves upper byte", wrapPciRead16(&upper, 8), 0xa5c7);
	expect("PCI32 preserves class/programming interface", wrapPciRead32(&upper, 8), 0x0300a5c7);
	for (auto other : {config(0x1002, 0x164c, 0x03000002), config(0x10de, 0x1681, 0x03000002),
	                   config(0x8086, 0x1234, 0x03000002), config(0x1002, 0x73ff, 0x03000002)}) {
		expect("other device PCI8", wrapPciRead8(&other, 8), 2);
		expect("other device PCI16", wrapPciRead16(&other, 8), 2);
		expect("other device PCI32", wrapPciRead32(&other, 8), 0x03000002);
		expect("other device identity PCI16", wrapPciRead16(&other, 2), read16(&other, 2));
		expect("other device identity PCI32", wrapPciRead32(&other, 0), read32(&other, 0));
	}
	for (auto revision : {0x00, 0x01, 0xc7, 0xff}) {
		auto other = config(0x1002, 0x1681, 0x03000000 | revision);
		expect("other revision PCI8", wrapPciRead8(&other, 8), revision);
		expect("other revision PCI16", wrapPciRead16(&other, 8), revision);
		expect("other revision PCI32", wrapPciRead32(&other, 8), 0x03000000 | revision);
	}
	std::printf("PASS: %u PCI identity checks using the source hooks\n", checks);
}
