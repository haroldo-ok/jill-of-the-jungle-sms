# Headless SMS emulator used for automated playtesting of jill.sms
CFLAGS := -O2 -Wall
all: smstest test_signedcmp
smstest: smstest.c z80.c z80.h
	$(CC) $(CFLAGS) -o $@ smstest.c z80.c
test_signedcmp: test_signedcmp.c z80.c z80.h
	$(CC) $(CFLAGS) -o $@ test_signedcmp.c z80.c
clean:
	rm -f smstest test_signedcmp
.PHONY: all clean
