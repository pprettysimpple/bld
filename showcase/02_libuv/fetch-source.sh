#!/bin/sh
# fetch-source.sh — download libuv 1.50.0 source
set -eu

VERSION="1.50.0"
TARBALL="libuv-v${VERSION}.tar.gz"
URL="https://dist.libuv.org/dist/v${VERSION}/libuv-v${VERSION}.tar.gz"
SRCDIR="libuv-v${VERSION}"

cd "$(dirname "$0")"

if [ -d "src" ]; then
    echo "Source already fetched (src/ exists), skipping download."
    exit 0
fi

echo "Downloading libuv ${VERSION}..."
curl -LO "${URL}"

echo "Extracting..."
tar xzf "${TARBALL}"

mv "${SRCDIR}/src" .
mv "${SRCDIR}/include" .
mv "${SRCDIR}/test" .
mv "${SRCDIR}/README.md" .

rm -rf "${SRCDIR}" "${TARBALL}"
echo "Done. Sources in src/, headers in include/"
echo ""
echo "To build:  cc -std=c11 -w build.c -o b -lpthread && ./b build"
