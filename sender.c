#include "sender.h"
// 包含发送方线程的框架代码

// 給發送方分配id
void init_sender(Sender * sender, int num_of_receivers, int id)
{
    //TODO: You should fill in this function as necessary
    sender->send_id = id;
    sender->input_cmdlist_head = NULL; // 用户的输入队列
    sender->input_framelist_head = NULL; // 從接收方發送過來的消息隊列
    sender->num_of_receviers = num_of_receivers; //一个发送方对应接收方数量
    sender->swp = (SWP *)malloc(num_of_receivers * sizeof(SWP)); //给发送方队列分配内存
    //给对应的多个接受者的每个滑动窗(一个接收者对应的一个窗口) 的 每个块分配内存和初始标志位并初始化
    sender->exiring_timeval = (struct timeval(*)[WINDOW_SIZE])malloc(sizeof(struct timeval) * num_of_receivers * WINDOW_SIZE); 
    sender->is_ack = (bool(*)[WINDOW_SIZE]) malloc(sizeof(bool) * num_of_receivers * WINDOW_SIZE);
    for (int i = 0; i < num_of_receivers; i++)
    {
        for (int j = 0; j < WINDOW_SIZE; j++)
        {
            sender->is_ack[i][j] = false;
            sender->exiring_timeval[i][j].tv_sec = 0;
            sender->exiring_timeval[i][j].tv_usec = 0;
        }
        
    }
    // 初始化发送方滑动窗口内存
        for (int i = 0; i < sender->num_of_receviers; i++)
    {
        sender->swp[i].left = 0;
        sender->swp[i].right = WINDOW_SIZE - 1; // 即初始化时 left-right ： 0-7
        memset(sender->swp[i].window_flag, false, WINDOW_SIZE * sizeof(bool)); //初始化滑动窗口标志位，全部设为false(无帧存储)
        memset(sender->swp[i].buffer, 0, WINDOW_SIZE * sizeof(Frame)); // 初始化滑动窗口帧的缓存区(8个帧)
    }
}


struct timeval * sender_get_next_expiring_timeval(Sender * sender) // 发送方获取下一个最临近到期时间
{
    //TODO: You should fill in this function so that it returns the next timeout that should occur
    int min_index_1 = 0; //最临近的时间在二维数组中的第1维下标
    int min_index_2 = 0; //最临近的时间在二维数组中的第2维下标

    // 找出最临近(时间最小)的下标
    for (int i = 0; i < sender->num_of_receviers; i++)
    {
        for (int j = 0; i < WINDOW_SIZE; i++)
        {
            if (sender->exiring_timeval[i][j].tv_sec < sender->exiring_timeval[min_index_1][min_index_2].tv_sec 
                && sender->exiring_timeval[i][j].tv_usec < sender->exiring_timeval[min_index_1][min_index_2].tv_usec)
            {
                min_index_1 = i;
                min_index_2 = j;
            }
            
        }
    }

    // 如果还没开始计时，就啥都不返回
    if (sender->exiring_timeval[min_index_1][min_index_2].tv_sec == 0
        && sender->exiring_timeval[min_index_1][min_index_2].tv_usec == 0)
    {
        return NULL;
    }

    return &sender->exiring_timeval[min_index_1][min_index_2];
}

// 是否是损坏的帧, 如果损坏，返回1(false)；如果完好，返回0(true)。
int is_broken_frame(Frame * inframe)
{
    unsigned short int crc = inframe->crc; //提出crc余数
    inframe->crc = 0; //帧的crc清0

    // 重新对crc部位清0后的帧做一遍crc_16，判断结果是否相等，不相等说明损坏了
    if (crc != crc_16((unsigned char *)inframe, MAX_FRAME_SIZE))
    {
        return 1;
    }

    // 没有损坏
    return 0;
}

// 执行发送方滑动窗口协议(每收到一个帧执行一次)
void move_sender_window(Sender * sender, Frame * inframe)
{
    // 设置相应标志位为true 并将时钟置0 不再重发
    int bias = ((inframe->ack + MAX_SEQ) - (sender->swp[inframe->src].left)) % MAX_SEQ;
    sender->is_ack[inframe->src][bias] = true;
    sender->exiring_timeval[inframe->src][bias].tv_sec = 0;
    sender->exiring_timeval[inframe->src][bias].tv_usec = 0;

     // 执行滑动窗口协议
    int i = 0; // 用于统计滑动了几格
    for (i = 0; i < WINDOW_SIZE; i++)
    {
        if(!(sender->is_ack[inframe->src][i])) // 如果在滑动窗口内遇到空位，则退出循环
        {
            break;
        }
    }
    
    // 待发帧、待发帧标志位、窗口ack标志位、各帧对应的计时器 左移动i块
    Frame tmp_frame;
    bool tmp_flag = false;
    struct timeval timp_time;
    timp_time.tv_sec = 0;
    timp_time.tv_usec = 0;
    shift_memory_left(sender->swp[inframe->src].buffer, &tmp_frame, WINDOW_SIZE, i, sizeof(Frame));
    shift_memory_left(sender->swp[inframe->src].window_flag, &tmp_flag, WINDOW_SIZE, i, sizeof(bool));
    shift_memory_left(sender->is_ack[inframe->src], &tmp_flag, WINDOW_SIZE, i, sizeof(bool));
    shift_memory_left(sender->exiring_timeval[inframe->src], &timp_time, WINDOW_SIZE, i, sizeof(struct timeval));

    // 滑动窗口
    sender->swp[inframe->src].left = (sender->swp[inframe->src].left + i) % MAX_SEQ;
    sender->swp[inframe->src].right = (sender->swp[inframe->src].right + i) % MAX_SEQ;
}

// 处理传入的确认号
void handle_incoming_acks(Sender * sender,
                          LLnode ** outgoing_frames_head_ptr)
{
    //TODO: Suggested steps for handling incoming ACKs
    //    1) Dequeue the ACK from the sender->input_framelist_head // 从接收方发来的消息队列取出一个
    //    2) Convert the char * buffer to a Frame data type // 将char转化为帧
    //    3) Check whether the frame is corrupted 检测帧是否损坏
    //    4) Check whether the frame is for this sender // 检查帧是否是发给自己的(也可能是发给别的发送方的)
    //    5) Do sliding window protocol for sender/receiver pair // 对发送方/接收方对执行滑动窗口协议
    int input_acks_length = ll_get_length(sender->input_framelist_head); // 确认号队列的长度
    //重新检查命令队列长度以查看是否有新的帧
    input_acks_length = ll_get_length(sender->input_framelist_head);
    while (input_acks_length > 0)//一个个处理队列中的确认帧
    {
        // 提取一个ack消息并转化为帧
        LLnode * ll_input_acks_node = ll_pop_node(&sender->input_framelist_head);
        char * char_ack = (char *)ll_input_acks_node->value;
        Frame * ack_frame = convert_char_to_frame(char_ack);
        // 对收到的帧处理逻辑如下
        // 1. 判断该帧是否有损坏
        //     2. 如果没有，判断是否是发给自己的
        //         3.1 如果不是，丢弃
        //         3.2 如果是，则收下，并执行滑动窗口协议
        if (!is_broken_frame(ack_frame)) // 是否位损坏帧
        {
            if(ack_frame->dst == sender->send_id)
            {
                // 调试信息
                fprintf(stderr, "get a ack from receiver <RECV_%d>---<SEND_%d>:[seq: %d]\n", ack_frame->src, ack_frame->dst, ack_frame->ack + 1);
                move_sender_window(sender, ack_frame); // 滑动发送窗口
            }
            else // 丢弃，释放空间
            {
                // ll_append_node(&sender->input_framelist_head, (void *) ack_frame);
                free(ack_frame);
                free(char_ack);
                free(ll_input_acks_node);
            }
        }

        input_acks_length = ll_get_length(sender->input_framelist_head); //看看队列中是否还有ack未处理
    }

}


// 处理用户输入队列
void handle_input_cmds(Sender * sender,
                       LLnode ** outgoing_frames_head_ptr) 
{
    //TODO: Suggested steps for handling input cmd
    //    1) Dequeue the Cmd from sender->input_cmdlist_head - 从输入队列中取出一个
    //    2) Convert to Frame - 转化为帧
    //    3) Set up the frame according to the sliding window protocol - 根据滑动窗口协议设置帧
    //    4) Compute CRC and add CRC to Frame - 计算CRC并将余数添加到帧尾

    int input_cmd_length = ll_get_length(sender->input_cmdlist_head);
    
        
    //Recheck the command queue length to see if stdin_thread dumped a command on us
    //重新检查命令队列长度以查看stdin_thread是否向我们转储了命令
    input_cmd_length = ll_get_length(sender->input_cmdlist_head);
    while (input_cmd_length > 0) // 当输入队列中有消息时持续处理
    {
        // 对消息队列的每一个消息处理逻辑如下
        // 1. 从链表中取出，转换为Cmd格式
        // 2. 判断目前该sender的发送窗口是否还有空间
        //     3.1 如果没有，则把消息塞回队头
        //     3.2 如果有，则判断该消息是否特别大需要切片
        //          4.1 如果不需要，直接包装一下(加入crc-16)发出,并设置对应的计时器
        //          4.2 如果需要，则将其切片，并将第一个片包装发出，剩下的塞回队头，下一个再发

        // 1
        //Pop a node off and update the input_cmd_length
        // 从输入队列中取出一个消息
        LLnode * ll_input_cmd_node = ll_pop_node(&sender->input_cmdlist_head); 
        input_cmd_length = ll_get_length(sender->input_cmdlist_head);
        //Cast to Cmd type and free up the memory for the node
        //并转换为Cmd格式
        Cmd * outgoing_cmd = (Cmd *) ll_input_cmd_node->value;
        free(ll_input_cmd_node);
        // 调试信息 - 输出从cmd队列中取出的消息
        fprintf(stderr, "get a message from sendlist:<SEND_%d>---<RECV_%d>:[%s]\n", outgoing_cmd->src_id, outgoing_cmd->dst_id, outgoing_cmd->message);
        // 2
        int bias = 0; //目前滑动窗口的偏置(已经被缓存占用的块数)，先初始化为0
        for (bias = 0; bias < WINDOW_SIZE; bias++)
        {
            if (!(sender->swp[outgoing_cmd->dst_id].window_flag[bias]))
            {
                break;
            }
        }
        int available_size = WINDOW_SIZE - bias; //窗口剩余可用空间

        // 3
        if(available_size > 0) //发送窗口还有空间可用用于发送
        {
            //DUMMY CODE: Add the raw char buf to the outgoing_frames list
            //NOTE: You should not blindly send this message out!
            //      Ask yourself: Is this message actually going to the right receiver (recall that default behavior of send is to broadcast to all receivers)?
            //                    Does the receiver have enough space in in it's input queue to handle this message?
            //                    Were the previous messages sent to this receiver ACTUALLY delivered to the receiver?
            // 调试信息 - 输出可发送窗口大小
            fprintf(stderr, "have window-size:%d to:<SEND_%d>---<RECV_%d>:[%s]\n", available_size, outgoing_cmd->src_id, outgoing_cmd->dst_id, outgoing_cmd->message);
            int msg_length = strlen(outgoing_cmd->message); // 看看这个消息有多长
            uint8_t seg_flag = 0; //是否是分组消息的标志
            // 4
            if (msg_length >= FRAME_PAYLOAD_SIZE) //超过大小了，要进行切片处理
            {
                seg_flag = 1;
                //Do something about messages that exceed the frame size
                //对超出帧大小的消息采取措施
                // 新建一个cmd，用于存储一个帧放不下的剩余内容
                Cmd * surplus_cmd = (Cmd*)malloc(sizeof(Cmd));;

                //将原来的cmd的头直接赋值给这个新的cmd
                surplus_cmd->src_id = outgoing_cmd->src_id;
                surplus_cmd->dst_id = outgoing_cmd->dst_id;

                // 把一个帧传不完的消息放进这个surplus_cmd
                surplus_cmd->message = (char*)malloc((msg_length - (int)FRAME_PAYLOAD_SIZE) + 2); 
                strncpy(surplus_cmd->message, (outgoing_cmd->message + (int)FRAME_PAYLOAD_SIZE - 1), (msg_length - (int)FRAME_PAYLOAD_SIZE) + 2);
                outgoing_cmd->message[FRAME_PAYLOAD_SIZE - 1] = '\0';

                // 将多余的部分入队
                ll_append_node(&sender->input_cmdlist_head, (void *) surplus_cmd);
                // printf("<SEND_%d>: sending messages of length greater than %d, so we divide it\n", sender->send_id, MAX_FRAME_SIZE);
            }

            //This is probably ONLY one step you want
            // 接下来，要将cmd组装为帧
            
            //创建帧并初始化
            Frame * outgoing_frame = (Frame *) malloc (sizeof(Frame)); // 将char转化为frame(帧)
            memset(outgoing_frame, 0, MAX_FRAME_SIZE);

            // 给该帧赋值,并加上crc-16
            outgoing_frame->src = outgoing_cmd->src_id;
            outgoing_frame->dst = outgoing_cmd->dst_id;
            outgoing_frame->seq = (sender->swp[outgoing_cmd->dst_id].left + bias) % MAX_SEQ;
            outgoing_frame->seg = seg_flag;
            strcpy(outgoing_frame->data, outgoing_cmd->message);
            outgoing_frame->crc = crc_16((unsigned char *)outgoing_frame, MAX_FRAME_SIZE); 

            //放入发送方的缓存窗口的对应位置，并设置该位置已占用
            memcpy(&sender->swp[outgoing_cmd->dst_id].buffer[bias], outgoing_frame, sizeof(Frame));
            sender->swp[outgoing_cmd->dst_id].window_flag[bias] = true;

            //设置计时器-设置该帧的超时预期时间
            struct timeval send_time;
            gettimeofday(&send_time, NULL);
            sender->exiring_timeval[outgoing_frame->dst][bias].tv_sec = send_time.tv_sec;
            sender->exiring_timeval[outgoing_frame->dst][bias].tv_usec = send_time.tv_usec + 100000;
            if (sender->exiring_timeval[outgoing_cmd->dst_id][bias].tv_usec >= 1000000) 
            {
            sender->exiring_timeval[outgoing_cmd->dst_id][bias].tv_usec -= 1000000;
            sender->exiring_timeval[outgoing_cmd->dst_id][bias].tv_sec++;
            }

            //At this point, we don't need the outgoing_cmd - 释放输入cmd的内存
            free(outgoing_cmd->message);
            free(outgoing_cmd);

            //Convert the message to the outgoing_charbuf - 将帧再转化为字符，放进双向链表的数据结构LLnode里
            char * outgoing_charbuf = convert_frame_to_char(outgoing_frame);
            // 调试信息 - 发送消息
            fprintf(stderr, "Sender send a message:<SEND_%d>---<RECV_%d>:[%s]\n", outgoing_frame->src, outgoing_frame->dst, outgoing_frame->data);
            ll_append_node(outgoing_frames_head_ptr,
                            outgoing_charbuf);
            free(outgoing_frame);
        }
        else //发送窗口已经满了,把消息先再存回队列中
        {
            // 调试信息 - 窗口已满的提示
            fprintf(stderr, "don't have enough memory to send the<SEND_%d>---<RECV_%d>:[%s], wait...\n", outgoing_cmd->src_id, outgoing_cmd->dst_id, outgoing_cmd->message);
            ll_append_node(&sender->input_cmdlist_head, (void *) outgoing_cmd);
        }
    }   
}


// 处理超时的帧
void handle_timedout_frames(Sender * sender,
                            LLnode ** outgoing_frames_head_ptr)
{
    //TODO: Suggested steps for handling timed out datagrams
    //    1) Iterate through the sliding window protocol information you maintain for each receiver
    //       遍历您为每个接收器维护的滑动窗口协议信息
    //    2) Locate frames that are timed out and add them to the outgoing frames
    //       找到超时的帧并将其添加到传出的帧
    //    3) Update the next timeout field on the outgoing frames
    //       更新传出帧上的下一个超时字段
    struct timeval curr_time; //当前时间

    for (int i = 0; i < sender->num_of_receviers; i++)
    {
        for (int j = 0; j < WINDOW_SIZE; j++)
        {
            if(sender->exiring_timeval[i][j].tv_sec == 0 && sender->exiring_timeval[i][j].tv_usec == 0)
            {
                continue;
            }
            else
            {
                gettimeofday(&curr_time, NULL); //获取当前时间

                if (timeval_usecdiff(&sender->exiring_timeval[i][j], &curr_time)) //当前时间已超过规定截止时间
                {
                   // 调试信息 - 显示需要重传的帧
                   fprintf(stderr, "Re send<SEND_%d>---<RECV_%d>:[%s]\n", sender->send_id, i, sender->swp[i].buffer[j].data);
                   ll_append_node(outgoing_frames_head_ptr, convert_frame_to_char(&sender->swp[i].buffer[j])); //再将其放入发送队列中，重传
                   sender->exiring_timeval[i][j].tv_sec = curr_time.tv_sec;
                   sender->exiring_timeval[i][j].tv_usec = curr_time.tv_usec + 100000;
                    if (sender->exiring_timeval[i][j].tv_usec >= 1000000) 
                    {
                        sender->exiring_timeval[i][j].tv_usec -= 1000000;
                        sender->exiring_timeval[i][j].tv_sec++;
                    }
                }
                        
            }
        }
        
    }
    
    

}


// 发送方核心逻辑代码
void * run_sender(void * input_sender)
{    
    struct timespec   time_spec;
    struct timeval    curr_timeval;
    const int WAIT_SEC_TIME = 0;
    const long WAIT_USEC_TIME = 100000;
    Sender * sender = (Sender *) input_sender;    
    LLnode * outgoing_frames_head;
    struct timeval * expiring_timeval;
    long sleep_usec_time, sleep_sec_time;
    
    //This incomplete sender thread, at a high level, loops as follows:
    //1. Determine the next time the thread should wake up
    //2. Grab the mutex protecting the input_cmd/inframe queues
    //3. Dequeues messages from the input queue and adds them to the outgoing_frames list
    //4. Releases the lock
    //5. Sends out the messages

    //此不完整的发送方线程在较高级别上循环如下：
     // 1。 确定线程下次应唤醒的时间
     // 2。 获取互斥锁以保护input_cmd / inframe队列
     // 3。 使消息从输入队列中出队，并将它们添加到Outing_frames列表中
     // 4。 释放锁
     // 5。 发送消息

    pthread_cond_init(&sender->buffer_cv, NULL);
    pthread_mutex_init(&sender->buffer_mutex, NULL);

    while(1)
    {    
        outgoing_frames_head = NULL;

        //Get the current time
        gettimeofday(&curr_timeval, 
                     NULL);

        //time_spec is a data structure used to specify when the thread should wake up
        //The time is specified as an ABSOLUTE (meaning, conceptually, you specify 9/23/2010 @ 1pm, wakeup)
        // time_spec是用于指定线程何时应唤醒的数据结构
        // 将时间指定为绝对时间（从概念上讲，您指定9/23/2010 @ 1pm，唤醒）
        time_spec.tv_sec  = curr_timeval.tv_sec;
        time_spec.tv_nsec = curr_timeval.tv_usec * 1000;

        //Check for the next event we should handle
        expiring_timeval = sender_get_next_expiring_timeval(sender); // 下一个超时的时间

        //Perform full on timeout - 执行完整的超时
        if (expiring_timeval == NULL)
        {
            time_spec.tv_sec += WAIT_SEC_TIME;
            time_spec.tv_nsec += WAIT_USEC_TIME * 1000;
        }
        else
        {
            //Take the difference between the next event and the current time
            //获取下一个事件与当前时间之间的差
            sleep_usec_time = timeval_usecdiff(&curr_timeval,
                                               expiring_timeval);

            //Sleep if the difference is positive
            //如果差异为正则睡眠
            if (sleep_usec_time > 0)
            {
                sleep_sec_time = sleep_usec_time/1000000;
                sleep_usec_time = sleep_usec_time % 1000000;   
                time_spec.tv_sec += sleep_sec_time;
                time_spec.tv_nsec += sleep_usec_time*1000;
            }   
        }

        //Check to make sure we didn't "overflow" the nanosecond field
        //检查以确保我们没有“溢出”纳秒级字段
        if (time_spec.tv_nsec >= 1000000000)
        {
            time_spec.tv_sec++;
            time_spec.tv_nsec -= 1000000000;
        }

        
        //*****************************************************************************************
        //NOTE: Anything that involves dequeing from the input frames or input commands should go 
        //      between the mutex lock and unlock, because other threads CAN/WILL access these structures
        //*****************************************************************************************
        pthread_mutex_lock(&sender->buffer_mutex);

        //Check whether anything has arrived
        int input_cmd_length = ll_get_length(sender->input_cmdlist_head);
        int inframe_queue_length = ll_get_length(sender->input_framelist_head);
        
        //Nothing (cmd nor incoming frame) has arrived, so do a timed wait on the sender's condition variable (releases lock)
        //A signal on the condition variable will wakeup the thread and reaquire the lock
        if (input_cmd_length == 0 &&
            inframe_queue_length == 0)
        {
            
            pthread_cond_timedwait(&sender->buffer_cv, 
                                   &sender->buffer_mutex,
                                   &time_spec);
        }
        //Implement this - 先处理接收方返回的ack帧
        handle_incoming_acks(sender,
                             &outgoing_frames_head);

        //Implement this - 再处理网络层(我们手动输入)的待发消息
        handle_input_cmds(sender,
                          &outgoing_frames_head);

        pthread_mutex_unlock(&sender->buffer_mutex);


        //Implement this - 处理超时的帧
        handle_timedout_frames(sender,
                               &outgoing_frames_head);

        //CHANGE THIS AT YOUR OWN RISK!
        //Send out all the frames
        //根据自己的风险更改此设置！
        //发送所有帧
        int ll_outgoing_frame_length = ll_get_length(outgoing_frames_head);
        
        while(ll_outgoing_frame_length > 0)
        {
            LLnode * ll_outframe_node = ll_pop_node(&outgoing_frames_head);
            char * char_buf = (char *)  ll_outframe_node->value;

            //Don't worry about freeing the char_buf, the following function does that
            send_msg_to_receivers(char_buf);

            //Free up the ll_outframe_node
            free(ll_outframe_node);

            ll_outgoing_frame_length = ll_get_length(outgoing_frames_head);
        }
    }
    pthread_exit(NULL);
    return 0;
}
