#include "smartMeter.h"

#include "simlink.h"
#include "hal_thread.h"
#include "static_model.h"
#include "iec61850_server.h"
#include "ladder.h"

SmartMeterModel* model;

IedServer iedServer = NULL;

static int running = 0;

char config_file[128];

void sigint_handler(int signalId)
{
    running = 0;
}

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
update_server(uint64_t timestamp, SmartMeterModel* model) {

    pthread_mutex_lock(model->data.dataLock);

    double vA = model->data.voltages[0];
    double vB = model->data.voltages[1];
    double vC = model->data.voltages[2];

    double iA = model->data.currents[0];
    double iB = model->data.currents[1];
    double iC = model->data.currents[2];
    pthread_mutex_unlock(model->data.dataLock);

    Timestamp iecTimestamp;
    Timestamp_clearFlags(&iecTimestamp);
    Timestamp_setTimeInMilliseconds(&iecTimestamp, timestamp);
    Timestamp_setLeapSecondKnown(&iecTimestamp, true);

    IedServer_lockDataModel(iedServer);
    IedServer_updateTimestampAttributeValue(iedServer, IEDMODEL_LogicalDevice_GGIO1_AnVA_t, &iecTimestamp);
    IedServer_updateInt16AttributeValue(iedServer, IEDMODEL_LogicalDevice_GGIO1_AnVA_mag_f, (int16_t)vA);

    IedServer_updateTimestampAttributeValue(iedServer, IEDMODEL_LogicalDevice_GGIO1_AnVB_t, &iecTimestamp);
    IedServer_updateInt16AttributeValue(iedServer, IEDMODEL_LogicalDevice_GGIO1_AnVB_mag_f, (int16_t)vB);

    IedServer_updateTimestampAttributeValue(iedServer, IEDMODEL_LogicalDevice_GGIO1_AnVC_t, &iecTimestamp);
    IedServer_updateInt16AttributeValue(iedServer, IEDMODEL_LogicalDevice_GGIO1_AnVC_mag_f, (int16_t)vC);

    IedServer_updateTimestampAttributeValue(iedServer, IEDMODEL_LogicalDevice_GGIO1_AnIA_t, &iecTimestamp);
    IedServer_updateInt16AttributeValue(iedServer, IEDMODEL_LogicalDevice_GGIO1_AnIA_mag_f, (int16_t)iA);

    IedServer_updateTimestampAttributeValue(iedServer, IEDMODEL_LogicalDevice_GGIO1_AnIB_t, &iecTimestamp);
    IedServer_updateInt16AttributeValue(iedServer, IEDMODEL_LogicalDevice_GGIO1_AnIB_mag_f, (int16_t)iB);

    IedServer_updateTimestampAttributeValue(iedServer, IEDMODEL_LogicalDevice_GGIO1_AnIC_t, &iecTimestamp);
    IedServer_updateInt16AttributeValue(iedServer, IEDMODEL_LogicalDevice_GGIO1_AnIC_mag_f, (int16_t)iC);

    IedServer_updateTimestampAttributeValue(iedServer, IEDMODEL_LogicalDevice_GGIO1_SecurityST_t, &iecTimestamp);
    IedServer_updateBooleanAttributeValue(iedServer, IEDMODEL_LogicalDevice_GGIO1_SecurityST_stVal, model->data.safeState);
    IedServer_unlockDataModel(iedServer);
}

void
*exchangeMMSData(void *args)
{
    SmartMeterModel* model = (SmartMeterModel*)args;
    struct timespec timer_start;
    clock_gettime(CLOCK_MONOTONIC, &timer_start);

    while(running) {
        uint64_t timestamp = Hal_getTimeInMs();

        update_server(timestamp, model);

        //sleep_until(&timer_start, OPLC_CYCLE);
        Thread_sleep(2000);
    }
}

/*
* MATLAB/Simulink interface methods
*/

SimLinkModel*
createSimlinkModel(SmartMeterModel* model)
{
    SimLinkModel* simModel = (SimLinkModel*)malloc(sizeof(SimLinkModel));

    strncpy(simModel->simulinkIp, model->ip, IP_SIZE);
    simModel->numStations = 1;
    simModel->commDelay = 250;

    // Create station
    simModel->stationsInfo = (StationInfo *)malloc(simModel->numStations * sizeof(StationInfo));
    simModel->stationsInfo->genericInPorts[0] = model->dataPort;

    // Create data buffers
    simModel->stationsData = (StationData *)malloc(simModel->numStations * sizeof(StationData));
    simModel->stationsData->genericIn[0].count = 2 * THREE_PHASES;
    simModel->stationsData->genericIn[0].itemSize = sizeof(double);
    simModel->stationsData->genericIn[0].maxSize = 2 * THREE_PHASES * sizeof(double);
    simModel->stationsData->genericIn[0].data = (double *)malloc(2 * THREE_PHASES * sizeof(double));

    // Link data buffers
    model->data.voltages = (double*)simModel->stationsData->genericIn[0].data;
    model->data.currents = (double*)(simModel->stationsData->genericIn[0].data + THREE_PHASES * sizeof(double));

    // Create and link mutex
    pthread_mutex_init(&simModel->lock, NULL);
    model->data.dataLock = &simModel->lock;

    return simModel;
}

void 
*displayData(void *args)
{
    char* iedName = (char*)args;
    while(running)
    {
        pthread_mutex_lock(model->data.dataLock);
        printf("%s:\n", iedName);
        printf("\tVoltage: %f\t\t%f\t\t%f\n", model->data.voltages[0], model->data.voltages[1], model->data.voltages[2]);
        printf("\tCurrent: %f\t\t%f\t\t%f\n", model->data.currents[0], model->data.currents[1], model->data.currents[2]);
        printf("\tSafe: %d\n", model->data.safeState);
        pthread_mutex_unlock(model->data.dataLock);

        sleep_mss(1000);
    }
}

int main(int argc, char* argv[]) {

    /* Create model and parse parameters */

    if (argc < 5 || argc > 6) {
        std::cerr << "Usage: " << argv[0] << " <SM_ID> <Simulink_IP> <DataPort> <safe_threshold> [<GOOSE_interface>]" << std::endl;
        return 1;
    }

    model = (SmartMeterModel*)malloc(sizeof(SmartMeterModel));
    model->id = (int16_t)atoi(argv[1]);
    model->ip = (char*)malloc(IP_SIZE * sizeof(char));
    strncpy(model->ip, argv[2], IP_SIZE);

    model->dataPort = (int16_t)atoi(argv[3]);
    model->data.dataLock = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));

    model->safetyThreshold = (int16_t)atoi(argv[4]);
    model->goose_interface = (char*)malloc(INTERFACE_SIZE * sizeof(char));

    if (argc == 6)
        strncpy(model->goose_interface, argv[5], INTERFACE_SIZE);
    else
        strncpy(model->goose_interface, DEF_GOOSE_INTERFACE, INTERFACE_SIZE);

    printf("Using GOOSE interface: %s\n", model->goose_interface);

    model->data.currents = NULL;
    model->data.voltages = NULL;
    model->data.safeState = true;

    /* Create Simulink model */

    SimLinkModel* simLinkModel = createSimlinkModel(model);

    /* Setup iec61850 MMS server */

    setUpMMSServer(MMS_PORT, model->goose_interface);

    if (!IedServer_isRunning(iedServer)) {
        printf("Error: Could not setup MMS server\n");
        IedServer_destroy(iedServer);
        running = 0;
        return 1;
    }

    /* Start data exchange */

    running = 1;
    signal(SIGINT, sigint_handler);

    exchangeDataWithSimulink(simLinkModel);

    pthread_t receivingThread;
    pthread_create(&receivingThread, NULL, exchangeMMSData, model);

    /* Display data */
    char smName[10];
    snprintf(smName, 10, "SM%02d", model->id);

    pthread_t displayThread;
    pthread_create(&displayThread, NULL, displayData, (void*)smName);


    int safetyCounter = 0;

    /* Update data */
    while(running)
    {
        pthread_mutex_lock(model->data.dataLock);

        double iA = model->data.currents[0];
        double iB = model->data.currents[1];
        double iC = model->data.currents[2];

        bool newState = !(iA > model->safetyThreshold || iB > model->safetyThreshold || iC > model->safetyThreshold);

        if (!model->data.safeState && newState && safetyCounter < 3)
        {
            safetyCounter++;
        }
        else
        {
            model->data.safeState = newState;
            safetyCounter = 0;
        }
        pthread_mutex_unlock(model->data.dataLock);

        // In case of unsafe circuit, wait for 3s to stabilize
        if (!model->data.safeState)
            sleep_mss(3000);

        sleep_mss(1000);
    }

    closeMMSServer();
}