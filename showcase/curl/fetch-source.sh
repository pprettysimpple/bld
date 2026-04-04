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
else
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
fi

# generated/ contains pre-made curl_config.h and curl_checks.h
# These are committed alongside this script.

# bld.h — copy amalgamated header if missing
if [ ! -e "bld.h" ]; then
    if [ -e "../../out/include/bld.h" ]; then
        cp "../../out/include/bld.h" bld.h
    else
        echo "Warning: bld.h not found. Run './b build' in repo root first, or copy amalgamated bld.h here."
    fi
fi
