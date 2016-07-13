#include "ring_buffer.h"
#include "pthread.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>

#define RB_SIZE (1024*1024)
#define CHECK_LEN (1024*1024*10)

static bool b_enable_check = true;

struct rb_ctx{
    int chunk;
    unsigned char data[16];
};

struct rb_thread_arg{
    RingBuffer *p_rb;
    void       *p_mem;
    int         count;
};

void rb_ctx_create(rb_ctx *p_ctx){
    if(!b_enable_check) return;
    p_ctx->chunk = 0;
    size_t l = sizeof(p_ctx->data);
    for(size_t i=0; i!=l; ++i){
        p_ctx->data[i] = rand() % 200;
        p_ctx->chunk += p_ctx->data[i];
    }
}

bool rb_ctx_check(rb_ctx *p_ctx){
    if(!b_enable_check) return true;
    int chunk = 0;
    size_t l = sizeof(p_ctx->data);
    for(size_t i=0; i!=l; ++i){
        chunk += p_ctx->data[i];
    }
    return chunk == p_ctx->chunk;
}

void* rb_writter(void *p_arg){
    rb_thread_arg *a = (rb_thread_arg*)p_arg;
    int writted = 0;
    while(true){
        if(writted >= a->count){
            break;
        }
        rb_ctx ctx;
        rb_ctx_create(&ctx);
        int ret = a->p_rb->push(&ctx, sizeof(ctx), a->p_mem);
        if(ret == 0){
            writted++;
        }else{
            usleep(1);
        }
    }
    return NULL;
}

void* rb_reader(void *p_arg){
    int readed = 0;
    rb_thread_arg *a = (rb_thread_arg*)p_arg;
    while(true){
        if(readed >= a->count){
            break;
        }
        rb_ctx ctx;
        unsigned int iLen = (unsigned int)sizeof(ctx);
        int ret = a->p_rb->pop(&ctx, &iLen, a->p_mem); 
        if(ret == 0){
            readed++;
            if(!rb_ctx_check(&ctx)){
                printf("!!!check error!!!\n");
                return NULL;
            }
        }else{
            usleep(1);
        }
    }
    printf("check success\n");
    return NULL;
}

void rb_1writter_1reader(){
    printf("begin check 1 writter & 1 reader:");
    void *p_mem = malloc(RB_SIZE);
    RingBuffer rb(RB_SIZE, false, false);
    rb_thread_arg thread_info;
    thread_info.count = CHECK_LEN;
    thread_info.p_rb = &rb;
    thread_info.p_mem = p_mem;
    pthread_t thread_writter, thread_reader;
    pthread_create(&thread_writter, NULL, rb_writter, &thread_info);
    pthread_create(&thread_reader, NULL, rb_reader, &thread_info);
    pthread_join(thread_reader, NULL);
    free(p_mem);
}

void rb_3writter_1reader(){
    printf("begin check 3 writter & 1 reader:");
    void *p_mem = malloc(RB_SIZE);
    RingBuffer rb(RB_SIZE, false, false);
    rb_thread_arg thread_info;
    thread_info.count = CHECK_LEN;
    thread_info.p_rb = &rb;
    thread_info.p_mem = p_mem;
    pthread_t thread_writter[3];
    pthread_t thread_reader;
    pthread_create(&thread_writter[1], NULL, rb_writter, &thread_info);
    pthread_create(&thread_writter[2], NULL, rb_writter, &thread_info);
    pthread_create(&thread_writter[0], NULL, rb_writter, &thread_info);

    rb_thread_arg thread_info_reader;
    thread_info_reader.count = 3*CHECK_LEN;
    thread_info_reader.p_rb = &rb;
    thread_info_reader.p_mem = p_mem;
    pthread_create(&thread_reader, NULL, rb_reader, &thread_info_reader);
    pthread_join(thread_reader, NULL);
    free(p_mem);
}

void rb_1writter_3reader(){
    printf("begin check 1 writter & 3 reader:");
    void *p_mem = malloc(RB_SIZE);
    RingBuffer rb(RB_SIZE, false, false);
    rb_thread_arg thread_info;
    thread_info.count = CHECK_LEN*3;
    thread_info.p_rb = &rb;
    thread_info.p_mem = p_mem;
    pthread_t thread_writter;
    pthread_create(&thread_writter, NULL, rb_writter, &thread_info);

    rb_thread_arg thread_info_reader;
    thread_info_reader.count = CHECK_LEN;
    thread_info_reader.p_rb = &rb;
    thread_info_reader.p_mem = p_mem;
    pthread_t thread_reader[3];
    pthread_create(&thread_reader[0], NULL, rb_reader, &thread_info_reader);
    pthread_create(&thread_reader[1], NULL, rb_reader, &thread_info_reader);
    pthread_create(&thread_reader[2], NULL, rb_reader, &thread_info_reader);
    for(int i=0; i!=3; ++i){
        pthread_join(thread_reader[i], NULL);
    }
    free(p_mem);
}

int main(){
    rb_1writter_1reader();
    b_enable_check = false;
    timeval t1, t2;
    gettimeofday(&t1, NULL);
    rb_1writter_1reader();
    gettimeofday(&t2, NULL);
    printf("20Byte:time used %ld ms\n", 1000*1000*(t2.tv_sec - t1.tv_sec) + t2.tv_usec - t1.tv_usec);
    //rb_3writter_1reader();
    //rb_1writter_3reader();
}

