#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <pthread.h>
#include <signal.h>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>

#include <simlink.h>

#include "hal_thread.h"
#include "static_model.h"
#include "iec61850_server.h"
#include "iec61850_client.h"
#include "ladder.h"

using namespace std;

#define NUM_SM 16
#define NUM_CB 16

#define IP_SIZE 16

#define CURRENT_IDX 3
#define CB_ST_IDX 0

#define IEDS_FILE "ieds.txt"

typedef struct iedArgs
{
    int iedId;
    char* iedIp;
    char* hostname;
} tIedArgs;

typedef struct dataset
{
    int16_t* currents;
    int16_t* breakers;
} tDataset;

// IEDs IPs
char smIps[NUM_SM][IP_SIZE];
char cbIps[NUM_CB][IP_SIZE];

// IEC61850 server
IedServer iedServer = NULL;


SimLinkModel model;

// Pointers to the data
int16_t* currents = NULL;
int16_t* breakers = NULL;
int16_t* lineTargted = NULL;
int16_t* lineST = NULL;

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

    //IedServer_setWriteAccessPolicy(iedServer, IEC61850_FC_ALL, ACCESS_POLICY_ALLOW);

    //start server
    IedServer_start(iedServer, port);
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

    IedServer_updateTimestampAttributeValue(iedServer, IEDMODEL_LogicalDevice_GGIO1_Line_t, &iecTimestamp);
    IedServer_updateInt16AttributeValue(iedServer, IEDMODEL_LogicalDevice_GGIO1_Line_mag_f, *lineTargted);

    IedServer_updateTimestampAttributeValue(iedServer, IEDMODEL_LogicalDevice_GGIO1_LineST_t, &iecTimestamp);
    IedServer_updateInt16AttributeValue(iedServer, IEDMODEL_LogicalDevice_GGIO1_LineST_mag_f, *lineST);

    pthread_mutex_unlock(&model->lock);
    IedServer_unlockDataModel(iedServer);
}

void
*sendMMSData(void *args)
{
    SimLinkModel* model = (SimLinkModel*)args;
    struct timespec timer_start;
    clock_gettime(CLOCK_MONOTONIC, &timer_start);

    int running = 1;
    while(running) {
        uint64_t timestamp = Hal_getTimeInMs();

        update_server(timestamp, model);

        //sleep_until(&timer_start, OPLC_CYCLE);
        Thread_sleep(2000);
    }
}

void
reportCallbackFunctionSM(void* parameter, ClientReport report)
{
    LinkedList dataSetDirectory = (LinkedList) parameter;

    MmsValue* dataSetValues = ClientReport_getDataSetValues(report);

    char* reference = ClientReport_getRcbReference(report);
    char id_str[3] = "";
    strncpy(id_str, reference + 2, 2);
    int smId = atoi(id_str);

    // printf("Received report for %s with rptId %s\n", ClientReport_getRcbReference(report), ClientReport_getRptId(report));

    if (ClientReport_hasTimestamp(report)) {
        time_t unixTime = ClientReport_getTimestamp(report) / 1000;
        char timeBuf[30];
        ctime_r(&unixTime, timeBuf);
        //printf("Timestamp (%u): %s", (unsigned int) unixTime, timeBuf);
    }

    if (dataSetDirectory) {
        int n = LinkedList_size(dataSetDirectory);

        ReasonForInclusion reason = ClientReport_getReasonForInclusion(report, CURRENT_IDX);

        if (reason != IEC61850_REASON_NOT_INCLUDED) {

            char valBuffer[500];
            sprintf(valBuffer, "no value");

            if (dataSetValues) {
                MmsValue* value = MmsValue_getElement(dataSetValues, CURRENT_IDX);

                if (value) {
                    if (MmsValue_getType(value) == MMS_INTEGER)
                        currents[smId] = MmsValue_toInt16(value);
                }
            }
        }
    }
}

void
reportCallbackFunctionCB(void* parameter, ClientReport report)
{
    LinkedList dataSetDirectory = (LinkedList) parameter;

    MmsValue* dataSetValues = ClientReport_getDataSetValues(report);

    char* reference = ClientReport_getRcbReference(report);
    char id_str[3] = "";
    strncpy(id_str, reference + 2, 2);
    int cbId = atoi(id_str);

    // printf("Received report for %s with rptId %s\n", ClientReport_getRcbReference(report), ClientReport_getRptId(report));

    if (ClientReport_hasTimestamp(report)) {
        time_t unixTime = ClientReport_getTimestamp(report) / 1000;
        char timeBuf[30];
        ctime_r(&unixTime, timeBuf);
       //printf("Timestamp (%u): %s", (unsigned int) unixTime, timeBuf);
    }

    if (dataSetDirectory) {
        int n = LinkedList_size(dataSetDirectory);
        bool values[n];

        ReasonForInclusion reason = ClientReport_getReasonForInclusion(report, CB_ST_IDX);

            if (reason != IEC61850_REASON_NOT_INCLUDED) {

                char valBuffer[500];
                sprintf(valBuffer, "no value");

                if (dataSetValues) {
                    MmsValue* value = MmsValue_getElement(dataSetValues, CB_ST_IDX);
    
                    if (value) {
                        if (MmsValue_getType(value) == MMS_BOOLEAN)
                            breakers[cbId] = (int16_t)MmsValue_getBoolean(value);
                    }
                }
            }
    }
}

void*
receiveMMSDataFromSM(void* parameters)
{

    tIedArgs *args = (tIedArgs*)parameters;
    int smId = args->iedId;

    IedClientError error;
    IedConnection con = IedConnection_create();

    IedConnection_connect(con, &error, args->hostname, 102);

    if (error != IED_ERROR_OK)
    {
        printf("Connection with %s failed!\n", args->hostname);
        IedConnection_close(con);
        return NULL;
    }

    ClientReportControlBlock rcb = NULL;
    ClientDataSet clientDataSet = NULL;
    LinkedList dataSetDirectory = NULL;

    /* read data set directory */
    char dataset[100];
    snprintf(dataset, sizeof(dataset), "SM%02dLogicalDevice/LLN0.AMI", smId);
    dataSetDirectory = IedConnection_getDataSetDirectory(con, &error, dataset, NULL);

    if (error != IED_ERROR_OK) {
        printf("Reading data set directory failed!\n");
        IedConnection_close(con);
        return NULL;
    }

    /* read data set */
    clientDataSet = IedConnection_readDataSetValues(con, &error, dataset, NULL);

    if (clientDataSet == NULL) {
        printf("failed to read dataset\n");
        IedConnection_close(con);
        return NULL;
    }

    /* Read RCB values */
    char values[100];
    snprintf(values, sizeof(values), "SM%02dLogicalDevice/LLN0.RP.AMI01", smId);
    rcb = IedConnection_getRCBValues(con, &error, values, NULL);

    if (error != IED_ERROR_OK) {
        printf("getRCBValues service error!\n");
        IedConnection_close(con);
        return NULL;
    }

    /* prepare the parameters of the RCP */
    char dataSetReference[100];
    snprintf(dataSetReference, sizeof(dataSetReference), "SM%02dLogicalDevice/LLN0$AMI", smId);

    ClientReportControlBlock_setResv(rcb, true);
    ClientReportControlBlock_setTrgOps(rcb, TRG_OPT_DATA_CHANGED | TRG_OPT_QUALITY_CHANGED | TRG_OPT_GI);
    ClientReportControlBlock_setDataSetReference(rcb, dataSetReference); 
    ClientReportControlBlock_setRptEna(rcb, true);
    ClientReportControlBlock_setGI(rcb, true);

    /* Configure the report receiver */
    char rcbReference[100];
    snprintf(rcbReference, sizeof(rcbReference), "SM%02dLogicalDevice/LLN0.RP.AMI", smId);

    IedConnection_installReportHandler(con, rcbReference, ClientReportControlBlock_getRptId(rcb), reportCallbackFunctionSM,
            (void*) dataSetDirectory);

    /* Write RCB parameters and enable report */
    IedConnection_setRCBValues(con, &error, rcb, RCB_ELEMENT_RESV | RCB_ELEMENT_DATSET | RCB_ELEMENT_TRG_OPS | RCB_ELEMENT_RPT_ENA | RCB_ELEMENT_GI, true);

    if (error != IED_ERROR_OK) {
        printf("setRCBValues service error!\n");
        IedConnection_close(con);
        return NULL;
    }

    Thread_sleep(1000);

    /* Trigger GI Report */
    ClientReportControlBlock_setGI(rcb, true);
    IedConnection_setRCBValues(con, &error, rcb, RCB_ELEMENT_GI, true);

    if (error != IED_ERROR_OK) {
        printf("Error triggering a GI report (code: %i)\n", error);
    }

    int running = 1;
    while (running) {
        Thread_sleep(10);

        IedConnectionState conState = IedConnection_getState(con);

        if (conState != IED_STATE_CONNECTED) {
            printf("Connection closed by server!\n");
            running = 0;
        }
    }

    /* disable reporting */
    ClientReportControlBlock_setRptEna(rcb, false);
    IedConnection_setRCBValues(con, &error, rcb, RCB_ELEMENT_RPT_ENA, true);

    IedConnection_close(con);
}

void*
receiveMMSDataFromCB(void* parameters)
{
    tIedArgs *args = (tIedArgs*)parameters;
    int cbId = args->iedId;

    IedClientError error;
    IedConnection con = IedConnection_create();

    IedConnection_connect(con, &error, args->hostname, 102);

    if (error != IED_ERROR_OK)
    {
        printf("Connection with %s failed!\n", args->hostname);
        IedConnection_close(con);
        return NULL;
    }

    ClientReportControlBlock rcb = NULL;
    ClientDataSet clientDataSet = NULL;
    LinkedList dataSetDirectory = NULL;

    /* read data set directory */
    char dataset[100];
    snprintf(dataset, sizeof(dataset), "CB%02dLogicalDevice/LLN0.CB", cbId);
    dataSetDirectory = IedConnection_getDataSetDirectory(con, &error, dataset, NULL);

    if (error != IED_ERROR_OK) {
        printf("Reading data set directory failed!\n");
        IedConnection_close(con);
        return NULL;
    }

    /* read data set */
    clientDataSet = IedConnection_readDataSetValues(con, &error, dataset, NULL);

    if (clientDataSet == NULL) {
        printf("failed to read dataset\n");
        IedConnection_close(con);
        return NULL;
    }

    /* Read RCB values */
    char values[100];
    snprintf(values, sizeof(values), "CB%02dLogicalDevice/LLN0.RP.CB01", cbId);
    rcb = IedConnection_getRCBValues(con, &error, values, NULL);

    if (error != IED_ERROR_OK) {
        printf("getRCBValues service error!\n");
        IedConnection_close(con);
        return NULL;
    }

    /* prepare the parameters of the RCP */
    char dataSetReference[100];
    snprintf(dataSetReference, sizeof(dataSetReference), "CB%02dLogicalDevice/LLN0$CB", cbId);

    ClientReportControlBlock_setResv(rcb, true);
    ClientReportControlBlock_setTrgOps(rcb, TRG_OPT_DATA_CHANGED | TRG_OPT_QUALITY_CHANGED | TRG_OPT_GI);
    ClientReportControlBlock_setDataSetReference(rcb, dataSetReference); 
    ClientReportControlBlock_setRptEna(rcb, true);
    ClientReportControlBlock_setGI(rcb, true);

    /* Configure the report receiver */
    char rcbReference[100];
    snprintf(rcbReference, sizeof(rcbReference), "CB%02dLogicalDevice/LLN0.RP.CB", cbId);

    IedConnection_installReportHandler(con, rcbReference, ClientReportControlBlock_getRptId(rcb), reportCallbackFunctionCB,
            (void*) dataSetDirectory);

    /* Write RCB parameters and enable report */
    IedConnection_setRCBValues(con, &error, rcb, RCB_ELEMENT_RESV | RCB_ELEMENT_DATSET | RCB_ELEMENT_TRG_OPS | RCB_ELEMENT_RPT_ENA | RCB_ELEMENT_GI, true);

    if (error != IED_ERROR_OK) {
        printf("setRCBValues service error!\n");
        IedConnection_close(con);
        return NULL;
    }

    Thread_sleep(1000);

    /* Trigger GI Report */
    ClientReportControlBlock_setGI(rcb, true);
    IedConnection_setRCBValues(con, &error, rcb, RCB_ELEMENT_GI, true);

    if (error != IED_ERROR_OK) {
        printf("Error triggering a GI report (code: %i)\n", error);
    }

    int running = 1;
    while (running) {
        Thread_sleep(10);

        IedConnectionState conState = IedConnection_getState(con);

        if (conState != IED_STATE_CONNECTED) {
            printf("Connection closed by server!\n");
            running = 0;
        }
    }

    /* disable reporting */
    ClientReportControlBlock_setRptEna(rcb, false);
    IedConnection_setRCBValues(con, &error, rcb, RCB_ELEMENT_RPT_ENA, true);

    IedConnection_close(con);
}

void startMMSClients()
{
   for (int i = 0; i < NUM_SM; i++) {
        char* smIp = (char*)malloc(IP_SIZE);
        strncpy(smIp, smIps[i], IP_SIZE);

        printf("Starting MMS client for SM %d with ip %s\n", i, smIp);

        tIedArgs *args = (tIedArgs*)malloc(sizeof(tIedArgs));
        args->iedId = i;
        args->iedIp = smIp;
        args->hostname = smIp;

        pthread_t thread;
        pthread_create(&thread, NULL, receiveMMSDataFromSM, args);
    }

    for (int i = 0; i < NUM_CB; i++) {
        char* cbIp = cbIps[i];

        printf("Starting MMS client for CB%d with ip %s\n", i, cbIp);

        tIedArgs *args = (tIedArgs*)malloc(sizeof(tIedArgs));
        args->iedId = i;
        args->iedIp = cbIp;
        args->hostname = cbIp;

        pthread_t thread;
        pthread_create(&thread, NULL, receiveMMSDataFromCB, args);
    }
}

void 
*displayData(void *args)
{
    char* iedName = (char*)args;
    while(true)
    {
        pthread_mutex_lock(&model.lock);
        printf("%s:\n", iedName);
        printf("\tLine: %d\n", *lineTargted);
        printf("\tSate: %d\n", *lineST);

        printf("\tCurrents:\n");
        for (int i = 0; i < NUM_SM; i++)
            printf("\t%d", currents[i]);
        printf("\n");

        printf("\Breakers:\n");
        for (int i = 0; i < NUM_SM; i++)
            printf("\t%d", breakers[i]);
        printf("\n");

        pthread_mutex_unlock(&model.lock);

        sleep_mss(3000);
    }
}

void loadIedsIps(char* filename)
{
    std::ifstream file(filename);
    std::string line;
    bool isSMSection = false;
    bool isCBSection = false;

    if (!file.is_open()) {
        std::cerr << "Error opening file!" << std::endl;
        return;
    }

    while (getline(file, line)) {

        std::stringstream ss(line);
        std::string type;
        getline(ss, type, ':');

        if (type.find("SM") != std::string::npos) {
            isSMSection = true;
            isCBSection = false;
        }
        else if (type.find("CB") != std::string::npos) {
            isSMSection = false;
            isCBSection = true;
        }

        if (isSMSection || isCBSection) {
            std::string id;
            std::string ip;

            getline(ss, id, ';');
            getline(ss, ip);

            if (isSMSection) {
                int smId = atoi(id.c_str());
                if (smId < NUM_SM)
                    strncpy(smIps[smId], ip.c_str(), IP_SIZE);
            } else if (isCBSection) {
                int cbId = atoi(id.c_str());
                if (cbId < NUM_CB)
                    strncpy(cbIps[cbId], ip.c_str(), IP_SIZE);
            }
        }
    }

    file.close();
}

int main(int argc, char* argv[]) {

    /* Parse parameters */
    if (argc != 7) {
        std::cerr << "Usage: " << argv[0] << " <DA_ID> <Simulink_IP> <Port_Line> <Port_LineST> <Port_Currents> <Port_Breakers>" << std::endl;
        return 1;
    }

    int smId = atoi(argv[1]);
    const char* simulinkIp = argv[2];
    int simulinkPortLine= atoi(argv[3]);
    int simulinkPortLineST= atoi(argv[4]);
    int simulinkPortCurrents= atoi(argv[5]);
    int simulinkPortBreakers= atoi(argv[6]);

    loadIedsIps(IEDS_FILE);

    /************************/
    /* Setup Simulink model */
    /************************/

    strncpy(model.simulinkIp, simulinkIp, 100);
    model.numStations = 1;
    model.commDelay = 250;

    model.stationsData = (StationData *)calloc(model.numStations, sizeof(StationData));
    model.stationsInfo = (StationInfo *)calloc(model.numStations, sizeof(StationInfo));

    // Simulink Algorithm input
    strncpy(model.stationsInfo[0].ip, simulinkIp, 100);

    model.stationsInfo[0].genericOutPorts[0] = simulinkPortCurrents;
    DataPacket* algCurrents = &model.stationsData[0].genericOut[0];
    algCurrents->count = NUM_SM + NUM_CB;
    algCurrents->itemSize = sizeof(int16_t);
    algCurrents->data = calloc(algCurrents->count, algCurrents->itemSize);

    currents = (int16_t*)algCurrents->data;

    model.stationsInfo[0].genericOutPorts[1] = simulinkPortBreakers;
    DataPacket* algBreakers = &model.stationsData[0].genericOut[1];
    algBreakers->count = NUM_SM + NUM_CB;
    algBreakers->itemSize = sizeof(int16_t);
    algBreakers->data = calloc(algBreakers->count, algBreakers->itemSize);

    breakers = (int16_t*)algBreakers->data;

    // Simulink Algorithm output
    model.stationsInfo[0].analogInPorts[0] = simulinkPortLine;
    lineTargted = &model.stationsData[0].analogIn[0];
    *lineTargted = -1;

    model.stationsInfo[0].analogInPorts[1] = simulinkPortLineST;
    lineST = &model.stationsData[0].analogIn[1];
    *lineST = -1;

    /****************/
    /* Display info */
    /****************/

    displayInfo(&model);

    /********************/
    /* Start MMS server */
    /********************/

    int port = 102;
    setUpMMSServer(port);

    if (!IedServer_isRunning(iedServer)) {
        printf("Error: Could not setup MMS server\n");
        IedServer_destroy(iedServer);
        exit(-1);
    }

    /*********************/
    /* Start MMS clients */
    /*********************/

    startMMSClients();

    /***********************/
    /* Start data exchange */
    /***********************/

    exchangeDataWithSimulink(&model);

    //pthread_t sendingThread;
    //pthread_create(&sendingThread, NULL, sendMMSData, &model);

    /* Display data */
    char smName[5];
    snprintf(smName, 5, "DA%02d", smId);

    pthread_t displayThread;
    pthread_create(&displayThread, NULL, displayData, (void*)smName);

    int running = 1;
    while(running){}


    /****************/
    /* Close data */
    /****************/

    //closeMMSServer();
}
