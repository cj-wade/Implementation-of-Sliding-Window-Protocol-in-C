#include "receiver.h"
// 包含接收方线程的框架代码

// 給接收方分配id
void init_receiver(Receiver * receiver, int num_of_senders, int id)
{
    receiver->recv_id = id;
    receiver->input_framelist_head = NULL;
    receiver->number_of_senders = num_of_senders; //发送方数量
    receiver->swp = (SWP *)malloc(num_of_senders * sizeof(SWP)); // 有多少个发送方就维护多少个滑动窗口
    for (int i = 0; i < receiver->number_of_senders; i++)
    {
        receiver->swp[i].left = 0;
        receiver->swp[i].right = WINDOW_SIZE - 1; // 即初始化时 left-right ： 0-7
        memset(receiver->swp[i].window_flag, false, WINDOW_SIZE * sizeof(bool)); //初始化滑动窗口标志位，全部设为false(无帧存储)
        memset(receiver->swp[i].buffer, 0, WINDOW_SIZE * sizeof(Frame)); // 初始化滑动窗口帧的缓存区(8个帧)
    }
}

// 是否是损坏的帧, 如果损坏，返回1(false)；如果完好，返回0(true)。
int is_a_broken_frame(Frame * inframe)
{
    uint16_t crc = inframe->crc; //提出crc余数
    inframe->crc = 0; //帧的crc清0

    // 重新对crc部位清0后的帧做一遍crc_16，判断结果是否相等，不相等说明损坏了
    if (crc != crc_16((unsigned char *)inframe, MAX_FRAME_SIZE))
    {
        return 1;
    }

    // 没有损坏
    return 0;
}

// 给消息的发送方返回确认帧
void send_ack_to_sender(Frame * inframe, LLnode ** outgoing_frames_head_ptr)
{
    Frame * ack_frame = (Frame *)malloc(sizeof(Frame)); //新建一个帧
    memset(ack_frame, 0, MAX_FRAME_SIZE); //帧内存初始化为0

    // 接收方发送方地址角色互换
    ack_frame->src = inframe->dst;
    ack_frame->dst = inframe->src;

    ack_frame->ack = inframe->seq; //ack号就是原发送方seq号
    ack_frame->crc = 0; //内存再清零保险一点
    ack_frame->crc = crc_16((unsigned char *)ack_frame, MAX_FRAME_SIZE); //添加crc-16
    // 调试信息s
    fprintf(stderr, "send a ack to sender <RECV_%d>---<SEND_%d>:[seq: %d]\n", ack_frame->src, ack_frame->dst, ack_frame->ack + 1);
    ll_append_node(outgoing_frames_head_ptr, convert_frame_to_char(ack_frame)); //添加该消息进入接收方的待发队列，等待系统调用线程自动发送
}

// 判断帧是否落在滑动窗口内(是返回1(true))
int is_in_the_swp(Receiver * receiver, Frame * inframe)
{ 
    // 综合以下三种情况
    // 1. 两者均没有进入超过255的循环
    //  1.1 帧号大于left, 相减>0
    //  1.2 帧号小于left, 相减<0
    // 2.帧号<=255, 但left>255(left进入下一次0-255循环了)，实际上left已经大于帧号(不在窗口内)，但表面上帧号远大于left，相减>MAX_SEQ-1
    // 3.left<=255, 但帧号>255(帧号进入下一次0-255循环了)，相减远<0

    //综上，给帧号加上256再余(%)256是个不错的选择，保证了bias(结果为正数)
    //如果在窗口内，bias < MAX_SEQ, 如果不在，bias则会非常大(但也小于255)
    int bias = ((inframe->seq + MAX_SEQ) - (receiver->swp[inframe->src].left)) % MAX_SEQ;
    if (bias < WINDOW_SIZE)
    {
        return 1;
    }

    return 0;
}

// 执行接收方滑动窗口协议(每收到一个帧执行一次)
void move_receiver_window(Receiver * receiver, Frame * inframe)
{
    // 该帧在滑动窗口中的偏置
    int bias = ((inframe->seq + MAX_SEQ) - (receiver->swp[inframe->src].left)) % MAX_SEQ;
    // 虽然该帧在窗口内，但已经收到过了。直接返回，啥也不执行
    if (receiver->swp[inframe->src].window_flag[bias])
    {
        fprintf(stderr, "the packet: <SEND_%d>---<RECV_%d>:[%s] had been receipt\n", inframe->src, inframe->dst, inframe->data);
        return;
    }
    
    // 设置标志位为true，先缓存该帧
    receiver->swp[inframe->src].window_flag[bias] = true;
    memcpy(&receiver->swp[inframe->src].buffer[bias], inframe, sizeof(Frame));
    
    // 执行滑动窗口协议
    int i = 0; // 用于统计滑动了几格
    for (i = 0; i < WINDOW_SIZE; i++)
    {
        if(!(receiver->swp[inframe->src].window_flag[i])) // 如果在滑动窗口内遇到空位，则退出循环，停止向网络层交付
        {
            break;
        }
        else
        {
            fprintf(stderr, "send to the network layer: <SEND_%d>---<RECV_%d>:[%s]\n", inframe->src, receiver->recv_id, receiver->swp[inframe->src].buffer[i].data);
            printf("<RECV_%d>:[%s]\n", receiver->recv_id, receiver->swp[inframe->src].buffer[i].data); //交付给网络层-实际上直接输出即可
            // printf("<SEND_%d>---<RECV_%d>:[%s]\n", receiver->swp[inframe->src].buffer[i].src,receiver->recv_id, receiver->swp[inframe->src].buffer[i].data); //交付给网络层-实际上直接输出即可
        }
    }
    
    // 内存和窗口标志位左移动i块
    Frame tmp_frame;
    bool tmp_flag = false;
    shift_memory_left(receiver->swp[inframe->src].buffer, &tmp_frame, WINDOW_SIZE, i, sizeof(Frame));
    shift_memory_left(receiver->swp[inframe->src].window_flag, &tmp_flag, WINDOW_SIZE, i, sizeof(bool));

    // 滑动窗口
    receiver->swp[inframe->src].left = (receiver->swp[inframe->src].left + i) % MAX_SEQ;
    receiver->swp[inframe->src].right = (receiver->swp[inframe->src].right + i) % MAX_SEQ;
}

// 处理输入的消息队列
void handle_incoming_msgs(Receiver * receiver,
                          LLnode ** outgoing_frames_head_ptr)
{
    //TODO: Suggested steps for handling incoming frames
    //    1) Dequeue the Frame from the sender->input_framelist_head
    //    2) Convert the char * buffer to a Frame data type
    //    3) Check whether the frame is corrupted
    //    4) Check whether the frame is for this receiver
    //    5) Do sliding window protocol for sender/receiver pair
    // TODO：处理传入帧的建议步骤
    // 1）从发件人-> input_framelist_head出帧
    // 2）将char *缓冲区转换为Frame数据类型
    // 3）检查帧是否损坏(利用crc校验)
    // 4）检查帧是否适合此接收器
    // 5）对发送方/接收方对执行滑动窗口协议
    int incoming_msgs_length = ll_get_length(receiver->input_framelist_head); //接收到的消息队列的长度
    // fprintf(stderr, "666!");
    while (incoming_msgs_length > 0)
    {

        //Pop a node off the front of the link list and update the count
        LLnode * ll_inmsg_node = ll_pop_node(&receiver->input_framelist_head); //从队列中取出一条消息
        incoming_msgs_length = ll_get_length(receiver->input_framelist_head); //剩余的消息队列长度
        // printf("incoming_msgs_length-%d\n", incoming_msgs_length);
        //DUMMY CODE: Print the raw_char_buf
        //NOTE: You should not blindly print messages!
        //      Ask yourself: Is this message really for me?
        //                    Is this message corrupted?
        //                    Is this an old, retransmitted message?           
        char * raw_char_buf = (char *) ll_inmsg_node->value;
        Frame * inframe = convert_char_to_frame(raw_char_buf); //将char类型转化为帧
        // 调试信息
        fprintf(stderr, "get a input message from sender <SEND_%d>---<RECV_%d>:[%s]\n", inframe->src, inframe->dst, inframe->data);
        // 处理该帧
        // 1.该帧是否损坏
        //      2.如果没损坏，该帧目标是否是此接收方 
        //          3.如果是，返回确认帧ACK
        //              4.判断该帧是否落在滑动窗口内
        //                  5.如果在窗口内，则执行滑动窗口协议，否则结束对该帧的处理
        //                      6.处理下一个输入帧

        if (!is_a_broken_frame(inframe)) //该帧是否损坏
        {
            if(inframe->dst == receiver->recv_id) //属于该接收方
            {
                send_ack_to_sender(inframe, outgoing_frames_head_ptr); //返回确认帧(不管在不在窗口内都返回)
                if(is_in_the_swp(receiver, inframe)) //该帧是否落在滑动窗口内
                {
                    // printf("it fall in window\n");
                    // printf("<RECV_%d>:[%s]\n", receiver->recv_id, inframe->data);
                    fprintf(stderr, "fall in receiver swp: <SEND_%d>---<RECV_%d>:[%s]\n", 
                    inframe->src, inframe->dst, inframe->data);
                    move_receiver_window(receiver, inframe); //执行滑动窗口协议
                }
            }
        }
        
        //Free raw_char_buf
        free(raw_char_buf);

        free(inframe);
        free(ll_inmsg_node);
    }
}

void * run_receiver(void * input_receiver)
{    
    struct timespec   time_spec;
    struct timeval    curr_timeval;
    const int WAIT_SEC_TIME = 0;
    const long WAIT_USEC_TIME = 100000;
    Receiver * receiver = (Receiver *) input_receiver;
    LLnode * outgoing_frames_head;


    //This incomplete receiver thread, at a high level, loops as follows:
    //1. Determine the next time the thread should wake up if there is nothing in the incoming queue(s)
    //2. Grab the mutex protecting the input_msg queue
    //3. Dequeues messages from the input_msg queue and prints them
    //4. Releases the lock
    //5. Sends out any outgoing messages
    //此不完整的接收器线程在较高级别循环如下：
    // 1。 如果传入队列中没有任何内容，请确定线程下次应唤醒的时间
    // 2。 获取互斥锁以保护input_msg队列
    // 3。 使消息从input_msg队列中出队并打印它们
    // 4。 释放锁
    // 5。 发送所有外发消息

    pthread_cond_init(&receiver->buffer_cv, NULL);
    pthread_mutex_init(&receiver->buffer_mutex, NULL);

    while(1)
    {    
        //NOTE: Add outgoing messages to the outgoing_frames_head pointer
        //注意：将传出消息添加到Outing_frames_head指针
        outgoing_frames_head = NULL;
        gettimeofday(&curr_timeval, 
                     NULL);

        //Either timeout or get woken up because you've received a datagram
        //NOTE: You don't really need to do anything here, but it might be useful for debugging purposes to have the receivers periodically wakeup and print info
        //由于收到数据报而超时或被唤醒
        //注意：您实际上不需要在这里做任何事情，但是对于调试目的，让接收器定期唤醒并打印信息可能很有用
        time_spec.tv_sec  = curr_timeval.tv_sec;
        time_spec.tv_nsec = curr_timeval.tv_usec * 1000;
        time_spec.tv_sec += WAIT_SEC_TIME;
        time_spec.tv_nsec += WAIT_USEC_TIME * 1000;
        if (time_spec.tv_nsec >= 1000000000)
        {
            time_spec.tv_sec++;
            time_spec.tv_nsec -= 1000000000;
        }

        //*****************************************************************************************
        //NOTE: Anything that involves dequeing from the input frames should go 
        //      between the mutex lock and unlock, because other threads CAN/WILL access these structures
        //*****************************************************************************************
        pthread_mutex_lock(&receiver->buffer_mutex);

        //Check whether anything arrived
        int incoming_msgs_length = ll_get_length(receiver->input_framelist_head);
        if (incoming_msgs_length == 0)
        {
            //Nothing has arrived, do a timed wait on the condition variable (which releases the mutex). Again, you don't really need to do the timed wait.
            //A signal on the condition variable will wake up the thread and reacquire the lock
            pthread_cond_timedwait(&receiver->buffer_cv, 
                                   &receiver->buffer_mutex,
                                   &time_spec);
        }

        handle_incoming_msgs(receiver,
                             &outgoing_frames_head);

        pthread_mutex_unlock(&receiver->buffer_mutex);
        
        //CHANGE THIS AT YOUR OWN RISK!
        //Send out all the frames user has appended to the outgoing_frames list
        int ll_outgoing_frame_length = ll_get_length(outgoing_frames_head);
        while(ll_outgoing_frame_length > 0)
        {
            LLnode * ll_outframe_node = ll_pop_node(&outgoing_frames_head);
            char * char_buf = (char *) ll_outframe_node->value;
            
            //The following function frees the memory for the char_buf object
            send_msg_to_senders(char_buf);

            //Free up the ll_outframe_node
            free(ll_outframe_node);

            ll_outgoing_frame_length = ll_get_length(outgoing_frames_head);
        }
    }
    pthread_exit(NULL);

}
