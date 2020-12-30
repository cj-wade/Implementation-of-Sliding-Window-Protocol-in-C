#include "communicate.h"
// 负责在发送方线程和接收方线程之间传输消息
//*********************************************************************
//NOTE: We will overwrite this file, so whatever changes you put here
//      WILL NOT persist
//*********************************************************************
void send_frame(char * char_buffer,
                enum SendFrame_DstType dst_type) // 发送帧
{
    int i = 0, j;
    char * per_recv_char_buffer;

    //Multiply out the probabilities to some degree of precision - 在一定程度上将概率相乘
    int prob_prec = 1000;
    int drop_prob = (int) prob_prec * glb_sysconfig.drop_prob;
    int corrupt_prob = (int) prob_prec * glb_sysconfig.corrupt_prob;
    int num_corrupt_bits = CORRUPTION_BITS;
    int corrupt_indices[num_corrupt_bits];

    //Pick a random number
    int random_num = rand() % prob_prec;
    int random_index;

    //Drop the packet on the floor - 丢包
    if (random_num < drop_prob)
    {
        free(char_buffer);
        return;
    }
    
    //Determine whether to corrupt bits - 决定是否误码
    random_num = rand() % prob_prec;
    if (random_num < corrupt_prob)
    {
        //Pick random indices to corrupt - 随机挑选一些位用于误码
        for (i=0;
             i < num_corrupt_bits;
             i++)
        {
            random_index = rand() % MAX_FRAME_SIZE;
            corrupt_indices[i] = random_index;
        }
    }

    //Determine the array size of the destination objects
    int array_length = 0;
    if (dst_type == ReceiverDst)
    {
        array_length = glb_receivers_array_length;
    }
    else if (dst_type == SenderDst)
    {
        array_length = glb_senders_array_length;
    }

    //Go through the dst array and add the packet to their receive queues
    //遍历dst数组并将数据包添加到其接收队列中
    for (i=0;
         i < array_length;
         i++)
    {
        //Allocate a per receiver char buffer for the message
        //为消息分配每个接收者char缓冲区
        per_recv_char_buffer = (char *) malloc(sizeof(char) * MAX_FRAME_SIZE);
        memcpy(per_recv_char_buffer,
               char_buffer,
               MAX_FRAME_SIZE);
        
        //Corrupt the bits (inefficient, should just corrupt one copy and memcpy it
        //损坏位（效率低下，应该只破坏一个副本并对其进行拷贝
        if (random_num < corrupt_prob)
        {
            //Corrupt bits at the chosen random indices-所选随机索引处的损坏位
            for (j=0;
                 j < num_corrupt_bits;
                 j++)
            {
                random_index = corrupt_indices[j];
                per_recv_char_buffer[random_index] = ~per_recv_char_buffer[random_index];
            }            
        }

        if (dst_type == ReceiverDst)
        {
            Receiver * dst = &glb_receivers_array[i]; // 接收方列表
            pthread_mutex_lock(&dst->buffer_mutex); // 锁住内存不让其他访问
            ll_append_node(&dst->input_framelist_head, // 添加从发送方发来的消息队列
                           (void *) per_recv_char_buffer);
            pthread_cond_signal(&dst->buffer_cv); // 释放信号
            pthread_mutex_unlock(&dst->buffer_mutex); // 解锁内存，其他线程可访问
        }
        else if (dst_type == SenderDst)
        {
            Sender * dst = &glb_senders_array[i];
            pthread_mutex_lock(&dst->buffer_mutex);
            ll_append_node(&dst->input_framelist_head,
                           (void *) per_recv_char_buffer);
            pthread_cond_signal(&dst->buffer_cv);
            pthread_mutex_unlock(&dst->buffer_mutex);
        }
    }
    
    free(char_buffer);
    return;
}

//NOTE: You should use the following method to transmit messages from senders to receivers
void send_msg_to_receivers(char * char_buffer)
{
    send_frame(char_buffer,
               ReceiverDst);
    return;
}

//NOTE: You should use the following method to transmit messages from receivers to senders
void send_msg_to_senders(char * char_buffer)
{
    send_frame(char_buffer,
               SenderDst);
    return;
}
