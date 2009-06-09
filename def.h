#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <pvm3.h>
#include <map>
#include <list>
#include <iostream>
#include <fstream>
#include <string>

#include "messages.h"

#define TMP_FILE		"/tmp/note0.txt"
#define NODENAME		"node"
#define NODENUM			10

#define NAMESIZE   		64
#define BUF_SIZE 		32

/* number of resources in the system - at least 1 for each node */
#define RESOURCE_NUM 	30

/* probability of requesting and freeing resources */
#define REQUEST_PROB 	0.75
#define FREE_PROB 		0.5

/* number of requested resources */
#define REQUEST_MIN 	2
#define REQUEST_MAX 	8

#define MSG_MSTR 		1
#define MSG_SLV  		2

/* masks */
#define USER_MSG_MASK	0x10
#define NON_MANAGING	0x30

/* init process */
#define MSG_INIT 		0x00
#define MSG_STOP 		0x01
#define MSG_START_ALG	0x02

/* node requesting or revoking a request */
#define MSG_REQUEST 	0x10
#define MSG_CANCEL 		0x11
#define MSG_FREE 		0x12

/* resource manager response */
#define MSG_GRANT 		0x13

/* deadlock detection messages */

#define MSG_FLOOD		0x20
#define MSG_ECHO		0x21
#define MSG_SHORT		0x22

/* prototypes */

void resourceGranted(int resId);
