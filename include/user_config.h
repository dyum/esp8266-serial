#ifndef __WIFI_SERIAL_H
#define __WIFI_SERIAL_H

#include "c_types.h"

/* Per Project Configuration */
#define SERIAL_BAUD_RATE BIT_RATE_2400
#define AP_SSID "APName"
#define AP_PASSWORD "password"
#define SERVER_IP "192.168.1.101"
#define SERVER_PORT 5000

/* String Buffers */
#define BUF_SIZE 32
#define BUF_COUNT 2

/* 0 is lowest priority */
#define TASK_PRIO_0 0
#define TASK_PRIO_1 1
#define TASK_PRIO_2 2
#define SENDER_QUEUE_LEN 1

#endif
