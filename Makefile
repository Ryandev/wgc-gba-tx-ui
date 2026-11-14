# wgc-gba-tx-ui build. Needs: gcc-arm-none-eabi, binutils-arm-none-eabi, libnewlib-arm-none-eabi,
# build-essential, git (and libmgba-dev + python3 for `make test`).
#
#   make                  ROM for the v1.1a cart protocol  -> build/wgc-gba-tx-ui.gba
#   make PROTOCOL=V11B    ROM for v1.1b (2023+) carts
#   make test             run the ROM headless in mGBA and compare cart writes with the stock ROM
#   make run              open the ROM in mgba-qt

PROTOCOL ?= V11A
NAME     := wgc-gba-tx-ui
B        := build
DEPS     := $(B)/deps
TONC     := $(DEPS)/libtonc
CRT      := $(DEPS)/devkitarm-crtls

CC       := arm-none-eabi-gcc
AR       := arm-none-eabi-ar
OBJCOPY  := arm-none-eabi-objcopy
ARCH     := -mcpu=arm7tdmi -mthumb-interwork
CFLAGS   := $(ARCH) -O2 -Wall -Wextra -ffunction-sections -fdata-sections -I$(TONC)/include -DPROTOCOL_$(PROTOCOL)
GCCLIB   := $(dir $(shell $(CC) -mcpu=arm7tdmi -mthumb -print-libgcc-file-name))
LDFLAGS  := $(ARCH) -mthumb -nostartfiles -T ld/gba_cart.ld -Wl,--gc-sections \
            -Wl,--no-warn-rwx-segments -Wl,-Map,$(B)/$(NAME).map

SRC_C    := $(wildcard src/*.c)
SRC_S    := $(wildcard src/*.s)
OBJS     := $(SRC_C:src/%.c=$(B)/obj/%.o) $(SRC_S:src/%.s=$(B)/obj/%.o)

.PHONY: all deps test test-v11b test-all baselines run clean
all: $(B)/$(NAME).gba

# ---- third-party: fetched at pinned commits, built here ------------------------------------
$(DEPS)/.stamp: scripts/fetch_deps.sh
	bash scripts/fetch_deps.sh $(DEPS)
	touch $@
deps: $(DEPS)/.stamp

# libtonc minus tte_iohook.c (needs devkitPro's newlib stdio hooks; we don't use printf) and pre1.3
$(B)/libtonc.a: $(DEPS)/.stamp
	@mkdir -p $(B)/tonc
	@set -e; for f in $$(find $(TONC)/src $(TONC)/asm \( -name '*.c' -o -name '*.s' \) | grep -v -e pre1.3 -e tte_iohook); do \
	  mode=-mthumb; case $$f in *.iwram.*|*.s) mode=-marm;; esac; \
	  x=; case $$f in *.s) x="-x assembler-with-cpp";; esac; \
	  $(CC) $(ARCH) -O2 $$mode -I$(TONC)/include $$x -c $$f -o $(B)/tonc/$$(basename $$f).o; \
	done
	$(AR) rcs $@ $(B)/tonc/*.o

$(B)/gba_crt0.o: $(DEPS)/.stamp
	@mkdir -p $(dir $@)
	$(CC) $(ARCH) -marm -x assembler-with-cpp -c $(CRT)/gba_crt0.s -o $@

$(B)/gbafix: $(DEPS)/.stamp
	@mkdir -p $(dir $@)
	gcc -O2 -w -o $@ $(DEPS)/gba-tools/src/gbafix.c

# ---- our code ------------------------------------------------------------------------------
# *.iwram.c: hot code for IWRAM, compiled as 32-bit ARM (the linker script places *iwram.* objects)
$(B)/obj/%.iwram.o: src/%.iwram.c $(wildcard src/*.h) $(DEPS)/.stamp
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -marm -c $< -o $@

$(B)/obj/%.o: src/%.c $(wildcard src/*.h) $(DEPS)/.stamp
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -mthumb -c $< -o $@

$(B)/obj/%.o: src/%.s
	@mkdir -p $(dir $@)
	$(CC) $(ARCH) -x assembler-with-cpp -c $< -o $@

$(B)/$(NAME).elf: $(B)/gba_crt0.o $(OBJS) $(B)/libtonc.a ld/gba_cart.ld
	$(CC) $(LDFLAGS) $(B)/gba_crt0.o $(GCCLIB)crti.o $(GCCLIB)crtbegin.o $(OBJS) \
	  -L$(B) -ltonc $(GCCLIB)crtend.o $(GCCLIB)crtn.o -o $@

$(B)/$(NAME).gba: $(B)/$(NAME).elf $(B)/gbafix
	$(OBJCOPY) -O binary $< $@
	$(B)/gbafix $@ -tWGC.TX.UI -cWGCU -m01 >/dev/null
	@echo "built $@ ($(PROTOCOL), $$(stat -c%s $@) bytes)"

# ---- test harness --------------------------------------------------------------------------
$(B)/emu_harness: tools/emu_harness.c
	@mkdir -p $(dir $@)
	gcc -O2 -o $@ $< -lmgba

# test: v1.1a build vs v1.1a stock recordings. test-all: also a v1.1b build (in build/v11b).
test: $(B)/$(NAME).gba $(B)/emu_harness
	python3 tests/run_tests.py $(B)/emu_harness $(B)/$(NAME).gba v1.1a

test-v11b: $(B)/emu_harness
	$(MAKE) --no-print-directory B=$(B)/v11b DEPS=$(DEPS) PROTOCOL=V11B $(B)/v11b/$(NAME).gba
	python3 tests/run_tests.py $(B)/emu_harness $(B)/v11b/$(NAME).gba v1.1b

test-all: test test-v11b

# Re-record tests/baselines/ from the stock ROMs (local only: reference/ is not in the repo)
baselines: $(B)/emu_harness
	python3 tests/record_baselines.py $(B)/emu_harness reference/stock-rom/GBA_TX_Cart_ROM_v1.1a.gba \
	  reference/stock-rom/GBA_TX_Cart_ROM_v1.1b.gba

run: $(B)/$(NAME).gba
	/usr/games/mgba-qt $< &

clean:
	rm -rf $(B)/obj $(B)/tonc $(B)/*.o $(B)/*.a $(B)/*.elf $(B)/*.gba $(B)/*.map $(B)/gbafix $(B)/emu_harness
