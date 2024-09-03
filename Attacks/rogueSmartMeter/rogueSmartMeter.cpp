#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <pthread.h>
#include <signal.h>
#include <cstdint>

#include "hal_thread.h"
#include "static_model.h"
#include "iec61850_server.h"
#include "ladder.h"

#define GOOSE_INTERFACE "eth0"

#define MIN_VOLTAGE -100
#define MAX_VOLTAGE 100

pthread_mutex_t lock;

int16_t iA, iB, iC, vA, vB, vC;

bool safeCircuit = true;

IedServer iedServer = NULL;

static int running = 0;

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
update_server(uint64_t timestamp) {

    Timestamp iecTimestamp;

    Timestamp_clearFlags(&iecTimestamp);
    Timestamp_setTimeInMilliseconds(&iecTimestamp, timestamp);
    Timestamp_setLeapSecondKnown(&iecTimestamp, true);

    IedServer_lockDataModel(iedServer);
    pthread_mutex_lock(&lock);

    IedServer_updateTimestampAttributeValue(iedServer, IEDMODEL_LogicalDevice_GGIO1_AnVA_t, &iecTimestamp);
    IedServer_updateInt64AttributeValue(iedServer, IEDMODEL_LogicalDevice_GGIO1_AnVA_mag_f, vA);

    IedServer_updateTimestampAttributeValue(iedServer, IEDMODEL_LogicalDevice_GGIO1_AnVB_t, &iecTimestamp);
    IedServer_updateInt64AttributeValue(iedServer, IEDMODEL_LogicalDevice_GGIO1_AnVB_mag_f, vB);

    IedServer_updateTimestampAttributeValue(iedServer, IEDMODEL_LogicalDevice_GGIO1_AnVC_t, &iecTimestamp);
    IedServer_updateInt64AttributeValue(iedServer, IEDMODEL_LogicalDevice_GGIO1_AnVC_mag_f, vC);

    IedServer_updateTimestampAttributeValue(iedServer, IEDMODEL_LogicalDevice_GGIO1_AnIA_t, &iecTimestamp);
    IedServer_updateInt64AttributeValue(iedServer, IEDMODEL_LogicalDevice_GGIO1_AnIA_mag_f, iA);

    IedServer_updateTimestampAttributeValue(iedServer, IEDMODEL_LogicalDevice_GGIO1_AnIB_t, &iecTimestamp);
    IedServer_updateInt64AttributeValue(iedServer, IEDMODEL_LogicalDevice_GGIO1_AnIB_mag_f, iB);

    IedServer_updateTimestampAttributeValue(iedServer, IEDMODEL_LogicalDevice_GGIO1_AnIC_t, &iecTimestamp);
    IedServer_updateInt64AttributeValue(iedServer, IEDMODEL_LogicalDevice_GGIO1_AnIC_mag_f, iC);

    IedServer_updateTimestampAttributeValue(iedServer, IEDMODEL_LogicalDevice_GGIO1_SecurityST_t, &iecTimestamp);
    IedServer_updateBooleanAttributeValue(iedServer, IEDMODEL_LogicalDevice_GGIO1_SecurityST_stVal, safeCircuit);

    pthread_mutex_unlock(&lock);
    IedServer_unlockDataModel(iedServer);
}

void
*exchangeMMSData(void *args)
{
    struct timespec timer_start;
    clock_gettime(CLOCK_MONOTONIC, &timer_start);

    while(running) {
        uint64_t timestamp = Hal_getTimeInMs();

        update_server(timestamp);

        //sleep_until(&timer_start, OPLC_CYCLE);
        Thread_sleep(2000);
    }
}

void 
displayData(char *iedName)
{
    printf("%s:\n", iedName);
    printf("\tVoltage: %d\t\t%d\t\t%d\n", iA, iB, iC);
    printf("\tCurrent: %d\t\t%d\t\t%d\n", vA, vB, vC);
    printf("\tSafe: %d\n", safeCircuit);
}

int main(int argc, char* argv[]) {

    /* Parse parameters */
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <SM_ID> <Simulink_IP>" << std::endl;
        return 1;
    }

    int smId = atoi(argv[1]);
    const char* simulinkIp = argv[2];

    char smName[10];
    snprintf(smName, 10, "SM%02d", smId);

    printf("Using GOOSE interface: %s\n", GOOSE_INTERFACE);

    /* Setup iec61850 MMS server */
    int port = 102;
    setUpMMSServer(port, GOOSE_INTERFACE);

    if (!IedServer_isRunning(iedServer)) {
        printf("Error: Could not setup MMS server\n");
        IedServer_destroy(iedServer);
        exit(-1);
    }

    /* Start data exchange */

    running = 1;

    //signal(SIGINT, sigint_handler);

    pthread_t receivingThread;
    pthread_create(&receivingThread, NULL, exchangeMMSData, NULL);

    /* Update data */
    while(running)
    {

        int16_t min, max;
        printf("Min value of currents to be sent: ");
        scanf("%d", &min);
        printf("Max value of currents to be sent: ");
        scanf("%d", &max);

        bool safe;
        printf("Safe circuit [0/1]: ");
        scanf("%d", &safe);

        pthread_mutex_lock(&lock);
        iA = min + rand() % (max - min + 1);
        iB = min + rand() % (max - min + 1);
        iC = min + rand() % (max - min + 1);

        vA = min + rand() % (MAX_VOLTAGE - MIN_VOLTAGE + 1);
        vB = min + rand() % (MAX_VOLTAGE - MIN_VOLTAGE + 1);
        vC = min + rand() % (MAX_VOLTAGE - MIN_VOLTAGE + 1);

        safeCircuit = safe;

        displayData(smName);

        pthread_mutex_unlock(&lock);

        sleep_mss(1000);
    }

    closeMMSServer();
}