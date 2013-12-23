#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>

#include <sys/epoll.h>
#include <sys/time.h>

#include "hashmap.h"
#include "heap.h"
#include "app.h"

#if defined(DBG_APP)
#define pr_debug(fmt, ...) \
    printf("[%s][%d]" fmt, __func__, __LINE__, ##__VA_ARGS__)
#else
#define pr_debug(fmt, ...) \
    do {} while(0)
#endif

struct app_event {
    int fd;
    bool epolled;     /* 1: in epoll wait list, 0 not in */
    uint32_t events;

    void (*handler_rd)(int fd, void *arg);
    void (*handler_wr)(int fd, void *arg);
    void (*handler_er)(int fd, void *arg);
    void *arg_rd;
    void *arg_wr;
    void *arg_er;
};

bool app_event_epolled(app_event_t ev)
{
    struct app_event *e = ev; 
    return e->epolled;
}

app_event_t app_event_create(int fd)
{
    struct app_event *e = calloc(1, sizeof(struct app_event));
    if (!e) {
		perror("app_event_create");
		return NULL;
	}
    e->fd = fd;
    return e;
}

void app_event_add_notifier(app_event_t ev, enum app_notifier_t type, 
                            void (*handler)(int, void *), void *arg)
{
    struct app_event *e = ev; 
    switch (type) {
    case NOTIFIER_READ:
        e->events |= EPOLLIN;
        e->handler_rd = handler;
        e->arg_rd = arg;
        break;
    case NOTIFIER_WRITE:
        e->events |= EPOLLOUT;
        e->handler_wr = handler;
        e->arg_wr = arg;
        break;
    case NOTIFIER_ERROR:
        e->events |= EPOLLERR;
        e->handler_er = handler;
        e->arg_er = arg;
        break;
    }
}

void app_event_free(app_event_t ev)
{
    struct app_event *e = ev; 
    free(e);
}

struct app {
    int epfd;
    int event_max;
    int event_cnt;
    bool need_exit;
    bool need_scan_timer;
    Hashmap timer_map;
    heap timer_heap;
};

int app_add_event(app_t app, app_event_t ev)
{
    struct app *a = app; 
    struct app_event *e = ev; 
    struct epoll_event epv;
    int op;

    if (a->event_cnt + 1 > a->event_max) {
        pr_debug("event_cnt is %d now while event_max is %d\n", 
                  a->event_cnt, a->event_max);
        return -1;
    }

    epv.data.ptr = e;
    epv.events = e->events;
    if (ev->epolled) {
        op = EPOLL_CTL_MOD;
    } else {
        op = EPOLL_CTL_ADD;
        ev->epolled = true;
    }

    if(epoll_ctl(a->epfd, op, e->fd, &epv) < 0) {
        pr_debug("Event Add failed: fd = %d\n", e->fd);
        return -1;
    } 

    if (op == EPOLL_CTL_ADD)
        a->event_cnt++;

    pr_debug("Event Add Success: fd = %d, events = %x\n", e->fd, epv.events);
    return 0;
}

int app_del_event(app_t app, app_event_t ev)
{
    struct app *a = app; 
    struct app_event *e = ev; 

    if (e->epolled) {
        e->epolled = false;
        if(epoll_ctl(a->epfd, EPOLL_CTL_DEL, e->fd, NULL) < 0) {
            pr_debug("Event Del failed: fd = %d\n", e->fd);
            return -1;
        } 
        a->event_cnt--;
    }

    pr_debug("Event Del Success: fd = %d, events = %x\n", e->fd, e->events);
    return 0;
}

struct app_timer {
    struct timeval exp_tv, inter_tv;  /* 超时时间，间隔时间 */
    void (*efunc)(void*); /* 超时函数 */
    void *arg;      /* 超时函数参数 */
    bool one_shot;        /* one shot */
    bool active;
};

app_timer_t app_timer_create(int ms, bool os, void (*efunc)(void*), void *arg)
{
    struct app_timer *t = calloc(1, sizeof(struct app_timer));
    if (!t) {
		perror("app_timer_create");
		return NULL;
	}
    assert(ms > 0 && efunc != NULL);
    t->one_shot = os;
    t->inter_tv.tv_sec = ms / 1000;
    t->inter_tv.tv_usec = (ms % 1000) * 1000;
    t->active = false;
    t->efunc = efunc;
    t->arg = arg;

    return t;
}

void app_timer_del(app_timer_t timer)
{
    struct app_timer *t = timer; 
    free(t);
}

int app_add_timer(app_t app, app_timer_t timer)
{
    struct app *a = app; 
    struct app_timer *t = timer; 
    struct timeval now;

    gettimeofday(&now, NULL);
    timeradd(&now, &t->inter_tv, &t->exp_tv);
    t->active = true;
    hashmap_set(a->timer_map, (void*)&t->exp_tv, t);    /* 注册定时器 */
    heap_insert(&a->timer_heap, (void*)&t->exp_tv, t);
    a->need_scan_timer = true;
    return 0;
}

static void tag_entry_val_null(heap_entry* h, void *arg) {
    if (h->key == arg) 
        h->value = NULL;
}

void app_del_timer(app_t app, app_timer_t timer)
{
    struct app *a = app; 
    struct app_timer *t = timer; 
    t->active = false;
    hashmap_delete(a->timer_map, (void*)&t->exp_tv);
    heap_foreach_entry(&a->timer_heap, tag_entry_val_null, &t->exp_tv);
}

void app_scan_timers(app_t app)
{
    struct app *a = app; 
    struct app_timer *t;
    struct timeval *exp, now;

    //pr_debug("timers scan\n");
    gettimeofday(&now, NULL);
    while (1) {
        if (heap_min(&a->timer_heap, (void**)&exp, (void**)&t) == 0)  /* heap空 */
            break;
        //pr_debug("now: s.u = %lu.%lu\n", now.tv_sec, now.tv_usec);
        //pr_debug("exp: s.u = %lu.%lu\n", exp->tv_sec, exp->tv_usec);
        if (timercmp(&now, exp, <))   /* 当前时间<超时时间 */
            break;

        heap_delmin(&a->timer_heap, NULL, NULL);
        if (t != NULL && t->active) {
            t->efunc(t->arg); 
            if (!t->one_shot) {
                timeradd(&now, &t->inter_tv, &t->exp_tv);
                heap_insert(&a->timer_heap, (void*)&t->exp_tv, t);
                pr_debug("heap_insert, size = %d\n", heap_size(&a->timer_heap));
            }
        }
    }
    a->need_scan_timer = (heap_size(&a->timer_heap) > 0);

}

int compare_tv_keys(register void* key1, register void* key2) {
    struct timeval *key1_v = key1;
    struct timeval *key2_v = key2;
    int ret = 0;

    // Perform the comparison
    if (key1_v->tv_sec < key2_v->tv_sec) {
        ret =-1;
    } else if (key1_v->tv_sec> key2_v->tv_sec) {
        ret = 1;
    } else {
        if (key1_v->tv_usec < key2_v->tv_usec) {
            ret = -1;
        } else if (key1_v->tv_usec > key2_v->tv_usec) {
            ret = 1;
        } else {
            ret = 0;
        }
    }
    return ret;
}

app_t app_create(int event_max)
{
    struct app *a = calloc(1, sizeof(struct app));
    if (!a) {
		perror("app_create");
		return NULL;
	}

    if (event_max > 0)
        a->event_max = event_max;
    else
        a->event_max = DEF_MAX_EVENT;
    
    a->epfd = epoll_create(a->event_max);
    if (a->epfd == -1) {
        perror("epoll_create");
        goto err_mem;
    }

    heap_create(&a->timer_heap, 0, compare_tv_keys);
    a->timer_map = hashmap_create(18);
    a->need_scan_timer = false;
    return a;
err_mem:
    free(a);
    return NULL;
}

void app_free(app_t app)
{
    struct app *a = app; 
    close(a->epfd);
    hashmap_free(a->timer_map);
    heap_destroy(&a->timer_heap);
    free(a);
}

int app_exec(app_t app)
{
    struct app *a = app; 
    struct epoll_event events[a->event_max];
    struct app_event *e;
    uint32_t event;
    int i, fds;
    
    a->need_exit = false;
    while (!a->need_exit) {
        fds = epoll_wait(a->epfd, events, a->event_max, a->need_scan_timer ? 1:100);
        if(fds < 0) {
            perror("epoll_wait");
            return -1;
        }

        for(i = 0; i < fds; i++){
            e = (struct app_event*)events[i].data.ptr;
            event = events[i].events;
            if ((event & EPOLLIN) && (e->events & EPOLLIN)) 
                e->handler_rd(e->fd, e->arg_rd);

            if ((event & EPOLLOUT) && (e->events & EPOLLOUT)) 
                e->handler_wr(e->fd, e->arg_wr);

            if ((event & EPOLLERR) && (e->events & EPOLLERR)) 
                e->handler_er(e->fd, e->arg_er);
        } 
        app_scan_timers(a);
    }
    return 0;
}

void app_finish(app_t app)
{
    struct app *a = app; 
    a->need_exit = true;
}

#if 0
#include <cam/v4l2.h>


void img_proc(const void *p, int size, void *arg)
{
    static int i;
    char buf[32];
    FILE *fp;
    sprintf(buf, "tmp/%d.yuv", ++i);
    fp = fopen(buf, "w");

    fwrite(p, size, 1, fp);
    fclose(fp);
    fprintf(stdout, ".");
    fflush(stdout);
}

int main(int argc, char *argv[])
{
    app_t a = app_create(0);
    
    v4l2_dev_t v = v4l2_create(a, "/dev/video2", 0, 0);
    v4l2_set_img_proc(v, img_proc, v);
    v4l2_start_capture(v);

    app_exec(a);

    v4l2_stop_capture(v);
    v4l2_free(v);

    app_free(a);

    return 0;
}
#endif

