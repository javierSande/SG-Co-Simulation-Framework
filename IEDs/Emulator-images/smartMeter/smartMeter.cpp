#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <pthread.h>
#include <signal.h>
#include <cstdint>

#include <simlink.h>

#include "hal_thread.h"
#include "static_model.h"
#include "iec61850_server.h"
#include "ladder.h"

#define GOOSE_INTERFACE "eth0"

SimLinkModel model;

IedServer iedServer = NULL;

static int running = 0;

bool* safeCircuit = NULL;

char config_file[128];

void
sleep_mss(int milliseconds)
{
    struct timespec ts = {milliseconds / 1000, (milliseconds % 1000) * 1000000};
    nanosleep(&ts, NULL);
}

/*
* MMS methods
*/

static void
rcbEventHandler(void* parameter, ReportControlBlock* rcb, ClientConnection connection, IedServer_RCBEventType event, const char* parameterName, MmsDataAccessError serviceError)
{
    //printf("RCB: %s event: %i\n", ReportControlBlock_getName(rcb), event);

    if ((event == RCB_EVENT_SET_PARAMETER) || (event == RCB_EVENT_GET_PARAMETER)) {
        printf("  param:  %s\n", parameterName);
        printf("  result: %i\n", serviceError);
    }

    if (event == RCB_EVENT_ENABLE) {
        char* rptId = ReportControlBlock_getRptID(rcb);
        printf("   rptID:  %s\n", rptId);
        char* dataSet = ReportControlBlock_getDataSet(rcb);
        printf("   datSet: %s\n", dataSet);

        free(rptId);
        free(dataSet);
    }
}

static void
connectionHandler (IedServer self, ClientConnection connection, bool connected, void* parameter)
{
    if (connected)
        printf("Connection opened\n");
    else
        printf("Connection closed\n");
}

static ControlHandlerResult
controlHandler(ControlAction action, void* parameter, MmsValue* value, bool test) {
    if (test)
        return CONTROL_RESULT_FAILED;

    std::string &node_string = *(static_cast<std::string*>(parameter));

    printf("ControlHandler called for %s\n", node_string.c_str());
    //log(log_msg_iecserver);

    //write_to_address(value, serverside_mapping[node_string]);
}

static void
goCbEventHandler(MmsGooseControlBlock goCb, int event, void* parameter)
{
    printf("Access to GoCB: %s\n", MmsGooseControlBlock_getName(goCb));
    printf("         GoEna: %i\n", MmsGooseControlBlock_getGoEna(goCb));
}

void
setUpMMSServer(int port, char* gooseInterface)
{
    printf("Starting IEC61850SERVER\n");

    /* Create new server configuration object */
    IedServerConfig config = IedServerConfig_create();

    /* Set buffer size for buffered report control blocks to 200000 bytes */
    IedServerConfig_setReportBufferSize(config, 200000);

    /* Set stack compliance to a specific edition of the standard (WARNING: data model has also to be checked for compliance) */
    IedServerConfig_setEdition(config, IEC_61850_EDITION_2);

    /* Set the base path for the MMS file services */
    IedServerConfig_setFileServiceBasePath(config, "./vmd-filestore/");

    /* disable MMS file service */
    IedServerConfig_enableFileService(config, false);

    /* enable dynamic data set service */
    IedServerConfig_enableDynamicDataSetService(config, true);

    /* disable log service */
    IedServerConfig_enableLogService(config, false);

    /* set maximum number of clients */
    IedServerConfig_setMaxMmsConnections(config, 2);

    /* Create a new IEC 61850 server instance */
    iedServer = IedServer_createWithConfig(&iedModel, NULL, config);

    IedServer_setConnectionIndicationHandler(iedServer, (IedConnectionIndicationHandler) connectionHandler, NULL);

    IedServer_setRCBEventHandler(iedServer, rcbEventHandler, NULL);

    //IedServer_setWriteAccessPolicy(iedServer, IEC61850_FC_ALL, ACCESS_POLICY_ALLOW);

    /* set GOOSE interface for all GOOSE publishers (GCBs) */
    IedServer_setGooseInterfaceId(iedServer, gooseInterface);
    IedServer_setGooseInterfaceIdEx(iedServer, IEDMODEL_LogicalDevice_LLN0, "securityEvent", gooseInterface);

    IedServer_setGoCBHandler(iedServer, goCbEventHandler, NULL);

    //start server
    IedServer_start(iedServer, port);

    /* Start GOOSE publishing */
    IedServer_enableGoosePublishing(iedServer);
}

void
closeMMSServer()
{
    IedServer_stop(iedServer);
    IedServer_destroy(iedServer);
}

void
update_server(uint64_t timestamp, SimLinkModel* model) {

    Timestamp iecTimestamp;

    Timestamp_clearFlags(&iecTimestamp);
    Timestamp_setTimeInMilliseconds(&iecTimestamp, timestamp);
    Timestamp_setLeapSecondKnown(&iecTimestamp, true);

    IedServer_lockDataModel(iedServer);
    pthread_mutex_lock(&model->lock);

    IedServer_updateTimestampAttributeValue(iedServer, IEDMODEL_LogicalDevice_GGIO1_AnVA_t, &iecTimestamp);
    IedServer_updateInt16AttributeValue(iedServer, IEDMODEL_LogicalDevice_GGIO1_AnVA_mag_f, model->stationsData->analogIn[0]);

    IedServer_updateTimestampAttributeValue(iedServer, IEDMODEL_LogicalDevice_GGIO1_AnVB_t, &iecTimestamp);
    IedServer_updateInt16AttributeValue(iedServer, IEDMODEL_LogicalDevice_GGIO1_AnVB_mag_f, model->stationsData->analogIn[1]);

    IedServer_updateTimestampAttributeValue(iedServer, IEDMODEL_LogicalDevice_GGIO1_AnVC_t, &iecTimestamp);
    IedServer_updateInt16AttributeValue(iedServer, IEDMODEL_LogicalDevice_GGIO1_AnVC_mag_f, model->stationsData->analogIn[2]);

    IedServer_updateTimestampAttributeValue(iedServer, IEDMODEL_LogicalDevice_GGIO1_AnIA_t, &iecTimestamp);
    IedServer_updateInt16AttributeValue(iedServer, IEDMODEL_LogicalDevice_GGIO1_AnIA_mag_f, model->stationsData->analogIn[3]);

    IedServer_updateTimestampAttributeValue(iedServer, IEDMODEL_LogicalDevice_GGIO1_AnIB_t, &iecTimestamp);
    IedServer_updateInt16AttributeValue(iedServer, IEDMODEL_LogicalDevice_GGIO1_AnIB_mag_f, model->stationsData->analogIn[4]);

    IedServer_updateTimestampAttributeValue(iedServer, IEDMODEL_LogicalDevice_GGIO1_AnIC_t, &iecTimestamp);
    IedServer_updateInt16AttributeValue(iedServer, IEDMODEL_LogicalDevice_GGIO1_AnIC_mag_f, model->stationsData->analogIn[5]);

    IedServer_updateTimestampAttributeValue(iedServer, IEDMODEL_LogicalDevice_GGIO1_SecurityST_t, &iecTimestamp);
    IedServer_updateBooleanAttributeValue(iedServer, IEDMODEL_LogicalDevice_GGIO1_SecurityST_stVal, *safeCircuit);

    pthread_mutex_unlock(&model->lock);
    IedServer_unlockDataModel(iedServer);
}

void
*exchangeMMSData(void *args)
{
    SimLinkModel* model = (SimLinkModel*)args;
    struct timespec timer_start;
    clock_gettime(CLOCK_MONOTONIC, &timer_start);

    while(running) {
        uint64_t timestamp = Hal_getTimeInMs();

        update_server(timestamp, model);

        //sleep_until(&timer_start, OPLC_CYCLE);
        Thread_sleep(2000);
    }
}

void 
*displayData(void *args)
{
    char* iedName = (char*)args;
    while(running)
    {
        pthread_mutex_lock(&model.lock);
        printf("%s:\n", iedName);
        printf("\tVoltage: %d\t\t%d\t\t%d\n", model.stationsData[0].analogIn[0], model.stationsData[0].analogIn[1], model.stationsData[0].analogIn[2]);
        printf("\tCurrent: %d\t\t%d\t\t%d\n", model.stationsData[0].analogIn[3], model.stationsData[0].analogIn[4], model.stationsData[0].analogIn[5]);
        printf("\tSafe: %d\n", *safeCircuit);
        pthread_mutex_unlock(&model.lock);

        sleep_mss(3000);
    }
}

int main(int argc, char* argv[]) {

    /* Parse parameters */
    if (argc != 6) {
        std::cerr << "Usage: " << argv[0] << " <SM_ID> <Simulink_IP> <Port_V> <Port_I> <safe_threshold>" << std::endl;
        return 1;
    }

    int smId = atoi(argv[1]);
    const char* simulinkIp = argv[2];
    const int simulinkPortV = atoi(argv[3]);
    const int simulinkPortI = atoi(argv[4]);
    const int safeThreshold = atoi(argv[5]);

    printf("Using GOOSE interface: %s\n", GOOSE_INTERFACE);

    /* Setup iec61850 MMS server */
    int port = 102;
    setUpMMSServer(port, GOOSE_INTERFACE);

    if (!IedServer_isRunning(iedServer)) {
        printf("Error: Could not setup MMS server\n");
        IedServer_destroy(iedServer);
        exit(-1);
    }

    /* Setup Simulink model */

    strncpy(model.simulinkIp, simulinkIp, 100);
    model.numStations = 1;
    model.commDelay = 250;

    model.stationsData = (StationData *)malloc(model.numStations * sizeof(StationData));
    model.stationsInfo = (StationInfo *)malloc(model.numStations * sizeof(StationInfo));

    // Voltage ports
    model.stationsInfo[0].analogInPorts[0] = simulinkPortV;
    model.stationsInfo[0].analogInPorts[1] = simulinkPortV + 1;
    model.stationsInfo[0].analogInPorts[2] = simulinkPortV + 2;

    // Current ports
    model.stationsInfo[0].analogInPorts[3] = simulinkPortI;
    model.stationsInfo[0].analogInPorts[4] = simulinkPortI + 1;
    model.stationsInfo[0].analogInPorts[5] = simulinkPortI + 2;

    safeCircuit = &model.stationsData[0].digitalOut[0];

    *safeCircuit = true;

    /* Display info */
    displayInfo(&model);

    /* Start data exchange */

    running = 1;

    //signal(SIGINT, sigint_handler);

    exchangeDataWithSimulink(&model);

    pthread_t receivingThread;
    pthread_create(&receivingThread, NULL, exchangeMMSData, &model);

    /* Display data */
    char smName[10];
    snprintf(smName, 10, "SM%02d", smId);

    pthread_t displayThread;
    pthread_create(&displayThread, NULL, displayData, (void*)smName);

    int safetyCounter = 0;
    /* Update data */
    while(running)
    {
        pthread_mutex_lock(&model.lock);

        int16_t iA = model.stationsData->analogIn[3];
        int16_t iB = model.stationsData->analogIn[4];
        int16_t iC = model.stationsData->analogIn[5];

        bool newState = !(iA > safeThreshold || iB > safeThreshold || iC > safeThreshold);

        if (!*safeCircuit && newState && safetyCounter < 3)
        {
            safetyCounter++;
        }
        else
        {
            *safeCircuit = newState;
            safetyCounter = 0;
        }
        pthread_mutex_unlock(&model.lock);

        // In case of unsafe circuit, wait for 3s to stabilize
        if (!*safeCircuit)
            sleep_mss(3000);

        sleep_mss(1000);
    }

    closeMMSServer();
}