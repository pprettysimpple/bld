#!/bin/sh
# fetch-source.sh — download curl 8.12.0 source
set -eu

VERSION="8.12.0"
TARBALL="curl-${VERSION}.tar.gz"
URL="https://curl.se/download/${TARBALL}"
SRCDIR="curl-${VERSION}"

cd "$(dirname "$0")"

if [ -d "lib" ]; then
    echo "Source already fetched (lib/ exists), skipping download."
    exit 0
fi

echo "Downloading curl ${VERSION}..."
curl -LO "${URL}"

echo "Extracting..."
tar xzf "${TARBALL}"

mv "${SRCDIR}/lib" .
mv "${SRCDIR}/src" .
mv "${SRCDIR}/include" .
mv "${SRCDIR}/docs" .

rm -rf "${SRCDIR}" "${TARBALL}"
echo "Done. Sources in lib/, src/, include/"
echo ""
echo "To build:  cc -std=c11 -w build.c -o b -lpthread && ./b build"
