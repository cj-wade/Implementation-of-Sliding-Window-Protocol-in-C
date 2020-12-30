#include "input.h"
// 负责处理用户输入的讯息(例如msg 0 0 hello world)
//*********************************************************************
//NOTE: We will overwrite this file, so whatever changes you put here
//      WILL NOT persist
//*********************************************************************
// 处理输入，通过非阻塞方式，将标准输入内容放入发送线程的 输入队列 
/* getline implementation is copied from glibc. */

#ifndef SIZE_MAX
# define SIZE_MAX ((size_t) -1)
#endif
#ifndef SSIZE_MAX
# define SSIZE_MAX ((ssize_t) (SIZE_MAX / 2))
#endif

ssize_t getline (char **lineptr, size_t *n, FILE *fp)
{
    ssize_t result;
    size_t cur_len = 0;

    if (lineptr == NULL || n == NULL || fp == NULL)
    {
	return -1;
    }

    if (*lineptr == NULL || *n == 0)
    {
	*n = 120;
	*lineptr = (char *) malloc (*n);
	if (*lineptr == NULL)
	{
	    result = -1;
	    goto end;
	}
    }

    for (;;)
    {
	int i;

	i = getc (fp);
	if (i == EOF)
	{
	    result = -1;
	    break;
	}

	/* Make enough space for len+1 (for final NUL) bytes.  */
	if (cur_len + 1 >= *n)
	{
	    size_t needed_max =
		SSIZE_MAX < SIZE_MAX ? (size_t) SSIZE_MAX + 1 : SIZE_MAX;
	    size_t needed = 2 * *n + 1;   /* Be generous. */
           char *new_lineptr;

	    if (needed_max < needed)
		needed = needed_max;
	    if (cur_len + 1 >= needed)
	    {
		result = -1;
		goto end;
	    }

	    new_lineptr = (char *) realloc (*lineptr, needed);
	    if (new_lineptr == NULL)
	    {
		result = -1;
		goto end;
	    }

	    *lineptr = new_lineptr;
	    *n = needed;
	}

	(*lineptr)[cur_len] = i;
	cur_len++;

	if (i == '\n')
	    break;
    }
    (*lineptr)[cur_len] = '\0';
    result = cur_len ? (ssize_t) cur_len : result;

end:
    return result;
}

void * run_stdinthread(void *threadid) //运行输入线程
{
    fd_set read_fds, master_fds;
    // fd_set（它比较重要所以先介绍一下）是一组文件描述字(fd)的集合，它用一位来表示一个fd（下面会仔细介绍）
    // 对于fd_set类型通过下面四个宏来操作：
    // FD_ZERO(fd_set *fdset);将指定的文件描述符集清空，在对文件描述符集合进行设置前，必须对其进行初始化，
    //                      如果不清空，由于在系统分配内存空间后，通常并不作清空处理，所以结果是不可知的。
    //  FD_SET(fd_set *fdset);用于在文件描述符集合中增加一个新的文件描述符。
    //  FD_CLR(fd_set *fdset);用于在文件描述符集合中删除一个文件描述符。
    //  FD_ISSET(int fd,fd_set *fdset);用于测试指定的文件描述符是否在该集合中。
    int fd_max;
    int select_res;
    int sender_id;
    int receiver_id;
    int sscanf_res;
    size_t input_buffer_size = (size_t) DEFAULT_INPUT_BUFFER_SIZE;
    char * input_buffer;
    char * input_message;    
    int input_bytes_read;
    char input_command[MAX_COMMAND_LENGTH];
    Sender * sender;
     

    //Zero out fd_sets - 初始化文件描述字集合
    FD_ZERO(&read_fds);
    FD_ZERO(&master_fds);

    //Add standard input to the fd_set - 添加新的文件描述符
    FD_SET(STDIN_FILENO, &master_fds);
    fd_max = STDIN_FILENO;
    
    while(1){
        //Copy the master set to avoid permanently altering the master set-复制主集合以避免永久性篡改主集合
        read_fds = master_fds; 

        if ((select_res = select(fd_max+1,
                                 &read_fds, 
                                 NULL, 
                                 NULL, 
                                 NULL)) == -1) {
            perror("Select failed - exiting program");
            pthread_exit(NULL);
        }

        //User inputted a string, read in input and prepare to send-用户输入了一个字符串，读入了输入并准备发送 
        if (FD_ISSET(STDIN_FILENO, &read_fds))
        {
            
            input_buffer = (char *) malloc (input_buffer_size * sizeof(char));
            
            //NULL set the entire input buffer-NULL设置整个输入缓冲区
            memset(input_buffer, 
                   0, 
                   input_buffer_size*sizeof(char));
            
            //Read in the command line into the input buffer-读取命令行输入缓冲区
            input_bytes_read = getline(&input_buffer, &input_buffer_size, stdin);
            
            //Zero out the readin buffers for the command-将命令行读取缓冲区清零
            memset(input_command, 
                   0, 
                   MAX_COMMAND_LENGTH*sizeof(char));

            //Zero out the memory for the message to communicate-将内存清零以进行消息通信
            input_message = (char *) malloc((input_bytes_read+1) * sizeof(char));
            memset(input_message, 
                   0, 
                   (input_bytes_read+1) * sizeof(char));

            //Scan the input for the arguments-扫描输入中的参数
            sscanf_res =  sscanf(input_buffer,
                                 "%s %d %d %[^\n]",
                                 input_command,
                                 &sender_id,
                                 &receiver_id,
                                 input_message);
            
            //Number of parsed objects is less than expected-解析的对象数少于预期
            if (sscanf_res < 4)
            {
                if (strcmp(input_command, "exit") == 0)
                {
                    free(input_message);
                    free(input_buffer);
                    return 0;
                }
                else
                {
                    fprintf(stderr, "Command is ill-formatted\n");
                }
            }
            else
            {
                if (strcmp(input_command,"msg") == 0)
                {
                    //Check to ensure that the sender and receiver ids are in the right range
                    //检查以确保发送方和接收方ID在正确的范围内
                    if (sender_id >= glb_senders_array_length || 
                        sender_id < 0)
                    {
                        fprintf(stderr, "Sender id is invalid\n");
                    }
                    if (receiver_id >= glb_receivers_array_length || 
                        receiver_id < 0)
                    {
                        fprintf(stderr, "Receiver id is invalid\n");
                    }
                    
                    //Only add if valid - 只有合法才添加
                    if (sender_id < glb_senders_array_length &&
                        receiver_id < glb_receivers_array_length && 
                        sender_id >= 0 &&
                        receiver_id >= 0)
                    {
                        //Add the message to the receive buffer for the appropriate thread
                        //对适当的线程往接收缓冲区里增加消息
                        Cmd * outgoing_cmd = (Cmd *) malloc(sizeof(Cmd));
                        char * outgoing_msg = (char *) malloc(sizeof(char) * (strlen(input_message)+1));
                        
                        //Copy out the input message into the outgoing command object
                        strcpy(outgoing_msg,
                               input_message);
                        
                        outgoing_cmd->src_id=sender_id;
                        outgoing_cmd->dst_id=receiver_id;
                        outgoing_cmd->message = outgoing_msg;
                        
                        //Add it to the appropriate input buffer 
                        sender = &glb_senders_array[sender_id];
                        
                        //Lock the buffer, add to the input list, and signal the thread
                        pthread_mutex_lock(&sender->buffer_mutex);
                        ll_append_node(&sender->input_cmdlist_head,
                                       (void *) outgoing_cmd);
                        pthread_cond_signal(&sender->buffer_cv);
                        pthread_mutex_unlock(&sender->buffer_mutex);
                    }
                }
                else
                {
                    fprintf(stderr, "Unknown command:%s\n", input_buffer);
                }
            }
            
            //Lastly, free the input_buffer and the input_message
            free(input_buffer);
            free(input_message);
        }
    }
    pthread_exit(NULL);
}
