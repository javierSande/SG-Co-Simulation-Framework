#!/bin/bash

if [ -z "$DA_ID" ]; then
    echo "Error: DA_ID environment variable is not set."
    exit 1
fi

if [ -z "$DA_IP" ]; then
    echo "Error: DA_IP environment variable is not set."
    exit 1
fi

if [ -z "$DA_GATEWAY" ]; then
    echo "Error: DA_GATEWAY environment variable is not set."
    exit 1
fi

if [ -z "$S_IP" ]; then
    echo "Error: S_IP environment variable is not set."
    exit 1
fi

if [ -z "$S_PORT_LINE" ]; then
    echo "Error: S_PORT_LINE environment variable is not set."
    exit 1
fi

if [ -z "$S_PORT_LINE_ST" ]; then
    echo "Error: S_PORT_LINE_ST environment variable is not set."
    exit 1
fi

if [ -z "$S_PORT_CURRENTS" ]; then
    echo "Error: S_PORT_CURRENTS environment variable is not set."
    exit 1
fi

if [ -z "$S_PORT_BREAKERS" ]; then
    echo "Error: S_PORT_BREAKERS environment variable is not set."
    exit 1
fi

echo "Generating model for DA $DA_ID..."
./modelGenerator $DA_ID $DA_IP $DA_GATEWAY && \
make model &>/dev/null && \
echo "Compiling SM $DA_ID..." && \
make &>/dev/null && \
./detectionAlgorithm $DA_ID $S_IP $S_PORT_LINE $S_PORT_LINE_ST $S_PORT_CURRENTS $S_PORT_BREAKERS