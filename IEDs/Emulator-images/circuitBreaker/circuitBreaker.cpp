#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <pthread.h>
#include <cstdint>

#include <simlink.h>

#include "hal_thread.h"
#include "static_model.h"
#include "iec61850_server.h"
#include "goose_receiver.h"
#include "goose_subscriber.h"
#include "ladder.h"

#define GOOSE_INTERFACE "eth0"

SimLinkModel model;

static int running = 0;

IedServer iedServer = NULL;
GooseReceiver gooseReceiver = NULL;

static bool automaticOperationMode = true;
static ClientConnection controllingClient = NULL;

char config_file[128];

bool operatorSt = false;
bool securitySt = true;

bool *breakerSt = NULL;

void
sleep_mss(int milliseconds)
{
    struct timespec ts = {milliseconds / 1000, (milliseconds % 1000) * 1000000};
    nanosleep(&ts, NULL);
}

static void
updateBreaker(uint64_t timeStamp) {

    bool newState = securitySt || operatorSt;
    *breakerSt = newState;

    IedServer_updateUTCTimeAttributeValue(iedServer, IEDMODEL_LogicalDevice_GGIO1_CircuitBreaker_t, timeStamp);
    IedServer_updateBooleanAttributeValue(iedServer, IEDMODEL_LogicalDevice_GGIO1_CircuitBreaker_stVal, newState);
    IedServer_updateQuality(iedServer, IEDMODEL_LogicalDevice_GGIO1_CircuitBreaker_q, QUALITY_VALIDITY_GOOD | QUALITY_SOURCE_SUBSTITUTED);
}

static void
updateSecurityStVal(bool newState, uint64_t timeStamp) {

    pthread_mutex_lock(&model.lock);
    securitySt = newState;
    updateBreaker(timeStamp);
    pthread_mutex_unlock(&model.lock);

    IedServer_updateUTCTimeAttributeValue(iedServer, IEDMODEL_LogicalDevice_GGIO1_Security_t, timeStamp);
    IedServer_updateBooleanAttributeValue(iedServer, IEDMODEL_LogicalDevice_GGIO1_Security_stVal, newState);
    IedServer_updateQuality(iedServer, IEDMODEL_LogicalDevice_GGIO1_Security_q, QUALITY_VALIDITY_GOOD | QUALITY_SOURCE_SUBSTITUTED);
}

static void
updateOperatorStVal(bool newState, uint64_t timeStamp) {

    pthread_mutex_lock(&model.lock);
    operatorSt = newState;
    updateBreaker(timeStamp);
    pthread_mutex_unlock(&model.lock);

    IedServer_updateUTCTimeAttributeValue(iedServer, IEDMODEL_LogicalDevice_GGIO1_Operator_t, timeStamp);
    IedServer_updateBooleanAttributeValue(iedServer, IEDMODEL_LogicalDevice_GGIO1_Operator_stVal, newState);
    IedServer_updateQuality(iedServer, IEDMODEL_LogicalDevice_GGIO1_Operator_q, QUALITY_VALIDITY_GOOD | QUALITY_SOURCE_SUBSTITUTED);
}

static ControlHandlerResult
controlHandlerForBinaryOutput(ControlAction action, void* parameter, MmsValue* value, bool test)
{
    if (test)
        return CONTROL_RESULT_OK;

    uint64_t timeStamp = Hal_getTimeInMs();

    bool newState = MmsValue_getBoolean(value);

    if (parameter == IEDMODEL_LogicalDevice_GGIO1_Security)
        updateSecurityStVal(newState, timeStamp);

    if (parameter == IEDMODEL_LogicalDevice_GGIO1_Operator)
        updateOperatorStVal(newState, timeStamp);

    return CONTROL_RESULT_OK;
}

static CheckHandlerResult
performCheckHandler(ControlAction action, void* parameter, MmsValue* ctlVal, bool test, bool interlockCheck, ClientConnection connection)
{
    if (controllingClient == NULL) {
        printf("Client takes control -> switch to remote control operation mode\n");
        controllingClient = connection;
        automaticOperationMode = false;
    }

    /* test command not accepted if mode is "on" */
    if (test)
        return CONTROL_TEMPORARILY_UNAVAILABLE;

    /* If there is already another client that controls the device reject the control attempt */
    if (controllingClient == connection)
        return CONTROL_ACCEPTED;
    else
        return CONTROL_TEMPORARILY_UNAVAILABLE;
}

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

void
setUpMMSServer(int port)
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

    IedServer_setWriteAccessPolicy(iedServer, IEC61850_FC_ALL, ACCESS_POLICY_ALLOW);

    //set_control_handlers();

    IedServer_setPerformCheckHandler(iedServer, IEDMODEL_LogicalDevice_GGIO1_Security,
        (ControlPerformCheckHandler) performCheckHandler, IEDMODEL_LogicalDevice_GGIO1_Security);

	IedServer_setControlHandler(iedServer, IEDMODEL_LogicalDevice_GGIO1_Security, (ControlHandler) controlHandlerForBinaryOutput,
	            IEDMODEL_LogicalDevice_GGIO1_Security);

    IedServer_setPerformCheckHandler(iedServer, IEDMODEL_LogicalDevice_GGIO1_Operator,
        (ControlPerformCheckHandler) performCheckHandler, IEDMODEL_LogicalDevice_GGIO1_Operator);

	IedServer_setControlHandler(iedServer, IEDMODEL_LogicalDevice_GGIO1_Operator, (ControlHandler) controlHandlerForBinaryOutput,
	            IEDMODEL_LogicalDevice_GGIO1_Operator);

    /* Initialize process values */
	MmsValue* Security_stVal = IedServer_getAttributeValue(iedServer, IEDMODEL_LogicalDevice_GGIO1_Security_stVal);
	MmsValue_setBitStringFromInteger(Security_stVal, 1); /* set Security Break to OFF */

    MmsValue* Operator_stVal = IedServer_getAttributeValue(iedServer, IEDMODEL_LogicalDevice_GGIO1_Operator_stVal);
	MmsValue_setBitStringFromInteger(Operator_stVal, 1); /* set Operator Break to OFF */

    //start server
    IedServer_start(iedServer, port);
}

void
update_server(uint64_t timestamp, SimLinkModel* model) {

    //printf("Updating server. Timestamp %llu\n", timestamp);
    Timestamp iecTimestamp;

    Timestamp_clearFlags(&iecTimestamp);
    Timestamp_setTimeInMilliseconds(&iecTimestamp, timestamp);
    Timestamp_setLeapSecondKnown(&iecTimestamp, true);

    /* toggle clock-not-synchronized flag in timestamp */
    // if (((int) t % 2) == 0)
        // Timestamp_setClockNotSynchronized(&iecTimestamp, true);

    IedServer_lockDataModel(iedServer);
    pthread_mutex_lock(&model->lock);

    IedServer_updateTimestampAttributeValue(iedServer, IEDMODEL_LogicalDevice_GGIO1_CircuitBreaker_t, &iecTimestamp);
    IedServer_updateBooleanAttributeValue(iedServer, IEDMODEL_LogicalDevice_GGIO1_CircuitBreaker_stVal, *breakerSt);

    pthread_mutex_unlock(&model->lock);
    IedServer_unlockDataModel(iedServer);
}

void
*exchangeMMSData(void *args)
{
    SimLinkModel* model = (SimLinkModel*)args;
    struct timespec timer_start;
    clock_gettime(CLOCK_MONOTONIC, &timer_start);

    while(true) {
        uint64_t timestamp = Hal_getTimeInMs();

        update_server(timestamp, model);

        updateSecurityStVal(securitySt, timestamp);
        Thread_sleep(2000);
    }
}

void
closeMMSServer()
{
    IedServer_stop(iedServer);
    IedServer_destroy(iedServer);
}

static void
gooseListener(GooseSubscriber subscriber, void* parameter)
{
    uint64_t timestamp = GooseSubscriber_getTimestamp(subscriber);

    if (GooseSubscriber_isValid(subscriber))
    {
        const MmsValue* dataset = GooseSubscriber_getDataSetValues(subscriber);
        const MmsValue* value = MmsValue_getElement(dataset, 0);
        bool newState = MmsValue_getBoolean(value);
        updateSecurityStVal(newState, timestamp);
    }
}

void
setUpGooseReceiver(int cbId)
{
    gooseReceiver = GooseReceiver_create();

    GooseReceiver_setInterfaceId(gooseReceiver, GOOSE_INTERFACE);

    char path[100];
    snprintf(path, sizeof(path), "SM%02dLogicalDevice/LLN0$GO$securityEvent", cbId);

    GooseSubscriber subscriber = GooseSubscriber_create(path, NULL);

    uint8_t dstMac[6] = {0x01,0x0c,0xcd,0x01,0x00,cbId};
    GooseSubscriber_setDstMac(subscriber, dstMac);
    GooseSubscriber_setAppId(subscriber, cbId);

    GooseSubscriber_setListener(subscriber, gooseListener, NULL);

    GooseReceiver_addSubscriber(gooseReceiver, subscriber);

    GooseReceiver_start(gooseReceiver);
}

void
closeGooseReceiver()
{
    GooseReceiver_stop(gooseReceiver);
    GooseReceiver_destroy(gooseReceiver);
}

int main(int argc, char* argv[]) {

    /* Parse parameters */
    if (argc != 4) {
        std::cerr << "Usage: " << argv[0] << " <CB_Id> <Simulink_IP> <Simulink_Port>" << std::endl;
        return 1;
    }
    int cbId = atoi(argv[1]);
    const char* simulinkIp = argv[2];
    const int simulinkPort = atoi(argv[3]);

    /* Setup iec61850 MMS server */
    int port = 102;
    setUpMMSServer(port);

    /* Setup iec61850 Goose receiver */
    setUpGooseReceiver(cbId);

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
    model.stationsInfo[0].digitalOutPorts[0] = simulinkPort;
    displayInfo(&model);

    /* Start data exchange */

    running = 1;

    breakerSt = &model.stationsData[0].digitalOut[0];

    updateBreaker(0);

    exchangeDataWithSimulink(&model);

    pthread_t receivingThread;
    pthread_create(&receivingThread, NULL, exchangeMMSData, &model);

    while(running)
    {
        pthread_mutex_lock(&model.lock);
        printf("CB %d\tSecurity: %d\tOperator: %d\t\tBreaker: %d\n", cbId, securitySt, operatorSt, *breakerSt);
        pthread_mutex_unlock(&model.lock);

        sleep_mss(3000);
    }

    closeMMSServer();
    closeGooseReceiver();
}
