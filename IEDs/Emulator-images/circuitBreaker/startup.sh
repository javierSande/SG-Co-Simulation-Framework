#!/bin/bash

if [ -z "$CB_ID" ]; then
    echo "Error: CB_ID environment variable is not set."
    exit 1
fi

if [ -z "$CB_IP" ]; then
    echo "Error: CB_IP environment variable is not set."
    exit 1
fi

if [ -z "$CB_GATEWAY" ]; then
    echo "Error: CB_GATEWAY environment variable is not set."
    exit 1
fi

if [ -z "$S_IP" ]; then
    echo "Error: S_IP environment variable is not set."
    exit 1
fi

if [ -z "$S_PORT" ]; then
    echo "Error: S_PORT environment variable is not set."
    exit 1
fi

echo "Generating model for CB $CB_ID..."
./modelGenerator $CB_ID $CB_IP $CB_GATEWAY && \
make model &>/dev/null && \
echo "Compiling CB $CB_ID..." && \
make &>/dev/null && \
./circuitBreaker $CB_ID $S_IP $S_PORT