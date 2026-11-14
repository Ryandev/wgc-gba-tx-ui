// Headless mGBA harness: boot a ROM with keys held, run a key script, dump screenshots
// and log every 16-bit store into cart space (0x08000000-0x09FFFFFF) -> the bytes the
// insideGadgets ATtiny would receive.
//
// Build (against libmgba 0.10, e.g. extracted from Ubuntu's libmgba-dev/libmgba0.10t64 debs):
//   gcc -O2 -I<root>/usr/include tools/emu_harness.c -L<root>/usr/lib/x86_64-linux-gnu -lmgba -o emu_harness
// Run:
//   LD_LIBRARY_PATH=<root>/usr/lib/x86_64-linux-gnu ./emu_harness rom.gba outdir STEP...
// Steps:  k:KEYS   set held keys (letters: A B s=Select S=Start R L > < ^ v r=Rshoulder l=Lshoulder, '-' = none)
//         f:N      run N frames
//         p:NAME   write outdir/NAME.ppm (240x160)
// Keys given before the first f: step are "held at boot".
#include <mgba/core/core.h>
#include <mgba/core/log.h>
#include <mgba/core/config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

static struct mCore* core;
static unsigned frame;
static FILE* wlog;

static void logcb(struct mLogger* l, int cat, enum mLogLevel lvl, const char* fmt, va_list ap) {
	(void) l; (void) cat; (void) lvl;
	char buf[256];
	vsnprintf(buf, sizeof buf, fmt, ap);
	const char* p = strstr(buf, "Store16");
	if (!p) return;
	const char* x = strstr(buf, "0x");
	if (!x) return;
	unsigned long addr = strtoul(x, NULL, 16);
	if (addr < 0x08000000 || addr > 0x09FFFFFF) return;
	// mGBA's message has no value, so decode the STRH that is executing and read its source
	// register. Pipeline: PC = instruction + 4 (Thumb) or + 8 (ARM).
	uint32_t pc = 0, cpsr = 0, val = 0;
	core->readRegister(core, "pc", &pc);
	core->readRegister(core, "cpsr", &cpsr);
	int rd = -1;
	if (cpsr & 0x20) {                                   // Thumb
		uint16_t op = core->rawRead16(core, pc - 4, -1);
		if ((op & 0xF800) == 0x8000 || (op & 0xFE00) == 0x5200) rd = op & 7;   // STRH imm / reg
	} else {                                             // ARM: STRH = cond 000P UIW0 Rn Rd .... 1011 ....
		uint32_t op = core->rawRead32(core, pc - 8, -1);
		if ((op & 0x0E1000F0) == 0x000000B0) rd = (op >> 12) & 15;
	}
	if (rd >= 0) {
		char name[4];
		snprintf(name, sizeof name, "r%d", rd);
		core->readRegister(core, name, &val);
		fprintf(wlog, "%u %08lx %02x\n", frame, addr, (unsigned) (val & 0xFFFF));
	} else {
		fprintf(wlog, "%u %08lx ?? (pc=%08x, not a recognised STRH)\n", frame, addr, (unsigned) pc);
	}
}

static uint32_t parseKeys(const char* s) {
	uint32_t k = 0;
	for (; *s; ++s) switch (*s) {
		case 'A': k |= 1 << 0; break; case 'B': k |= 1 << 1; break;
		case 's': k |= 1 << 2; break; case 'S': k |= 1 << 3; break;
		case '>': k |= 1 << 4; break; case '<': k |= 1 << 5; break;
		case '^': k |= 1 << 6; break; case 'v': k |= 1 << 7; break;
		case 'r': k |= 1 << 8; break; case 'l': k |= 1 << 9; break;
	}
	return k;
}

int main(int argc, char** argv) {
	if (argc < 3) { fprintf(stderr, "usage: %s rom outdir steps...\n", argv[0]); return 1; }
	static struct mLogFilter filter;
	mLogFilterInit(&filter);
	filter.defaultLevels = mLOG_ALL;
	static struct mLogger logger = { .log = logcb, .filter = &filter };
	mLogSetDefaultLogger(&logger);

	core = mCoreFind(argv[1]);
	if (!core || !core->init(core)) { fprintf(stderr, "no core\n"); return 1; }
	mCoreInitConfig(core, NULL);
	unsigned w, h;
	core->desiredVideoDimensions(core, &w, &h);
	color_t* fb = calloc(w * h, sizeof(color_t));
	core->setVideoBuffer(core, fb, w);
	if (!mCoreLoadFile(core, argv[1])) { fprintf(stderr, "load failed\n"); return 1; }

	char path[1024];
	snprintf(path, sizeof path, "%s/cart_writes.log", argv[2]);
	wlog = fopen(path, "w");
	fprintf(wlog, "# frame addr value\n");

	uint32_t keys = 0;
	int i = 3;
	for (; i < argc && argv[i][0] == 'k'; ++i) keys = parseKeys(argv[i] + 2);
	core->setKeys(core, keys);
	core->reset(core);
	for (; i < argc; ++i) {
		const char* a = argv[i];
		if (a[0] == 'k') { keys = parseKeys(a + 2); core->setKeys(core, keys); }
		else if (a[0] == 'f') { int n = atoi(a + 2); while (n--) { core->setKeys(core, keys); core->runFrame(core); ++frame; } }
		else if (a[0] == 'p') {
			snprintf(path, sizeof path, "%s/%s.ppm", argv[2], a + 2);
			FILE* f = fopen(path, "wb");
			fprintf(f, "P6\n%u %u\n255\n", w, h);
			for (unsigned j = 0; j < w * h; ++j) {
				uint32_t c = fb[j]; // XBGR8888
				unsigned char rgb[3] = { c & 0xFF, (c >> 8) & 0xFF, (c >> 16) & 0xFF };
				fwrite(rgb, 1, 3, f);
			}
			fclose(f);
		}
	}
	fclose(wlog);
	core->deinit(core);
	return 0;
}
