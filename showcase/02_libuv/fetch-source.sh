#!/bin/sh
# fetch-source.sh — download libuv 1.50.0 source into prj/
set -eu

VERSION="1.50.0"
TARBALL="libuv-v${VERSION}.tar.gz"
URL="https://dist.libuv.org/dist/v${VERSION}/libuv-v${VERSION}.tar.gz"
SRCDIR="libuv-v${VERSION}"

cd "$(dirname "$0")"

if [ -d "prj/src" ]; then
    echo "Source already fetched (prj/src/ exists), skipping download."
    exit 0
fi

echo "Downloading libuv ${VERSION}..."
curl -LO "${URL}"

echo "Extracting..."
tar xzf "${TARBALL}"

mkdir -p prj
mv "${SRCDIR}/src" prj/
mv "${SRCDIR}/include" prj/
mv "${SRCDIR}/test" prj/
mv "${SRCDIR}/README.md" prj/

rm -rf "${SRCDIR}" "${TARBALL}"
echo "Done. Sources in prj/{src,include,test}/"
echo ""
echo "To build:  cc -std=c11 -w build.c -o b -lpthread && ./b build"
