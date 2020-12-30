#ifndef __COMMON_H__
#define __COMMON_H__

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
// 存放各种源文件中常用的数据结构
// 在此文件定义帧格式

#define MAX_COMMAND_LENGTH 16
#define AUTOMATED_FILENAME 512
// 定义各种需要用到的标志位的大小
#define MAX_FRAME_SIZE 48 // 帧的长度最大48个字节
#define SRC_SIZE 2 // 源地址长度(2字节，最长16位)
#define DST_SIZE 2 // 目标地址长度(2字节，最长16位)
#define SEQ_SIZE 1 // 帧序号长度(1字节，最长8位)
#define ACK_SIZE 1 // 确认号长度(1字节，最长8位)
#define SEG_SIZE 1 // 分组号长度(00代表消息无需分组，01代表消息分组但不是最后一个分组)
#define CRC_SIZE 2 // CRC长度，这里采用16位(2个字节)的CRC检测

#define WINDOW_SIZE 8 // 滑动窗口的最大数量，8个
#define MAX_SEQ 256 //帧的最大序号为256，超过的从头开始
//TODO: You should change this!
//Remember, your frame can be AT MOST 48 bytes!

// 帧内的data最大长度，等于帧的长度减去帧头和帧尾的长度
#define FRAME_PAYLOAD_SIZE 48 - SRC_SIZE - DST_SIZE - SEQ_SIZE - ACK_SIZE - SEG_SIZE - CRC_SIZE
typedef unsigned char uchar_t;

//System configuration information - 系统的一些配置信息
struct SysConfig_t
{
    float drop_prob; // 丢包率
    float corrupt_prob;  // 误码率
    unsigned char automated; // 自动化的？-脚本中需要用到-don't care
    char automated_file[AUTOMATED_FILENAME]; // 512个字符的空间？-脚本中需要用到-don't care
};
typedef struct SysConfig_t  SysConfig; // 把SysConfig_t重命名为SysConfig

//Command line input information - 控制行的输入信息
struct Cmd_t
{
    uint16_t src_id; // 源地址
    uint16_t dst_id; // 目标地址
    char * message; // 消息
};
typedef struct Cmd_t Cmd;

//Linked list information - 链接列表信息
enum LLtype 
{
    llt_string,
    llt_frame,
    llt_integer,
    llt_head
} LLtype;

struct LLnode_t
{
    struct LLnode_t * prev;
    struct LLnode_t * next;
    enum LLtype type;

    void * value;
};
typedef struct LLnode_t LLnode;

struct Frame_t // 帧
{
    uint16_t crc; //循环冗余余数
    uint16_t src; //源地址
    uint16_t dst; //目标地址
    uint8_t seq; //帧序号
    uint8_t ack; //确认号
    uint8_t seg; //分组标志
    char data[FRAME_PAYLOAD_SIZE];
};
typedef struct Frame_t Frame;

// 没有找到布尔类型，这里自己做一个
typedef enum
{
    true=1, false=0
}bool;

struct SlideWindow{// 滑动窗口
    uchar_t left;
    uchar_t right;
    bool window_flag[MAX_FRAME_SIZE]; // 滑动窗口中每一块的标志(共8块)，True表示有帧存储，False则无
    Frame buffer[WINDOW_SIZE]; // 对应窗口每一块正在缓存的帧
};
typedef struct SlideWindow SWP;

//Receiver and sender data structures - 接收方和发送方的数据结构
struct Receiver_t // 接收方
{
    //DO NOT CHANGE:
    // 1) buffer_mutex - 缓冲互斥体
    // 2) buffer_cv
    // 3) input_framelist_head - 输入框架列表头
    // 4) recv_id 
    pthread_mutex_t buffer_mutex; // pthread_mutex_t是linux线程互斥量，一个线程访问这个变量时，其他线程不能访问。
    pthread_cond_t buffer_cv; // pthreaad_cond_t是条件变量，与pthread_mutex_t相互配合
    LLnode * input_framelist_head; // 从发送方发过来的消息队列
    int recv_id; // 接收方id
    int number_of_senders; // 一个接收者对应的发送者数量
    SWP *swp; //接收发的滑动窗口
};

struct Sender_t // 发送方
{
    //DO NOT CHANGE:
    // 1) buffer_mutex
    // 2) buffer_cv
    // 3) input_cmdlist_head
    // 4) input_framelist_head
    // 5) send_id
    pthread_mutex_t buffer_mutex;
    pthread_cond_t buffer_cv;    
    LLnode * input_cmdlist_head; // 用户的输入队列
    LLnode * input_framelist_head; // 收到的消息队列
    int send_id; // 发送方id
    int num_of_receviers; // 一个发送者对应的接受者数量
    SWP * swp; // 发送方的滑动窗口
    struct timeval (*exiring_timeval)[WINDOW_SIZE]; //发送窗口中帧超时时间，接收方无需此标志
    bool (*is_ack)[WINDOW_SIZE]; // 发送窗口中的帧是否被确认的标志, 接收方无需此标志
};

enum SendFrame_DstType 
{
    ReceiverDst, // 代表接收方
    SenderDst // 代表发送方
} SendFrame_DstType ;

typedef struct Sender_t Sender;
typedef struct Receiver_t Receiver;


//Declare global variables here
//DO NOT CHANGE: 
//   1) glb_senders_array
//   2) glb_receivers_array
//   3) glb_senders_array_length
//   4) glb_receivers_array_length
//   5) glb_sysconfig
//   6) CORRUPTION_BITS
Sender * glb_senders_array;
Receiver * glb_receivers_array;
int glb_senders_array_length; // 发送方最大线程数
int glb_receivers_array_length; // 接收方最大线程数
SysConfig glb_sysconfig;
int CORRUPTION_BITS; // 损坏位
#endif 
