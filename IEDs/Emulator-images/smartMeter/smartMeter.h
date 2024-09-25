#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <pthread.h>
#include <signal.h>
#include <cstdint>

#define DEF_GOOSE_INTERFACE "eth0"
#define MMS_PORT 102

#define THREE_PHASES 3

#define IP_SIZE 16
#define INTERFACE_SIZE 100

struct smData {
    bool safeState;
    double* currents;
    double* voltages;
    pthread_mutex_t* dataLock;
} typedef SmartMeterData;

struct smModel {
    int16_t id;
    char* ip;
    char* goose_interface;
    int16_t dataPort;
    SmartMeterData data;
    int16_t safetyThreshold;
} typedef SmartMeterModel;