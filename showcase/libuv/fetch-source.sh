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
else
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
fi

# bld.h — copy amalgamated header if missing
if [ ! -e "bld.h" ]; then
    if [ -e "../../out/include/bld.h" ]; then
        cp "../../out/include/bld.h" bld.h
    else
        echo "Warning: bld.h not found. Run './b build' in repo root first, or copy amalgamated bld.h here."
    fi
fi
