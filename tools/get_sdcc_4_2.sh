#!/bin/sh
# Optional helper for local builds if sdcc is not already installed.
# Downloads the official SDCC 4.2.0 Linux x86_64 binary kit into /tmp/sdcc-4.2.0
# and prints a PATH export command.  Review the URL/checksums for your own CI.
set -eu
mkdir -p /tmp/sdcc-download
cd /tmp/sdcc-download
wget -O sdcc.tar.bz2 'https://sourceforge.net/projects/sdcc/files/sdcc-linux-amd64/4.2.0/sdcc-4.2.0-amd64-unknown-linux2.5.tar.bz2/download'
tar -xjf sdcc.tar.bz2 -C /tmp
echo 'export PATH=/tmp/sdcc-4.2.0/bin:$PATH'
