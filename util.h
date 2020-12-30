#ifndef __UTIL_H__
#define __UTIL_H__

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

//Linked list functions 双向链表 函数
int ll_get_length(LLnode *);
void ll_append_node(LLnode **, void *);
LLnode * ll_pop_node(LLnode **);
void ll_destroy_node(LLnode *);

//Print functions 显示命令
void print_cmd(Cmd *);

//Time functions 计算时间差  usec
long timeval_usecdiff(struct timeval *, 
                      struct timeval *);

//TODO: Implement these functions 待实现功能 
char * convert_frame_to_char(Frame *);
Frame * convert_char_to_frame(char *);

// 内存循环左移
void shift_memory_left(void * p, void * tmp, int window_size, int n, int memoery_size);
#endif
