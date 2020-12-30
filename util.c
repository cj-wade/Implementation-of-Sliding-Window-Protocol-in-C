#include "util.h"
// 包含实用函数，即提供的链表实现的所有实用函数——滑动窗口协议可在此实现
//Linked list functions
int ll_get_length(LLnode * head)
{
    LLnode * tmp;
    int count = 1;
    if (head == NULL)
        return 0;
    else
    {
        tmp = head->next;
        while (tmp != head)
        {
            count++;
            tmp = tmp->next;
        }
        return count;
    }
}

void ll_append_node(LLnode ** head_ptr, 
                    void * value)
{
    LLnode * prev_last_node;
    LLnode * new_node;
    LLnode * head;

    if (head_ptr == NULL)
    {
        return;
    }
    
    //Init the value pntr
    head = (*head_ptr);
    new_node = (LLnode *) malloc(sizeof(LLnode));
    new_node->value = value;

    //The list is empty, no node is currently present
    if (head == NULL)
    {
        (*head_ptr) = new_node;
        new_node->prev = new_node;
        new_node->next = new_node;
    }
    else
    {
        //Node exists by itself
        prev_last_node = head->prev;
        head->prev = new_node;
        prev_last_node->next = new_node;
        new_node->next = head;
        new_node->prev = prev_last_node;
    }
}


LLnode * ll_pop_node(LLnode ** head_ptr)
{
    LLnode * last_node;
    LLnode * new_head;
    LLnode * prev_head;

    prev_head = (*head_ptr);
    if (prev_head == NULL)
    {
        return NULL;
    }
    last_node = prev_head->prev;
    new_head = prev_head->next;

    //We are about to set the head ptr to nothing because there is only one thing in list
    if (last_node == prev_head)
    {
        (*head_ptr) = NULL;
        prev_head->next = NULL;
        prev_head->prev = NULL;
        return prev_head;
    }
    else
    {
        (*head_ptr) = new_head;
        last_node->next = new_head;
        new_head->prev = last_node;

        prev_head->next = NULL;
        prev_head->prev = NULL;
        return prev_head;
    }
}

void ll_destroy_node(LLnode * node)
{
    if (node->type == llt_string)
    {
        free((char *) node->value);
    }
    free(node);
}

//Compute the difference in usec for two timeval objects
// 返回时间差(以微妙为单位)
long timeval_usecdiff(struct timeval *start_time, 
                      struct timeval *finish_time)
{
  long usec;
  usec=(finish_time->tv_sec - start_time->tv_sec)*1000000;
  usec+=(finish_time->tv_usec- start_time->tv_usec);
  return usec;
}


//Print out messages entered by the user
void print_cmd(Cmd * cmd)
{
    fprintf(stderr, "src=%d, dst=%d, message=%s\n", 
           cmd->src_id,
           cmd->dst_id,
           cmd->message);
}


char * convert_frame_to_char(Frame * frame)
{
    //TODO: You should implement this as necessary
    char * char_buffer = (char *) malloc(sizeof(Frame));
    memset(char_buffer,
           0,
           sizeof(Frame));
	//小改动，frame->data改成了frame，因为添加了新的变量
    memcpy(char_buffer, 
           frame,
           sizeof(Frame));
    return char_buffer;
}


Frame * convert_char_to_frame(char * char_buf)
{
    //TODO: You should implement this as necessary
    Frame * frame = (Frame *) malloc(sizeof(Frame));
	//初始化
    memset(frame,
           0,
           sizeof(Frame));
	//和上一个函数对应，直接用memcpy就行了
    memcpy(frame, 
           char_buf,
           sizeof(Frame));
    return frame;
}


//内存循环左移，对左移后原来的位置重置
// p-左移的内存起始位置,
// tmp-用于重置的替换项
// window_size-滑动窗口大小
// n-左移次数
// memoery_size-替换用的内存大小
void shift_memory_left(void * p, void * tmp, int window_size, int n, int memoery_size)
{
    for (int j = 0; j < n; j++)
    {
        for (int i = 0; i < window_size - 1; i++)
        {
            memcpy((p + i * memoery_size), (p + (i + 1) * memoery_size), memoery_size);
        }
    }
    for (int i = window_size - n; i < window_size; i++)
    {
        memcpy((p + i * memoery_size), tmp, memoery_size);
    }
}