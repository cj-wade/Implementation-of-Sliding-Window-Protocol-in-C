#ifndef __SENDER_H__
#define __SENDER_H__

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
#include "communicate.h"
#include "crc_16.h"


void init_sender(Sender *, int num_of_receivers, int id);  // 初始化发送方
void * run_sender(void *); // 运行发送方线程
// 是否是损坏的帧, 如果损坏，返回1(false)；如果完好，返回0(true)
void move_sender_window(Sender * sender, Frame * inframe);

#endif
