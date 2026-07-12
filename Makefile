CC       := sdcc
IHX2SMS  := ./ihx2sms
SMSLIB   := SMSlib
CRT0     := crt0_sms.rel
PROG     := jill

include gen/banks.mk
BANKRELS := $(addprefix build/bank,$(addsuffix .rel,$(BANKS)))

CFLAGS   := -mz80 --peep-file $(SMSLIB)/peep-rules.txt -I$(SMSLIB) -Igen
LDFLAGS  := -mz80 --no-std-crt0 --data-loc 0xC000 $(BANKFLAGS)

all: $(PROG).sms

assets:
	python3 tools/convert_jill.py

build/main.rel: src/main.c gen/game_data.h
	@mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@

build/game_data.rel: gen/game_data.c gen/game_data.h
	@mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@

build/bank%.rel: gen/bank%.c gen/game_data.h
	@mkdir -p build
	$(CC) -mz80 --constseg BANK$* -Igen -ISMSlib -c $< -o $@

$(PROG).ihx: build/main.rel build/game_data.rel $(BANKRELS)
	$(CC) -o $@ $(LDFLAGS) $(CRT0) build/main.rel build/game_data.rel $(SMSLIB)/SMSlib.lib $(BANKRELS)

$(PROG).sms: $(PROG).ihx
	$(IHX2SMS) $< $@

test: assets
	python3 tools/test_jill_data.py

fulltest: assets
	python3 tools/playtest_static.py

clean:
	rm -rf build $(PROG).ihx $(PROG).sms $(PROG).map $(PROG).noi $(PROG).lk $(PROG).sym $(PROG).asm $(PROG).lst $(PROG).rel $(PROG).rst

.PHONY: all assets test clean
