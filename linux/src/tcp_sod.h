/*
 * TCP SOD congestion control interface
 */
#ifndef __TCP_SOD_H
#define __TCP_SOD_H 1

/* SOD variables */

struct Packet
{
    double time;
    u32 len;
};


typedef struct 
{
    struct Packet *window;
    int64_t _size, capacity;
}slideWindow;  
    
void initSlideWindow(slideWindow *sw, u_int size)
{
    sw->capacity = size;
    sw->_size = 0;
    sw->window = (struct Packet *)malloc(sizeof(struct Packet)*size);
    int i;
    for (i = 0; i < sw->capacity; i ++)
    {
        sw->window[i].time = sw->window[i].len = 0;
    }
}

void delSlideWindow(slideWindow *sw)
{
    free(sw->window);
}

int isEmpty(slideWindow *sw)
{
    if (!sw->_size)
        return 1;
    else
        return 0;
}

u_int size(slideWindow *sw)
{
    return sw->_size;
}

double timeInterval(slideWindow *sw, double current_time)
{
    if (isEmpty(sw))
        return 0;
    else
        return (current_time <= sw->window[0].time ? 0 : current_time - sw->window[0].time);
}

void shift(slideWindow *sw)
{
    int i = 0;
    for (i = 0; i < sw->_size - 1; i ++)
    {
        sw->window[i].time = sw->window[i+1].time;
    }
}

void timeShift(slideWindow *sw, double current_time, double thresh)
{
    while (timeInterval(sw, current_time) > thresh)
    {
        shift(sw);
        sw->_size --;
    }
}

void put(slideWindow *sw, double time)
{
    if (sw->_size < sw->capacity)
    {
        sw->window[sw->_size].time = time;
        sw->_size ++;
    }
    else
    {
        shift(sw);
        sw->window[sw->_size-1].time = time;
    }
}


struct sod 
{
    u32	   beg_snd_nxt;         /* right edge during last RTT */
    u32	   beg_snd_una;         /* left edge  during last RTT */
    u32	   beg_snd_cwnd;	/* saves the size of the cwnd */
    u8	   doing_sod_now;       /* if true, do vegas for this RTT */
    u16	   cntRTT;		/* # of RTTs measured within last RTT */
    int64_t    currentQueueLen;     /* min of RTTs measured within last RTT (in usec) */
    u64	   minRTT;              /* min of RTTs measured within last RTT (in usec) */
    int64_t	   targetQueueLen;
    u64	   baseRTT;             /* the min of all Vegas RTT measurements seen (in usec) */
    double    estimatedBandwidth;  /**/
    int    is_1st_ack_rcv;
    double    start_time;
    double    update_period;
    slideWindow bwWindow;    
    FILE* output;

};

extern void tcp_sod_init(struct sock *sk);
extern void tcp_sod_state(struct sock *sk, u8 ca_state);
extern void tcp_sod_pkts_acked(struct sock *sk, u32 cnt, ktime_t last);
extern void tcp_sod_cwnd_event(struct sock *sk, enum tcp_ca_event event);
extern void tcp_sod_get_info(struct sock *sk, u32 ext, struct sk_buff *skb);

#endif	/* __TCP_SOD_H */
