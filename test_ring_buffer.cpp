#include "ring_buffer.h"
#include "pthread.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
#include "atomic_ops.h"

#define RB_SIZE_BIG (1024*1024*512)
#define RB_SIZE_SMALL (1024*1024)
#define CHECK_LEN (1024*1024)

#define WATCH(func, size)\
    {\
    b_enable_check = false;\
    timeval t1, t2;\
    gettimeofday(&t1, NULL);\
    func;\
    gettimeofday(&t2, NULL);\
    unsigned long t = 1000*1000*(t2.tv_sec - t1.tv_sec) + t2.tv_usec - t1.tv_usec;\
    unsigned long qps = 1000 * 1000 * (unsigned long)CHECK_LEN / t;\
    printf("%4d Byte %60s :time used %ld ms qps %ld\n", size, #func, t, qps);\
    }\

static bool b_enable_check = true;
static int stop_thread = 0;

unsigned int crc32(unsigned int crc, const void *buf, int size);

struct rb_ctx{
    unsigned int chunk;
    unsigned char data[32];
};

struct rb_ctx_128{
    unsigned int chunk;
    unsigned char data[128];
};

struct rb_ctx_4096{
    unsigned int chunk;
    unsigned char data[4096];
};

struct rb_thread_arg{
    RingBuffer *p_rb;
    void       *p_mem;
    int         count;
};

template <typename T>
void rb_ctx_create(T *p_ctx){
    if(!b_enable_check) return;
    p_ctx->chunk = 0;
    size_t l = sizeof(p_ctx->data);
    for(size_t i=0; i!=l; ++i){
        p_ctx->data[i] = rand() % 200;
    }
    p_ctx->chunk = crc32(0, p_ctx->data, l);
}

template <typename T>
bool rb_ctx_check(T *p_ctx){
    if(!b_enable_check) return true;
    size_t l = sizeof(p_ctx->data);
    unsigned int chunk = crc32(0, p_ctx->data, l);
    return chunk == p_ctx->chunk;
}

template <typename T>
void* rb_writter(void *p_arg){
    rb_thread_arg *a = (rb_thread_arg*)p_arg;
    int writted = 0;
    while(true){
        if(writted >= a->count || stop_thread > 0){
            break;
        }
        T ctx;
        rb_ctx_create(&ctx);
        int ret = a->p_rb->push(&ctx, sizeof(ctx), a->p_mem);
        if(ret == 0){
            writted++;
        }else{
            usleep(10);
        }
    }
    return NULL;
}

template <typename T>
void* rb_reader(void *p_arg){
    int readed = 0;
    rb_thread_arg *a = (rb_thread_arg*)p_arg;
    while(true){
        if(readed >= a->count || stop_thread > 0){
            break;
        }
        T ctx;
        unsigned int iLen = (unsigned int)sizeof(ctx);
        int ret = a->p_rb->pop(&ctx, &iLen, a->p_mem); 
        if(ret == 0){
            readed++;
            if(!rb_ctx_check(&ctx)){
                printf("!!!check error!!!");
                AtomicAdd(&stop_thread, 1);
                return NULL;
            }
        }else if(ret == -2){    
            printf("!!!check error len!!!");
            AtomicAdd(&stop_thread, 1);
            return NULL;
        }else{
            usleep(10);
        }
    }
    return NULL;
}

template <typename T>
void* rb_peeker(void *p_arg){
    int readed = 0;
    rb_thread_arg *a = (rb_thread_arg*)p_arg;
    while(true){
        if(readed >= a->count || stop_thread > 0){
            break;
        }
        unsigned int len = 0; 
        T* p_ctx = (T*)a->p_rb->peek(&len, 0, a->p_mem);
        if(p_ctx){
            readed ++;
            a->p_rb->remove(a->p_mem);
            if(!rb_ctx_check(p_ctx) || len != sizeof(T)){
                printf("!!!check error!!!");
                AtomicAdd(&stop_thread, 1);
                return NULL;
            }
        }else{
            usleep(10);
        }
    }
    return NULL;
}

template <typename T>
void rb_1writter_1reader(bool peeker, int size){
    stop_thread = 0;
    if(peeker){
        printf("begin check 1 writter & 1 peeker:");
    }else{
        printf("begin check 1 writter & 1 reader:");
    }
    void *p_mem = malloc(size);
    RingBuffer rb(size, false, false);
    rb_thread_arg thread_info;
    thread_info.count = CHECK_LEN;
    thread_info.p_rb = &rb;
    thread_info.p_mem = p_mem;
    pthread_t thread_writter, thread_reader;
    pthread_create(&thread_writter, NULL, rb_writter<T>, &thread_info);
    if(peeker){
        pthread_create(&thread_reader, NULL, rb_peeker<T>, &thread_info);
    }else{
        pthread_create(&thread_reader, NULL, rb_reader<T>, &thread_info);
    }
    pthread_join(thread_reader, NULL);
    printf(":check over\n");
    free(p_mem);
}

template <typename T>
void rb_3writter_1reader(int size){
    stop_thread = 0;
    printf("begin check 3 writter & 1 reader:");
    void *p_mem = malloc(size);
    RingBuffer rb(size, false, false);
    rb_thread_arg thread_info;
    thread_info.count = CHECK_LEN;
    thread_info.p_rb = &rb;
    thread_info.p_mem = p_mem;
    pthread_t thread_writter[3];
    pthread_t thread_reader;
    pthread_create(&thread_writter[1], NULL, rb_writter<T>, &thread_info);
    pthread_create(&thread_writter[2], NULL, rb_writter<T>, &thread_info);
    pthread_create(&thread_writter[0], NULL, rb_writter<T>, &thread_info);

    rb_thread_arg thread_info_reader;
    thread_info_reader.count = 3*CHECK_LEN;
    thread_info_reader.p_rb = &rb;
    thread_info_reader.p_mem = p_mem;
    pthread_create(&thread_reader, NULL, rb_reader<T>, &thread_info_reader);
    pthread_join(thread_reader, NULL);
    for(int i=0; i!=3; ++i){
        pthread_join(thread_writter[i], NULL);
    }
    printf(":check over\n");
    free(p_mem);
}

template <typename T>
void rb_1writter_3reader(int size){
    stop_thread = 0;
    printf("begin check 1 writter & 3 reader:");
    void *p_mem = malloc(size);
    RingBuffer rb(size, false, false);
    rb_thread_arg thread_info;
    thread_info.count = CHECK_LEN*3;
    thread_info.p_rb = &rb;
    thread_info.p_mem = p_mem;
    pthread_t thread_writter;
    pthread_create(&thread_writter, NULL, rb_writter<T>, &thread_info);

    rb_thread_arg thread_info_reader;
    thread_info_reader.count = CHECK_LEN;
    thread_info_reader.p_rb = &rb;
    thread_info_reader.p_mem = p_mem;
    pthread_t thread_reader[3];
    pthread_create(&thread_reader[0], NULL, rb_reader<T>, &thread_info_reader);
    pthread_create(&thread_reader[1], NULL, rb_reader<T>, &thread_info_reader);
    pthread_create(&thread_reader[2], NULL, rb_reader<T>, &thread_info_reader);
    for(int i=0; i!=3; ++i){
        pthread_join(thread_reader[i], NULL);
    }
    printf(":check over\n");
    free(p_mem);
}


int main(){
    rb_1writter_3reader<rb_ctx_128>(RB_SIZE_BIG);

    printf("test for small ringbuffer size\n");
    WATCH(rb_1writter_1reader<rb_ctx>(false, RB_SIZE_SMALL), 32);
    WATCH(rb_1writter_1reader<rb_ctx>(true, RB_SIZE_SMALL), 32);
    WATCH(rb_1writter_1reader<rb_ctx_128>(false, RB_SIZE_SMALL), 128);
    WATCH(rb_1writter_1reader<rb_ctx_128>(true, RB_SIZE_SMALL), 128);
    WATCH(rb_1writter_1reader<rb_ctx_4096>(false, RB_SIZE_SMALL), 4096);
    WATCH(rb_1writter_1reader<rb_ctx_4096>(true, RB_SIZE_SMALL), 4096);

    printf("test for big ringbuffer size\n");
    WATCH(rb_1writter_1reader<rb_ctx>(false, RB_SIZE_BIG), 32);
    WATCH(rb_1writter_1reader<rb_ctx>(true, RB_SIZE_BIG), 32);
    WATCH(rb_1writter_1reader<rb_ctx_128>(false, RB_SIZE_BIG), 128);
    WATCH(rb_1writter_1reader<rb_ctx_128>(true, RB_SIZE_BIG), 128);
    WATCH(rb_1writter_1reader<rb_ctx_4096>(false, RB_SIZE_BIG), 4096);
    WATCH(rb_1writter_1reader<rb_ctx_4096>(true, RB_SIZE_BIG), 4096);
}

