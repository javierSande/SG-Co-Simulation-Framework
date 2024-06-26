#!/bin/bash

if [ -z "$SM_ID" ]; then
    echo "Error: SM_ID environment variable is not set."
    exit 1
fi

if [ -z "$SM_IP" ]; then
    echo "Error: SM_IP environment variable is not set."
    exit 1
fi

if [ -z "$SM_GATEWAY" ]; then
    echo "Error: SM_GATEWAY environment variable is not set."
    exit 1
fi

if [ -z "$S_IP" ]; then
    echo "Error: S_IP environment variable is not set."
    exit 1
fi

if [ -z "$S_PORTV" ]; then
    echo "Error: S_PORTV environment variable is not set."
    exit 1
fi

if [ -z "$S_PORTI" ]; then
    echo "Error: S_PORTI environment variable is not set."
    exit 1
fi

if [ -z "$SAFE_THRESHOLD" ]; then
    echo "Error: SAFE_THRESHOLD environment variable is not set."
    exit 1
fi

echo "Generating model for SM $SM_ID..."
./modelGenerator $SM_ID $SM_IP $SM_GATEWAY && \
make model &>/dev/null && \
echo "Compiling SM $SM_ID..." && \
make &>/dev/null && \
./smartMeter $SM_ID $S_IP $S_PORTV $S_PORTI $SAFE_THRESHOLD