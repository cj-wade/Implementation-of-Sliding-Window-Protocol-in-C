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
#include "input.h"
#include "communicate.h"
#include "receiver.h"
#include "sender.h"

// 负责处理命令行选项和初始化数据结构
int main(int argc, char *argv[])
{
    pthread_t stdin_thread;
    pthread_t * sender_threads;
    pthread_t * receiver_threads;
    int i;
    unsigned char print_usage = 0;

    //DO NOT CHANGE THIS
    //Set the number of bits to corrupt
    CORRUPTION_BITS = (int) MAX_FRAME_SIZE/2; // 误码位赋值

    //DO NOT CHANGE THIS
    //Prepare the glb_sysconfig object // 初始配置文件里的变量
    glb_sysconfig.drop_prob = 0;
    glb_sysconfig.corrupt_prob = 0;
    glb_sysconfig.automated = 0;
    memset(glb_sysconfig.automated_file,
           0,
           AUTOMATED_FILENAME);

    //DO NOT CHANGE THIS
    //Prepare other variables and seed the psuedo random number generator
    glb_receivers_array_length = -1;
    glb_senders_array_length = -1;
    srand(time(NULL)); // 用于产生随机时间种子

    //Parse out the command line arguments - 解析命令行参数
    for(i=1; i < argc;) 
    {
      if(strcmp(argv[i], "-s") == 0) // 给发送方的数量赋值
      {
          sscanf(argv[i+1], 
                 "%d", 
                 &glb_senders_array_length);
          i += 2;
      }

      else if(strcmp(argv[i], "-r") == 0) // 给接收方的数量赋值
      {
          sscanf(argv[i+1], 
                 "%d", 
                 &glb_receivers_array_length);
          i += 2;
      }      
      else if(strcmp(argv[i], "-d") == 0) // 给丢包率赋值
      {
          sscanf(argv[i+1], 
                 "%f", 
                 &glb_sysconfig.drop_prob);
          i += 2;
      }      
      else if(strcmp(argv[i], "-c") == 0) // 给误码率赋值
      {
          sscanf(argv[i+1], 
                 "%f", 
                 &glb_sysconfig.corrupt_prob);
          i += 2;
      }
      else if(strcmp(argv[i], "-a") == 0) // don't care, not about the project
      {
          int filename_len = strlen(argv[i+1]);
          if (filename_len < AUTOMATED_FILENAME)
          {
              glb_sysconfig.automated = 1;
              strcpy(glb_sysconfig.automated_file, 
                     argv[i+1]);
          }
          i += 2;
      }     
      else if(strcmp(argv[i], "-h") == 0) // don't care, not about the project
      {
          print_usage=1;
          i++;
      }     
      else
      {
          i++;
      }
    }

    //Spot check the input variables - 抽查输入变量(保证命令输入的各类值是合法的)
    if (glb_senders_array_length <= 0 ||
        glb_receivers_array_length <= 0 ||
        (glb_sysconfig.drop_prob < 0 || glb_sysconfig.drop_prob > 1) ||
        (glb_sysconfig.corrupt_prob < 0 || glb_sysconfig.corrupt_prob > 1) ||
        print_usage)
    {
        fprintf(stderr, "USAGE: etherchat \n   -r int [# of receivers] \n   -s int [# of senders] \n   -c float [0 <= corruption prob <= 1] \n   -d float [0 <= drop prob <= 1]\n");
        exit(1);
    }
        
    
    //DO NOT CHANGE THIS
    //Create the standard input thread
    int rc = pthread_create(&stdin_thread,
                            NULL,
                            run_stdinthread, // 这里用到了input.c里的函数run_stdinthread，产生输入线程
                            (void *) 0);
    if (rc)
    {
        fprintf(stderr, "ERROR; return code from pthread_create() is %d\n", rc);
        exit(-1);
    }

    //DO NOT CHANGE THIS
    //Init the pthreads data structure - 给发送方们和接收方们的线程分配内存
    sender_threads = (pthread_t *) malloc(sizeof(pthread_t) * glb_senders_array_length);
    receiver_threads = (pthread_t *) malloc(sizeof(pthread_t) * glb_receivers_array_length);

    //Init the global senders array - 给发送方们和接收方们的数据结构分配内存 
    glb_senders_array = (Sender *) malloc(glb_senders_array_length * sizeof(Sender));
    glb_receivers_array = (Receiver *) malloc(glb_receivers_array_length * sizeof(Receiver));
    
    fprintf(stderr, "Messages will be dropped with probability=%f\n", glb_sysconfig.drop_prob);
    fprintf(stderr, "Messages will be corrupted with probability=%f\n", glb_sysconfig.corrupt_prob);
    fprintf(stderr, "Available sender id(s):\n");

    //Init sender objects, assign ids - 初始化具体的发送方对象，分配id
    //NOTE: Do whatever initialization you want here or inside the init_sender function
    for (i = 0;
         i < glb_senders_array_length;
         i++)
    {
        init_sender(&glb_senders_array[i], glb_receivers_array_length, i);
        fprintf(stderr, "   send_id=%d\n", i);
        
    }

    //Init receiver objects, assign ids - 初始化具体的接收方对象，分配id
    //NOTE: Do whatever initialization you want here or inside the init_receiver function
    fprintf(stderr, "Available receiver id(s):\n");
    for (i = 0;
         i < glb_receivers_array_length;
         i++)
    {
        init_receiver(&glb_receivers_array[i], glb_senders_array_length, i);
        fprintf(stderr, "   recv_id=%d\n", i);
    }
    
    //Spawn sender threads - 产生发送方的线程
    for (i = 0;
         i < glb_senders_array_length;
         i++)
    {
        rc = pthread_create(sender_threads+i,
                            NULL,
                            run_sender,
                            (void *) &glb_senders_array[i]);
        if (rc){
            fprintf(stderr, "ERROR; return code from pthread_create() is %d\n", rc);
            exit(-1);
        }
    }

    //Spawn receiver threads - 产生接收方的线程
    for (i = 0;
         i < glb_receivers_array_length;
         i++)
    {
        rc = pthread_create(receiver_threads+i,
                            NULL,
                            run_receiver,
                            (void *) &glb_receivers_array[i]);
        if (rc){
            fprintf(stderr, "ERROR; return code from pthread_create() is %d\n", rc);
            exit(-1);
        }
    }
    pthread_join(stdin_thread, NULL); // 等待stdin_thread线程
    // pthread_join()的作用可以这样理解：主线程等待子线程的终止。
    // 也就是在子线程调用了pthread_join()方法后面的代码，只有等到子线程结束了才能执行
    for (i = 0;
         i < glb_senders_array_length;
         i++)
    {
        pthread_cancel(sender_threads[i]); // 发送请求终止信号给sender，但不代表一定会执行终止
        pthread_join(sender_threads[i], NULL); // 等待sender_threads线程
    }

    for (i = 0;
         i < glb_receivers_array_length;
         i++)
    {
        pthread_cancel(receiver_threads[i]);
        pthread_join(receiver_threads[i], NULL); // 等待sender_threads线程
    }

    free(sender_threads);
    free(receiver_threads);
    free(glb_senders_array);
    free(glb_receivers_array);

    return 0;
}
