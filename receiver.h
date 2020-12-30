#ifndef __RECEIVER_H__
#define __RECEIVER_H__

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

void init_receiver(Receiver *, int num_of_senders, int id); // 初始化接收方
void * run_receiver(void *); // 运行接收方线程
// 给消息的发送方返回确认帧
void send_ack_to_sender(Frame * inframe, LLnode ** outgoing_frames_head_ptr);
// 判断帧是否落在滑动窗口内(是返回1(true))
int is_in_the_swp(Receiver * receiver, Frame * inframe);
// 执行接收方滑动窗口协议(每收到一个帧执行一次)
void move_receiver_window(Receiver * receiver, Frame * inframe);
// 处理输入的消息队列
void handle_incoming_msgs(Receiver * receiver,
                          LLnode ** outgoing_frames_head_ptr);
#endif
