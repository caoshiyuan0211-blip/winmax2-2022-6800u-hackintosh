// HdaSnoop: enable DMA snooping on AMD HD Audio controllers before OpenCore's AudioDxe plays the chime.
// Linux hda_intel (AZX_DCAPS_PRESET_AMD_SB for 1022:15e3, AZX_SNOOP_TYPE_ATI) sets PCI config 0x42
// (ATI_SB450_HDAUDIO_MISC_CNTR2) bits 0-2 = 0x02 (ENABLE_SNOOP); AudioDxe never does. Without it the
// controller reads the chime's DMA buffer from RAM while the decoded samples still sit in the CPU cache
// -> silence after a cold boot; after a warm reboot the bit left by the previous OS (or the old buffer
// contents in RAM) make it play. Same non-coherent DMA as the GPU IH ring on this machine (kext ihflush).
// Freestanding (no EDK2): build with src/HdaSnoop/Makefile. No serial on bare metal, so the result goes
// to NVRAM HdaSnoop-6d2b8f31-9c41-4f0e-a752-138e5d2c914b (written only when it changes):
// per controller 5 bytes = bus, dev, fn, reg 0x42 before, reg 0x42 after.
//
// Codec power-up (cold-boot chime fix): Linux conexant.c cx_set_power_state: "SN6140 may not respond to
// AFG D0 immediately after S3" -> 1 s wait after the AFG D0 verb before the widgets. AudioDxe sends AFG D0
// and the widget D0s back to back and plays at once; warm reboots work because the previous OS left the
// codec in D0 (HDA link reset does not power it down). So before AudioDxe loads we bring the Conexant
// codec (vendor 14f1) to D0 ourselves via the Immediate Command interface, waiting until the AFG really
// reports D0 (<= 1.5 s, re-sending D0 every 100 ms), then D0 to every widget with power control.
// Only runs when the codec is not already in D0 (warm boot = no delay). All synchronous in efi_main,
// before AudioDxe exists (never call into other drivers from a timer: that deadlocked OC on 10-31).
// Result in NVRAM HdaPower-<same guid>: 'P',1, cad, flags (1 cold path ran, 2 D0 reached, 4 IC timeout,
// 8 no codec, 0x10 CORB was running), AFG power resp before u32, after u32, ms waited u16, D0 sends u8,
// widgets powered u8, GCTL before u32, STATESTS u16.

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;
typedef u64 uintn;
typedef u64 status_t;
typedef void *handle_t;
typedef u16 char16;

#define EFIAPI __attribute__((ms_abi))
#define EFI_ERR(s) ((s) >> 63)

typedef struct { u32 a; u16 b, c; u8 d[8]; } guid_t;
static const guid_t PciIoGuid = {0x4cf5b200, 0x68b8, 0x4ca5, {0x9e, 0xec, 0xb2, 0x3e, 0x3f, 0x50, 0x02, 0x9a}};
static const guid_t LogGuid = {0x6d2b8f31, 0x9c41, 0x4f0e, {0xa7, 0x52, 0x13, 0x8e, 0x5d, 0x2c, 0x91, 0x4b}};

typedef struct { u64 sig; u32 rev, size, crc, rsvd; } table_hdr;

typedef struct {
    table_hdr Hdr;
    void *RaiseTPL, *RestoreTPL;
    void *AllocatePages, *FreePages, *GetMemoryMap;
    void *AllocatePool;
    status_t (EFIAPI *FreePool)(void *buf);
    status_t (EFIAPI *CreateEvent)(u32 type, uintn tpl, void (EFIAPI *fn)(void *ev, void *ctx), void *ctx, void **ev);
    status_t (EFIAPI *SetTimer)(void *ev, u32 type, u64 trigger);
    void *WaitForEvent, *SignalEvent, *CloseEvent, *CheckEvent;
    void *InstallProtocolInterface, *ReinstallProtocolInterface, *UninstallProtocolInterface;
    status_t (EFIAPI *HandleProtocol)(handle_t h, const guid_t *g, void **iface);
    void *Reserved;
    status_t (EFIAPI *RegisterProtocolNotify)(const guid_t *g, void *ev, void **reg);
    status_t (EFIAPI *LocateHandle)(u32 type, const guid_t *g, void *key, uintn *size, handle_t *buf);
    void *LocateDevicePath, *InstallConfigurationTable;
    void *LoadImage, *StartImage, *Exit, *UnloadImage, *ExitBootServices;
    void *GetNextMonotonicCount;
    status_t (EFIAPI *Stall)(uintn us);
    void *SetWatchdogTimer;
    void *ConnectController, *DisconnectController;
    void *OpenProtocol, *CloseProtocol, *OpenProtocolInformation;
    void *ProtocolsPerHandle;
    status_t (EFIAPI *LocateHandleBuffer)(u32 type, const guid_t *g, void *key, uintn *n, handle_t **buf);
} boot_services;

typedef struct {
    table_hdr Hdr;
    void *GetTime, *SetTime, *GetWakeupTime, *SetWakeupTime, *SetVirtualAddressMap, *ConvertPointer;
    status_t (EFIAPI *GetVariable)(const char16 *name, const guid_t *g, u32 *attr, uintn *size, void *data);
    void *GetNextVariableName;
    status_t (EFIAPI *SetVariable)(const char16 *name, const guid_t *g, u32 attr, uintn size, void *data);
} runtime_services;

typedef struct {
    table_hdr Hdr;
    char16 *FirmwareVendor;
    u32 FirmwareRevision;
    handle_t ConsoleInHandle;
    void *ConIn;
    handle_t ConsoleOutHandle;
    void *ConOut;
    handle_t StandardErrorHandle;
    void *StdErr;
    runtime_services *RuntimeServices;
    boot_services *BootServices;
} system_table;

typedef struct pci_io pci_io;
typedef status_t (EFIAPI *pci_cfg_fn)(pci_io *, u32 width, u32 off, uintn count, void *buf);
struct pci_io {
    void *PollMem, *PollIo;
    struct { status_t (EFIAPI *Read)(pci_io *, u32 width, u8 bar, u64 off, uintn count, void *buf);
             status_t (EFIAPI *Write)(pci_io *, u32 width, u8 bar, u64 off, uintn count, void *buf); } Mem;
    struct { void *Read, *Write; } Io;
    struct { pci_cfg_fn Read, Write; } Pci;
    void *CopyMem, *Map, *Unmap, *AllocateBuffer, *FreeBuffer, *Flush;
    status_t (EFIAPI *GetLocation)(pci_io *, uintn *seg, uintn *bus, uintn *dev, uintn *fn);
};
enum { PciWidthUint8 = 0, PciWidthUint32 = 2 };

// ATI_SB450_HDAUDIO_MISC_CNTR2 (Linux hda_intel.c)
#define MISC_CNTR2 0x42
#define MISC_CNTR2_MASK 0x07
#define MISC_CNTR2_ENABLE_SNOOP 0x02

// clang may emit calls to these even freestanding
void *memcpy(void *d, const void *s, uintn n) { u8 *dd = d; const u8 *ss = s; while (n--) *dd++ = *ss++; return d; }
void *memset(void *d, int c, uintn n) { u8 *dd = d; while (n--) *dd++ = (u8)c; return d; }

static boot_services *gBs;
static pci_io *gHda;
static u32 rd32(u32 off) { u32 v = 0xffffffff; gHda->Mem.Read(gHda, PciWidthUint32, 0, off, 1, &v); return v; }
static u32 rd16(u32 off) { u16 v = 0xffff; gHda->Mem.Read(gHda, 1, 0, off, 1, &v); return v; }
static u32 rd8(u32 off) { u8 v = 0xff; gHda->Mem.Read(gHda, PciWidthUint8, 0, off, 1, &v); return v; }
static void wr32(u32 off, u32 v) { gHda->Mem.Write(gHda, PciWidthUint32, 0, off, 1, &v); }
static void wr16(u32 off, u32 v) { u16 x = (u16)v; gHda->Mem.Write(gHda, 1, 0, off, 1, &x); }
static void wr8(u32 off, u32 v) { u8 x = (u8)v; gHda->Mem.Write(gHda, PciWidthUint8, 0, off, 1, &x); }

static int gIcFail;
static void preSnap(int second);
static u32 gCad = 0xff;
static volatile u8 *gBar;  // BAR0 for the timer: direct MMIO, no PciIo call outside efi_main
static u32 mrd8(u32 o) { return gBar[o]; }
static u32 mrd16(u32 o) { return *(volatile u16 *)(gBar + o); }
static u32 mrd32(u32 o) { return *(volatile u32 *)(gBar + o); }
// Immediate Command: ICOI 0x60, ICII 0x64, ICIS 0x68 (bit0 ICB busy, bit1 IRV response valid)
static int icVerb(u32 verb, u32 *resp)
{
    int t;
    for (t = 0; t < 1000 && (rd16(0x68) & 1); t++) gBs->Stall(10);
    if (rd16(0x68) & 1) { gIcFail = 1; return 0; }
    wr16(0x68, 2);  // clear IRV
    wr32(0x60, verb);
    wr16(0x68, 1);  // go
    for (t = 0; t < 1000; t++) {
        u32 st = rd16(0x68);
        if (!(st & 1) && (st & 2)) { *resp = rd32(0x64); return 1; }
        gBs->Stall(10);
    }
    gIcFail = 1;
    return 0;
}
#define VERB(cad, nid, v12, p8) (((u32)(cad) << 28) | ((u32)(nid) << 20) | ((u32)(v12) << 8) | (p8))

static void put32(u8 *b, u32 v) { for (int i = 0; i < 4; i++) b[i] = (u8)(v >> (8 * i)); }

static void codecPower(runtime_services *rs)
{
    u8 rec[24];
    memset(rec, 0, sizeof(rec));
    rec[0] = 'P'; rec[1] = 1;
    u32 cmd = 0;
    gHda->Pci.Read(gHda, PciWidthUint32, 0x04, 1, &cmd);
    if (!(cmd & 2)) { u32 c = cmd | 2; gHda->Pci.Write(gHda, PciWidthUint32, 0x04, 1, &c); }
    u32 gctl = rd32(0x08);
    put32(rec + 16, gctl);
    if (!(gctl & 1)) {  // bring the link out of reset
        wr32(0x08, gctl | 1);
        for (int t = 0; t < 100 && !(rd32(0x08) & 1); t++) gBs->Stall(1000);
        gBs->Stall(2000);  // codecs need >= 521 us after CRST to request an address
    }
    if ((rd8(0x4C) & 2) || (rd8(0x5C) & 2)) {  // CORB/RIRB DMA running: stop, IC needs them idle
        rec[3] |= 0x10;
        wr8(0x4C, rd8(0x4C) & ~2);
        wr8(0x5C, rd8(0x5C) & ~2);
        gBs->Stall(1000);
    }
    u32 sts = rd16(0x0E);
    rec[22] = (u8)sts; rec[23] = (u8)(sts >> 8);
    u32 cad = 0xff, r;
    for (u32 a = 0; a < 15 && cad == 0xff; a++)
        if ((sts & (1u << a)) && icVerb(VERB(a, 0, 0xF00, 0), &r) && (r >> 16) == 0x14F1) cad = a;
    if (cad == 0xff) { rec[3] |= 8 | (gIcFail ? 4 : 0); goto out; }
    rec[2] = (u8)cad;
    gCad = cad;
    preSnap(0);
    u32 ps = 0xffffffff;
    icVerb(VERB(cad, 1, 0xF05, 0), &ps);
    put32(rec + 4, ps);
    if (ps != 0xffffffff && ((ps >> 4) & 0xf) != 0) {  // AFG actual power state not D0 = cold codec
        rec[3] |= 1;
        u32 ms = 0, sends = 0;
        for (; ms <= 1500; ms += 10) {
            if (ms % 100 == 0) { icVerb(VERB(cad, 1, 0x705, 0), &r); sends++; }
            gBs->Stall(10000);
            if (icVerb(VERB(cad, 1, 0xF05, 0), &ps) && ((ps >> 4) & 0xf) == 0 && ms >= 50) break;
        }
        rec[12] = (u8)ms; rec[13] = (u8)(ms >> 8); rec[14] = (u8)sends;
        // power every widget that has power control (Linux snd_hda_codec_set_power_to_all)
        u32 sub = 0;
        if (icVerb(VERB(cad, 1, 0xF00, 0x04), &sub)) {
            u32 start = (sub >> 16) & 0xff, cnt = sub & 0xff, n = 0;
            for (u32 w = start; w < start + cnt && w < 0x80; w++) {
                u32 caps = 0;
                if (icVerb(VERB(cad, w, 0xF00, 0x09), &caps) && (caps & (1u << 10))) { icVerb(VERB(cad, w, 0x705, 0), &r); n++; }
            }
            rec[15] = (u8)n;
        }
    } else if (ps != 0xffffffff) {
        rec[3] |= 0;
    }
    if (((ps >> 4) & 0xf) == 0 && ps != 0xffffffff) rec[3] |= 2;
    put32(rec + 8, ps);
    if (gIcFail) rec[3] |= 4;
out:;
    static const char16 name[] = u"HdaPower";
    u8 old[24];
    uintn oldLen = sizeof(old);
    u32 attr;
    int same = !EFI_ERR(rs->GetVariable(name, &LogGuid, &attr, &oldLen, old)) && oldLen == sizeof(rec);
    for (uintn i = 0; same && i < sizeof(rec); i++) same = old[i] == rec[i];
    if (!same) rs->SetVariable(name, &LogGuid, 7, sizeof(rec), rec);
}


// ---- 10-31 diag (SAFE: synchronous verbs only before AudioDxe exists; the timer reads MMIO only) ----
// NVRAM HdaPre: 'V',1, n, 0, then n x u32 responses of the verb list below (0xffffffff = no answer),
// taken twice: before codecPower (state left by firmware / previous OS) and after it.
static const u16 kPreNid[] = {1,1,1,1, 0x10,0x10,0x10,0x10,0x10, 0x16,0x16,0x16,0x16,0x16,0x16, 0x17,0x17,0x17,0x17,0x17,0x17};
static const u32 kPreVerb[] = {0xF0500,0xF1500,0xF1600,0xF1700, 0xF0500,0xF0600,0xA0000,0xBA000,0xB8000,
                               0xF0500,0xF0700,0xF0C00,0xF0100,0xF0900,0xBA000, 0xF0500,0xF0700,0xF0C00,0xF0100,0xF0900,0xBA000};
#define NPRE (sizeof(kPreVerb) / sizeof(kPreVerb[0]))
static u8 gPre[4 + 2 * NPRE * 4];
static void preSnap(int second)
{
    u8 *b = gPre + 4 + (second ? NPRE * 4 : 0);
    for (u32 i = 0; i < NPRE; i++) {
        u32 r = 0xffffffff;
        if (gCad != 0xff) {
            u32 verb = ((u32)gCad << 28) | ((u32)kPreNid[i] << 20) | kPreVerb[i];
            if (!icVerb(verb, &r)) r = 0xffffffff;
        }
        put32(b + 4 * i, r);
    }
}
// NVRAM HdaLpib: 'L',1, sd, flags (1 RUN seen, 2 RUN ended, 4 timeout no RUN), then u16 ms load->RUN,
// u32 ctl, u8 sts at RUN, u8 sts OR over run, u16 fmt, u32 cbl, u16 lvi, u32 bdpl, u32 bdpu, u32 dplbase,
// u32 gctl, u32 intctl, u16 ms RUN->end, u32 final lpib, u32 final ctl, then 48 x u32 LPIB every 50 ms.
static u8 gL[4 + 2 + 4 + 1 + 1 + 2 + 4 + 2 + 4 + 4 + 4 + 4 + 4 + 2 + 4 + 4 + 48 * 4];
static runtime_services *gRs;
static void *gTimer;
static u32 gTick, gRunAt, gSd = 0xff, gSamples, gDone, gStsOr;
static u32 gSdBase(u32 sd) { return 0x80 + 0x20 * sd; }
static void saveL(void)
{
    static const char16 name[] = u"HdaLpib";
    gRs->SetVariable(name, &LogGuid, 7, sizeof(gL), gL);
}
// ---- 10-31 wbinvd test: at the first tick that sees RUN (whole chime already CopyMem'd into the 512 KB ring
// by AudioDxe, <=10 ms of DMA done) count non-zero dwords per BDL block (CPU/cache view), then wbinvd = write
// every dirty line back to RAM. Chime audible now = HDA DMA does not snoop the CPU cache.
// NVRAM HdaWb: 'W',1, flags (1 wbinvd done, 2 BDL read), 0, then 2 x {u64 addr, u32 len, u32 ioc}, u32 nz0, u32 nz1,
// u32 first nonzero byte offset in block 0 (0xffffffff none), u32 LPIB before wbinvd, u32 LPIB after.
static u8 gW[4 + 2 * 16 + 5 * 4];
static void wbTest(u32 o)
{
    gW[0] = 'W'; gW[1] = 1;
    u64 bdl = (u64)mrd32(o + 0x18) | ((u64)mrd32(o + 0x1c) << 32);
    bdl &= ~(u64)0x7f;
    put32(gW + 4 + 2 * 16 + 12, mrd32(o + 4));
    if (bdl && bdl < 0x100000000ull) {
        gW[2] |= 2;
        for (int e = 0; e < 2; e++) {
            volatile u32 *ent = (volatile u32 *)(uintn)(bdl + 16 * e);
            u64 a = (u64)ent[0] | ((u64)ent[1] << 32);
            u32 len = ent[2], ioc = ent[3];
            put32(gW + 4 + 16 * e, (u32)a); put32(gW + 8 + 16 * e, (u32)(a >> 32));
            put32(gW + 12 + 16 * e, len); put32(gW + 16 + 16 * e, ioc);
            u32 nz = 0, first = 0xffffffff;
            if (a && a < 0x100000000ull && len <= 0x100000) {
                volatile u32 *d = (volatile u32 *)(uintn)a;
                for (u32 i = 0; i < len / 4; i++) if (d[i]) { nz++; if (first == 0xffffffff) first = i * 4; }
            }
            put32(gW + 36 + 4 * e, nz);
            if (e == 0) put32(gW + 44, first);
        }
    } else put32(gW + 44, 0xffffffff);
    __asm__ volatile("wbinvd" ::: "memory");
    gW[2] |= 1;
    put32(gW + 52, mrd32(o + 4));
    static const char16 nm[] = u"HdaWb";
    gRs->SetVariable(nm, &LogGuid, 7, sizeof(gW), gW);
}

// ---- 10-31 19:50 controller-state dump (Linux B test: codec fine with AudioDxe-only verbs -> diff the
// controller side vs Linux). NVRAM HdaCfg: 'C',1, flags (1 cfg+mmio at SetupPlayback, 2 mmio at RUN), sd,
// then PCI cfg 0x00-0xFF, MMIO 0x00-0x7F after SetupPlayback, MMIO 0x00-0x7F at RUN, SDn regs 0x00-0x1F at RUN.
static u8 gC[4 + 256 + 128 + 128 + 32];
static void saveC(void) { static const char16 nm[] = u"HdaCfg"; if (gRs) gRs->SetVariable(nm, &LogGuid, 7, sizeof(gC), gC); }
static void cfgDumpSetup(void)
{
    gC[0] = 'C'; gC[1] = 1;
    for (u32 o = 0; o < 256; o += 4) { u32 v = 0xffffffff; gHda->Pci.Read(gHda, PciWidthUint32, o, 1, &v); put32(gC + 4 + o, v); }
    for (u32 o = 0; o < 128; o += 4) put32(gC + 260 + o, rd32(o));
    gC[2] |= 1; saveC();
}
static void cfgDumpRun(u32 o)
{
    for (u32 r = 0; r < 128; r += 4) put32(gC + 388 + r, mrd32(r));
    for (u32 r = 0; r < 32; r += 4) put32(gC + 516 + r, mrd32(o + r));
    gC[0] = 'C'; gC[1] = 1; gC[2] |= 2; gC[3] = (u8)gSd; saveC();
}
static void EFIAPI lpibTick(void *ev, void *ctx)
{
    (void)ev; (void)ctx;
    if (gDone) return;
    gTick++;  // 10 ms
    u32 gcap = mrd16(0x00), iss = (gcap >> 8) & 0xf, oss = (gcap >> 12) & 0xf;
    if (gSd == 0xff) {
        for (u32 i = 0; i < oss; i++) {
            u32 sd = iss + i;
            if (mrd8(gSdBase(sd)) & 2) {
                u32 o = gSdBase(sd);
                gSd = sd; gRunAt = gTick;
                gL[2] = (u8)sd; gL[3] |= 1;
                u32 ms = gTick * 10; gL[4] = (u8)ms; gL[5] = (u8)(ms >> 8);
                put32(gL + 6, mrd32(o) & 0xffffff);
                gL[10] = (u8)mrd8(o + 3);
                u32 f = mrd16(o + 0x12); gL[12] = (u8)f; gL[13] = (u8)(f >> 8);
                put32(gL + 14, mrd32(o + 8));
                u32 l = mrd16(o + 0xc); gL[18] = (u8)l; gL[19] = (u8)(l >> 8);
                put32(gL + 20, mrd32(o + 0x18)); put32(gL + 24, mrd32(o + 0x1c));
                put32(gL + 28, mrd32(0x70)); put32(gL + 32, mrd32(0x08)); put32(gL + 36, mrd32(0x20));
                wbTest(o);
                cfgDumpRun(o);
                break;
            }
        }
        if (gSd == 0xff && gTick > 3000) { gL[3] |= 4; gDone = 1; saveL(); }  // 30 s, no stream
        return;
    }
    u32 o = gSdBase(gSd);
    gStsOr |= mrd8(o + 3); gL[11] = (u8)gStsOr;
    u32 dt = gTick - gRunAt;
    if (dt % 5 == 0 && gSamples < 48) { put32(gL + 50 + 4 * gSamples, mrd32(o + 4)); gSamples++; }
    int running = mrd8(o) & 2;
    if (!running || gSamples >= 48) {
        if (!running) gL[3] |= 2;
        u32 ms = dt * 10; gL[40] = (u8)ms; gL[41] = (u8)(ms >> 8);
        put32(gL + 42, mrd32(o + 4)); put32(gL + 46, mrd32(o) & 0xffffff);
        gDone = 1;
        saveL();
    }
}

// ---- 10-31 16:45 FIX: power the SN6140 up AFTER AudioDxe's link reset, right before the chime ----
// AudioDxe always resets the HDA link (HdaControllerReset CRST 0->1) when it starts the controller, so the
// codecPower() above is undone. Its codec probe then sends AFG D0 and, a few ms later, D0 to the first widget
// (DAC 0x10); SN6140 ignores verbs right after AFG D0 (Linux conexant.c: 1 s delay) -> DAC stays D3 on a cold
// codec = silent chime. We wrap EFI_AUDIO_IO_PROTOCOL.SetupPlayback (pointer swapped in a protocol notify,
// which itself calls nothing in AudioDxe); the wrapper runs in OC's chime call (TPL_APPLICATION, no AudioDxe
// command in flight) and uses AudioDxe's own HdaIo SendCommand sequentially = safe.
// NVRAM HdaPlay: 'Y',1, flags (1 Conexant, 2 DAC10 D0 reached, 4 orig called, 8 orig failed), resends,
// u16 ms waited, u16 widgets powered, then NPL u32 x3 (before fix, after fix, after original SetupPlayback).
static const guid_t AudioIoGuid = {0x22266891, 0x2032, 0x4BAE, {0xB7, 0xB5, 0x43, 0x74, 0xE7, 0x32, 0x09, 0x49}};
static const guid_t HdaIoGuid = {0xA090D7F9, 0xB50A, 0x4EA1, {0xBD, 0xE9, 0x1A, 0xA5, 0xE9, 0x81, 0x2F, 0x45}};
typedef struct hda_io hda_io;
struct hda_io {
    void *GetAddress;
    status_t (EFIAPI *SendCommand)(hda_io *, u8 node, u32 verb, u32 *resp);
};
typedef status_t (EFIAPI *setup_fn)(void *This, u64 mask, signed char gain, u32 freq, u32 bits, u8 ch, uintn delay);
typedef struct { uintn Revision; void *GetOutputs, *RawGainToDecibels; setup_fn SetupPlayback; } audio_io;
static struct { audio_io *aio; hda_io *hio; setup_fn orig; } gWrap[4];
static void *gAioReg;
static const u8 kPlNid[] = {1,1,1,1, 0x10,0x10,0x10,0x10,0x10, 0x11, 0x16,0x16,0x16,0x16, 0x17,0x17,0x17,0x17};
static const u32 kPlVerb[] = {0xF0500,0xF1500,0xF1600,0xF1700, 0xF0500,0xF0600,0xA0000,0xBA000,0xB8000, 0xF0500,
                              0xF0500,0xF0700,0xF0C00,0xF0100, 0xF0500,0xF0700,0xF0C00,0xF0100};
#define NPL (sizeof(kPlVerb) / sizeof(kPlVerb[0]))
static u8 gPl[8 + 3 * NPL * 4];
static void plSnap(hda_io *h, int k)
{
    for (u32 i = 0; i < NPL; i++) {
        u32 r = 0xffffffff;
        if (EFI_ERR(h->SendCommand(h, kPlNid[i], kPlVerb[i], &r))) r = 0xffffffff;
        put32(gPl + 8 + (k * NPL + i) * 4, r);
    }
}

// AudioDxe clears PCIe "Enable No Snoop" only at Intel's fixed cfg 0x78; on AMD 15e3 the PCIe cap is elsewhere ->
// stream DMA may be non-snooped and read stale RAM while the chime data is still in CPU cache (silent cold boot).
// Clear DevCtl bit 11 in the real PCIe capability. NVRAM HdaNs: 'N',1,cap, [pre-AudioDxe devctl before,after],
// [at chime devctl before,after], cfg 0x78 at load (u16 each).
static u8 gNs[16];
static void nsFix(int k)
{
    if (!gHda) return;
    u8 p = 0, id = 0, cap = 0;
    gHda->Pci.Read(gHda, PciWidthUint8, 0x34, 1, &p);
    for (int g = 0; g < 48 && p >= 0x40 && p != 0xff; g++) {
        gHda->Pci.Read(gHda, PciWidthUint8, p, 1, &id);
        if (id == 0x10) { cap = p; break; }
        gHda->Pci.Read(gHda, PciWidthUint8, p + 1, 1, &p);
    }
    gNs[0] = 'N'; gNs[1] = 1; gNs[2] = cap;
    if (!cap) return;
    u16 b = 0xffff, a = 0xffff;
    gHda->Pci.Read(gHda, 1, cap + 8, 1, &b);
    if (b != 0xffff && (b & (1u << 11))) { u16 v = (u16)(b & ~(1u << 11)); gHda->Pci.Write(gHda, 1, cap + 8, 1, &v); }
    gHda->Pci.Read(gHda, 1, cap + 8, 1, &a);
    gNs[4 + 4 * k] = (u8)b; gNs[5 + 4 * k] = (u8)(b >> 8); gNs[6 + 4 * k] = (u8)a; gNs[7 + 4 * k] = (u8)(a >> 8);
    if (gRs) { static const char16 nm[] = u"HdaNs"; gRs->SetVariable(nm, &LogGuid, 7, sizeof(gNs), gNs); }
}
// Full-state dump (cold silent vs warm-from-macOS audible: everything in HdaPre/HdaPlay/HdaCfg was identical).
// Alternating slots A/B (seq in NVRAM HdaSeq) so a cold boot + a warm boot both survive:
//  HdaCdcX: 'F',1,seq,nw, afg[8] u32, then per widget 11 u32 (kFdVerb) for nw widgets from the AFG start node
//  HdaEcX : 256 bytes EC RAM (ACPI ERAX MMIO 0xFE0B0400)
//  HdaGpX : 0x400 bytes AMD FCH GPIO MMIO 0xFED81500
static const u32 kFdAfg[8] = {0xF0500,0xF1500,0xF1600,0xF1700,0xF0800,0xF2000,0xF0004,0xF0000};
static const u32 kFdVerb[11] = {0xF0900,0xF0500,0xF0600,0xA0000,0xBA000,0xB8000,0xB2000,0xB0000,0xF0100,0xF0700,0xF0C00};
#define FD_MAXW 40
static u8 gFd[4 + 8 * 4 + FD_MAXW * 11 * 4];
static u8 gFdEc[256];
static u8 gFdGp[0x400];
static void fullDump(hda_io *h, u32 tag)
{
    if (!gRs) return;
    static const guid_t *g = &LogGuid;
    u32 seq = 0, attr;
    uintn sz = sizeof(seq);
    static const char16 sn[] = u"HdaSeq";
    if (EFI_ERR(gRs->GetVariable(sn, g, &attr, &sz, &seq))) seq = 0;
    seq++;
    gRs->SetVariable(sn, g, 7, sizeof(seq), &seq);
    int b = seq & 1;
    memset(gFd, 0xff, sizeof(gFd));
    gFd[0] = 'F'; gFd[1] = 1; gFd[2] = (u8)seq; gFd[3] = 0;
    u32 sub = 0, r;
    for (int i = 0; i < 8; i++) {
        r = 0xffffffff;
        if (EFI_ERR(h->SendCommand(h, i == 7 ? 0 : 1, kFdAfg[i], &r))) r = 0xffffffff;
        if (i == 6) sub = r;
        put32(gFd + 4 + 4 * i, r);
    }
    u32 st = (sub >> 16) & 0xff, cnt = sub & 0xff;
    if (sub == 0xffffffff) cnt = 0;
    if (cnt > FD_MAXW) cnt = FD_MAXW;
    gFd[3] = (u8)cnt;
    for (u32 w = 0; w < cnt; w++)
        for (int k = 0; k < 11; k++) {
            r = 0xffffffff;
            if (EFI_ERR(h->SendCommand(h, (u8)(st + w), kFdVerb[k], &r))) r = 0xffffffff;
            put32(gFd + 4 + 32 + (w * 11 + k) * 4, r);
        }
    (void)tag;
    volatile u8 *ec = (volatile u8 *)(uintn)0xFE0B0400ull;
    for (int i = 0; i < 255; i++) gFdEc[i] = ec[i];
    gFdEc[255] = (u8)seq;
    volatile u32 *gp = (volatile u32 *)(uintn)0xFED81500ull;
    for (int i = 0; i < 0x100; i++) put32(gFdGp + 4 * i, gp[i]);
    static const char16 c0[] = u"HdaCdcA", c1[] = u"HdaCdcB", e0[] = u"HdaEcA", e1[] = u"HdaEcB",
                        p0[] = u"HdaGpA", p1[] = u"HdaGpB";
    gRs->SetVariable(b ? c1 : c0, g, 7, 4 + 32 + cnt * 44, gFd);
    gRs->SetVariable(b ? e1 : e0, g, 7, sizeof(gFdEc), gFdEc);
    gRs->SetVariable(b ? p1 : p0, g, 7, sizeof(gFdGp), gFdGp);
}
static status_t EFIAPI wrapSetup(void *This, u64 mask, signed char gain, u32 freq, u32 bits, u8 ch, uintn delay)
{
    int i;
    for (i = 0; i < 4 && gWrap[i].aio != This; i++) {}
    if (i == 4) return 0x8000000000000003ull;  // EFI_UNSUPPORTED (cannot happen)
    hda_io *h = gWrap[i].hio;
    u32 vid = 0, r;
    if (h && !EFI_ERR(h->SendCommand(h, 0, 0xF0000, &vid)) && (vid >> 16) == 0x14F1) {
        memset(gPl, 0, sizeof(gPl));
        gPl[0] = 'Y'; gPl[1] = 1; gPl[2] |= 1;
        plSnap(h, 0);
        u32 ms = 0, sends = 0, ps = 0xffffffff, n = 0;
        h->SendCommand(h, 1, 0x70500, &r);
        for (; ms < 1500; ms += 10) {  // AFG must really be in D0 before the widgets listen
            gBs->Stall(10000);
            if (!EFI_ERR(h->SendCommand(h, 1, 0xF0500, &ps)) && ((ps >> 4) & 0xf) == 0 && ms >= 50) break;
            if (ms % 100 == 90) h->SendCommand(h, 1, 0x70500, &r);
        }
        for (ms = 0; ms <= 1500; ms += 10) {
            if (ms % 100 == 0) {
                u32 sub = 0;
                n = 0;
                if (!EFI_ERR(h->SendCommand(h, 1, 0xF0004, &sub))) {
                    u32 st = (sub >> 16) & 0xff, cnt = sub & 0xff;
                    for (u32 w = st; w < st + cnt && w < 0x80; w++) {
                        u32 caps = 0;
                        if (!EFI_ERR(h->SendCommand(h, (u8)w, 0xF0009, &caps)) && (caps & (1u << 10))) {
                            h->SendCommand(h, (u8)w, 0x70500, &r); n++;
                        }
                    }
                }
                sends++;
            }
            gBs->Stall(10000);
            u32 a = 0xffffffff, b = 0xffffffff, c = 0xffffffff;
            h->SendCommand(h, 0x10, 0xF0500, &a);
            h->SendCommand(h, 0x16, 0xF0500, &b);
            h->SendCommand(h, 0x17, 0xF0500, &c);
            if (((a | b | c) & 0xf0) == 0 && a != 0xffffffff && ms >= 20) { gPl[2] |= 2; break; }
        }
        gPl[3] = (u8)sends; gPl[4] = (u8)ms; gPl[5] = (u8)(ms >> 8); gPl[6] = (u8)n;
        plSnap(h, 1);
    } else {
        h = 0;
    }
    nsFix(1);
    status_t s = gWrap[i].orig(This, mask, gain, freq, bits, ch, delay);
    if (h) {
        gPl[2] |= 4 | (EFI_ERR(s) ? 8 : 0);
        // Speaker pin 0x17 on DAC 0x11 like Linux (AudioDxe feeds HP 0x16 + SPK 0x17 both from DAC 0x10; chime
        // stays silent with everything else proven right). Copy DAC10 stream/format/amp to DAC11, select it.
        u32 st = 0, fmt = 0, amp = 0, sel = 0xffffffff;
        if (!EFI_ERR(s) && !EFI_ERR(h->SendCommand(h, 0x10, 0xF0600, &st)) && (st & 0xf0) &&
            !EFI_ERR(h->SendCommand(h, 0x10, 0xA0000, &fmt)) && !EFI_ERR(h->SendCommand(h, 0x10, 0xBA000, &amp))) {
            h->SendCommand(h, 0x11, 0x70500, &r);
            h->SendCommand(h, 0x11, 0x20000 | (fmt & 0xffff), &r);
            h->SendCommand(h, 0x11, 0x70600 | (st & 0xff), &r);
            h->SendCommand(h, 0x11, 0x3B000 | (amp & 0x7f), &r);
            h->SendCommand(h, 0x17, 0x70101, &r);
            h->SendCommand(h, 0x17, 0xF0100, &sel);
            if (sel == 1) gPl[2] |= 0x10;
        }
        plSnap(h, 2);
        fullDump(h, 0);
        static const char16 name[] = u"HdaPlay";
        gRs->SetVariable(name, &LogGuid, 7, sizeof(gPl), gPl);
    }
    cfgDumpSetup();
    return s;
}
static void EFIAPI aioNotify(void *ev, void *ctx)
{
    (void)ev; (void)ctx;
    for (;;) {
        handle_t hd = 0;
        uintn sz = sizeof(hd);
        if (EFI_ERR(gBs->LocateHandle(1 /* ByRegisterNotify */, &AudioIoGuid, gAioReg, &sz, &hd))) break;
        audio_io *a = 0;
        hda_io *hio = 0;
        if (EFI_ERR(gBs->HandleProtocol(hd, &AudioIoGuid, (void **)&a)) || !a) continue;
        gBs->HandleProtocol(hd, &HdaIoGuid, (void **)&hio);
        for (int i = 0; i < 4; i++) {
            if (gWrap[i].aio == a) break;
            if (!gWrap[i].aio) {
                gWrap[i].aio = a; gWrap[i].hio = hio; gWrap[i].orig = a->SetupPlayback;
                a->SetupPlayback = wrapSetup;
                break;
            }
        }
    }
}

status_t EFIAPI efi_main(handle_t image, system_table *st)
{
    (void)image;
    boot_services *bs = st->BootServices;
    runtime_services *rs = st->RuntimeServices;
    gBs = bs;
    u8 log[40];
    uintn len = 0;

    uintn n = 0;
    handle_t *hs = 0;
    if (EFI_ERR(bs->LocateHandleBuffer(2 /* ByProtocol */, &PciIoGuid, 0, &n, &hs))) return 0;
    for (uintn i = 0; i < n; i++) {
        pci_io *io;
        u32 id, cls;
        if (EFI_ERR(bs->HandleProtocol(hs[i], &PciIoGuid, (void **)&io))) continue;
        if (EFI_ERR(io->Pci.Read(io, PciWidthUint32, 0x00, 1, &id))) continue;
        if (EFI_ERR(io->Pci.Read(io, PciWidthUint32, 0x08, 1, &cls))) continue;
        // AMD (not ATI: the GPU HDMI audio function is the non-snooped ATI_HDMI_NS type in Linux) HDA
        if ((id & 0xffff) != 0x1022 || (cls >> 16) != 0x0403) continue;
        if (!gHda) gHda = io;
        u8 before = 0xff, after = 0xff;
        if (!EFI_ERR(io->Pci.Read(io, PciWidthUint8, MISC_CNTR2, 1, &before))) {
            u8 v = (u8)((before & ~MISC_CNTR2_MASK) | MISC_CNTR2_ENABLE_SNOOP);
            if (v != before) io->Pci.Write(io, PciWidthUint8, MISC_CNTR2, 1, &v);
            io->Pci.Read(io, PciWidthUint8, MISC_CNTR2, 1, &after);
        }
        uintn seg, bus, dev, fn;
        if (EFI_ERR(io->GetLocation(io, &seg, &bus, &dev, &fn))) bus = dev = fn = 0xff;
        if (len + 5 <= sizeof(log)) {
            log[len++] = (u8)bus;
            log[len++] = (u8)dev;
            log[len++] = (u8)fn;
            log[len++] = before;
            log[len++] = after;
        }
    }
    bs->FreePool(hs);

    // Record for Linux (efivarfs); only rewrite on change to spare the flash.
    static const char16 name[] = u"HdaSnoop";
    u8 old[40];
    uintn oldLen = sizeof(old);
    u32 attr;
    int same = !EFI_ERR(rs->GetVariable(name, &LogGuid, &attr, &oldLen, old)) && oldLen == len;
    for (uintn i = 0; same && i < len; i++) same = old[i] == log[i];
    if (!same && len) rs->SetVariable(name, &LogGuid, 7 /* NV | BS | RT */, len, log);
    gRs = rs;
    if (gHda) {
        u16 c78 = 0xffff;
        gHda->Pci.Read(gHda, 1, 0x78, 1, &c78);
        gNs[12] = (u8)c78; gNs[13] = (u8)(c78 >> 8);
        nsFix(0);
        codecPower(rs);
        if (gCad != 0xff) {
            preSnap(1);
            gPre[0] = 'V'; gPre[1] = 1; gPre[2] = (u8)NPRE;
            static const char16 pname[] = u"HdaPre";
            rs->SetVariable(pname, &LogGuid, 7, sizeof(gPre), gPre);
        }
        gL[0] = 'L'; gL[1] = 1; gL[2] = 0xff;
        u32 b0 = 0, b1 = 0;
        gHda->Pci.Read(gHda, PciWidthUint32, 0x10, 1, &b0);
        gHda->Pci.Read(gHda, PciWidthUint32, 0x14, 1, &b1);
        u64 bar = (b0 & ~0xfull) | (((b0 & 6) == 4) ? ((u64)b1 << 32) : 0);
        gBar = (volatile u8 *)(uintn)bar;
        // MMIO-only watcher of the chime stream (never calls AudioDxe/HdaIo: that deadlocked OC before)
        if (bar && !EFI_ERR(bs->CreateEvent(0x80000200 /* TIMER|NOTIFY_SIGNAL */, 8 /* TPL_CALLBACK */, lpibTick, 0, &gTimer)))
            bs->SetTimer(gTimer, 1 /* periodic */, 100000 /* 10 ms */);
    }
    {   // wrap AudioDxe SetupPlayback (see wrapSetup)
        void *ev = 0;
        if (!EFI_ERR(bs->CreateEvent(0x200 /* NOTIFY_SIGNAL */, 8 /* TPL_CALLBACK */, aioNotify, 0, &ev)))
            bs->RegisterProtocolNotify(&AudioIoGuid, ev, &gAioReg);
    }
    gRs = rs;
    // OpenCore halts on a driver that returns an error.
    return 0;
}
