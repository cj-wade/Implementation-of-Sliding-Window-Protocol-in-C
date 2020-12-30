#ifndef __COMMUNICATE_H__
#define __COMMUNICATE_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <unistd.h>
#include <netdb.h>
#include <math.h>
#include <sys/time.h>
#include "common.h"
#include "util.h"
// 模拟物理层函数，在发送与接收者间通信，采用内存复制方式，考虑丢包率及误码率。
void send_msg_to_receivers(char * );
void send_msg_to_senders(char * );
void send_frame(char * ,
                enum SendFrame_DstType);

#endif
