#!/bin/sh
# fetch-source.sh — download curl 8.12.0 source into prj/
set -eu

VERSION="8.12.0"
TARBALL="curl-${VERSION}.tar.gz"
URL="https://curl.se/download/${TARBALL}"
SRCDIR="curl-${VERSION}"

cd "$(dirname "$0")"

if [ -d "prj/lib" ]; then
    echo "Source already fetched (prj/lib/ exists), skipping download."
    exit 0
fi

echo "Downloading curl ${VERSION}..."
curl -LO "${URL}"

echo "Extracting..."
tar xzf "${TARBALL}"

mkdir -p prj
mv "${SRCDIR}/lib" prj/
mv "${SRCDIR}/src" prj/
mv "${SRCDIR}/include" prj/
mv "${SRCDIR}/docs" prj/

rm -rf "${SRCDIR}" "${TARBALL}"
echo "Done. Sources in prj/{lib,src,include,docs}/"
echo ""
echo "To build:  cc -std=c11 -w build.c -o b -lpthread && ./b build"
