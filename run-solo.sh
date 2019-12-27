#!/usr/bin/env bash
STRATUM_CONFIG=equihash.conf
# Build 
make -C iniparser -j$(nproc)
make -C sha3 -j$(nproc)
make -C algos -j$(nproc)
make -f Makefile -j$(nproc)
# Prepare working directory
mkdir -p workdir
if [ ! -f "workdir/${STRATUM_CONFIG}" ]; then
    cp config.sample/${STRATUM_CONFIG} workdir/${STRATUM_CONFIG}
    echo "${STRATUM_CONFIG} created"
fi

cd workdir
../stratum equihash

