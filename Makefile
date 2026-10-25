CC       := sdcc
IHX2SMS  := python3 tools/ihx2rom.py
SMSLIB   := SMSlib
CRT0     := crt0_sms.rel
PROG     := jill

include gen/banks.mk
BANKRELS := $(addprefix build/bank,$(addsuffix .rel,$(BANKS)))

CFLAGS   := -mz80 --peep-file $(SMSLIB)/peep-rules.txt -I$(SMSLIB) -Igen
LDFLAGS  := -mz80 --no-std-crt0 --data-loc 0xC000 $(BANKFLAGS) -Wl-b_OVLA=0x1E4000 -Wl-b_OVLB=0x1F4000

all: $(PROG).sms

assets:
	python3 tools/convert_jill.py

build/main.asm: src/main.c src/*.inc gen/game_data.h
	@mkdir -p build
	$(CC) $(CFLAGS) -S $< -o $@

# main.c is one translation unit; its functions are distributed over the resident
# area and the two slot-1 code overlays at the assembly level
build/main_ovl.rel: build/main.asm tools/split_overlays.py
	python3 tools/split_overlays.py $< build/main_ovl.asm
	sdasz80 -plosgff build/main_ovl.asm

build/game_data.rel: gen/game_data.c gen/game_data.h
	@mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@

build/bank%.rel: gen/bank%.c gen/game_data.h
	@mkdir -p build
	$(CC) -mz80 --constseg BANK$* -Igen -ISMSlib -c $< -o $@

$(PROG).ihx: build/main_ovl.rel build/game_data.rel $(BANKRELS)
	$(CC) -o $@ $(LDFLAGS) $(CRT0) build/main_ovl.rel build/game_data.rel $(SMSLIB)/SMSlib.lib $(BANKRELS)

$(PROG).sms: $(PROG).ihx
	$(IHX2SMS) $< $@

test: assets
	python3 tools/test_jill_data.py

# Build the headless SMS emulator used for automated playtesting.
tools/smstest/smstest:
	$(MAKE) -C tools/smstest

# Automated playtest: boots the real ROM in an emulator, drives the controller,
# and asserts on live game state via the SDCC .noi symbols.  CI gate.
playtest: $(PROG).sms tools/smstest/smstest
	@mkdir -p shots
	tools/smstest/smstest $(PROG).sms tests/playtest.txt

fulltest: assets
	python3 tools/playtest_static.py

clean:
	$(MAKE) -C tools/smstest clean
	rm -rf build shots $(PROG).ihx $(PROG).sms $(PROG).map $(PROG).noi $(PROG).lk $(PROG).sym $(PROG).asm $(PROG).lst $(PROG).rel $(PROG).rst

.PHONY: all assets test playtest clean

# Debug build with every function/variable visible in the symbol map (for the playtester).
debug:
	@mkdir -p build_dbg
	$(CC) $(CFLAGS) -DDEBUGSYMS -S src/main.c -o build_dbg/main.asm
	python3 tools/split_overlays.py build_dbg/main.asm build_dbg/main_ovl.asm
	sdasz80 -plosgff build_dbg/main_ovl.asm
	$(CC) -o jilldbg.ihx $(LDFLAGS) $(CRT0) build_dbg/main_ovl.rel build/game_data.rel $(SMSLIB)/SMSlib.lib $(BANKRELS)
	$(IHX2SMS) jilldbg.ihx jilldbg.sms
