/* Modified Linux module source code from /home/weixl/linux-2.6.22.6 */
#define NS_PROTOCOL "tcp_sod.c"
#include "../ns-linux-c.h"
#include "../ns-linux-util.h"
/*
 * Queue length Adaptive TCP congestion control
 */


#include "tcp_sod.h"
/*

*/
#define V_PARAM_SHIFT 5

//static int target_qs = 3<<V_PARAM_SHIFT;
static int init_cwnd = 150;
static int init_cwnd_on = 1;

//module_param(target_qs, int, 0644);
//MODULE_PARM_DESC(target_qs, "Target Queue Length");
module_param(init_cwnd, int, 0644);
MODULE_PARM_DESC(init_cwnd, "Initial congestion window size");
module_param(init_cwnd_on, int, 0644);
MODULE_PARM_DESC(init_cwnd_on, "Initial congestion window on");

int bw_dev = 0;

static void sod_enable(struct sock *sk)
{
	const struct tcp_sock *tp = tcp_sk(sk);
	struct sod *sod = inet_csk_ca(sk);

	sod->doing_sod_now = 1;
	
	
	sod->currentQueueLen = 0x7fffffff;       
	sod->baseRTT = 0x7fffffff;
	sod->currentQueueLen = 0x7fffffff;
	//sod->minRTT = 0x7fffffff;
	//sod->cntRTT = 0;
        sod->targetQueueLen = 10;
        sod->update_period = 0.002;
        sod->estimate_period = 1;        
        sod->is_1st_ack_rcv = 0; 
        sod->start_seq = sk->snd_nxt;
      
        sod->start_time = 0;
        sod->output = fopen("sod-output", "w");
        
        cleanWindow(&sod->bwWindow);
                
}

static inline void sod_disable(struct sock *sk)
{
	struct sod *sod = inet_csk_ca(sk);

	sod->doing_sod_now = 0;
        
}

void tcp_sod_init(struct sock *sk)
{
	struct tcp_sock *tp = tcp_sk(sk);
	struct sod *sod = inet_csk_ca(sk);
                               
        
        if (init_cwnd_on != 0)
        {
            tp->snd_cwnd = init_cwnd;
            printf("initial congestion window: %lu %d\n", tp->snd_cwnd, init_cwnd_on);
        }        
                
        initSlideWindow(&sod->bwWindow, 1024);
	sod_enable(sk);
                
}
EXPORT_SYMBOL_GPL(tcp_sod_init);

void tcp_sod_pkts_acked(struct sock *sk, u32 cnt, ktime_t last)
{
        
    struct tcp_sock *tp = tcp_sk(sk);     
    struct sod *sod = inet_csk_ca(sk);
    
    u64 vrtt;          	
   
    double now = sk->current_time;   
    
    /*
    if (ktime_equal(last, net_invalid_timestamp()))    
    {
        printf("what\n");
        return;        
    }
    */
    
    /* Never allow zero rtt or baseRTT */    
    vrtt = ktime_to_us(net_timedelta(last)) + 1;
           
    /* Find the minimum propagation delay: */
    if (vrtt < sod->baseRTT)
            sod->baseRTT = vrtt;                
    
    if (!sod->is_1st_ack_rcv && sod->doing_sod_now)
    {
        if (sk->snd_una > sod->start_seq)
        {
            sod->is_1st_ack_rcv = 1;       
        
            sod->start_time = now;
            sk->sod_start = 1;
            //sk->sod_diff -= (long)cnt;
                              
            put(&sod->bwWindow, now, cnt); 
            
        }
    }
    else if (sod->is_1st_ack_rcv && sod->doing_sod_now)
    {
        sk->sod_diff -= (long)cnt;
        put(&sod->bwWindow, now, cnt); 
    }   
    
    //sod->minRTT = min(sod->minRTT, vrtt);
    //sod->cntRTT++;
   
            
}
EXPORT_SYMBOL_GPL(tcp_sod_pkts_acked);

void tcp_sod_state(struct sock *sk, u8 ca_state)
{

	if (ca_state == TCP_CA_Open)
		sod_enable(sk);
	else
        {
		sod_disable(sk);              
        }
}
EXPORT_SYMBOL_GPL(tcp_sod_state);

/*
 * If the connection is idle and we are restarting,
 * then we don't want to do any Vegas calculations
 * until we get fresh RTT samples.  So when we
 * restart, we reset our Vegas state to a clean
 * slate. After we get acks for this flight of
 * packets, _then_ we can make Vegas calculations
 * again.
 */
void tcp_sod_cwnd_event(struct sock *sk, enum tcp_ca_event event)
{
	if (event == CA_EVENT_CWND_RESTART ||
	    event == CA_EVENT_TX_START)
		tcp_sod_init(sk);
}
EXPORT_SYMBOL_GPL(tcp_sod_cwnd_event);

static void tcp_sod_cong_avoid(struct sock *sk, u32 ack,
				 u32 seq_rtt, u32 in_flight, int flag)
{
        
        struct tcp_sock *tp = tcp_sk(sk);
	struct sod *sod = inet_csk_ca(sk);
	//u32 diff;	

	if (!sod->doing_sod_now)
		return tcp_reno_cong_avoid(sk, ack, seq_rtt, in_flight, flag);
       
	/* limited by applications */
	//if (!tcp_is_cwnd_limited(sk, in_flight))
	//	return;
        
                
        double now = tp->current_time;
        
        //printf("%lf %lf %u\n", now, sod->start_time, ack);
        
	if (now - sod->start_time >= sod->update_period)
        {   
          
            if (timeInterval(&sod->bwWindow, now) >= sod->estimate_period)//(double)sod->baseRTT/(double)1000000 + sk->ack_var)
            {                
                //sod->thruput = sod->bwWindow._total / timeInterval(&sod->bwWindow, now);               
                //printf("%u\n", tp->snd_cwnd);
                sod->estimatedBandwidth = sod->bwWindow._total / timeInterval(&sod->bwWindow, now); 
                sod->currentQueueLen = init_cwnd - (sod->estimatedBandwidth + bw_dev)* ((double)sod->baseRTT/(double)1000000 + sk->ack_var) + sk->sod_diff;
                timeShift(&sod->bwWindow, now, sod->estimate_period);//(double)sod->baseRTT/(double)1000000 + sk->ack_var);
                tp->snd_cwnd = ((int32_t)tp->snd_cwnd <= sod->currentQueueLen - sod->targetQueueLen ? 0 : tp->snd_cwnd - (sod->currentQueueLen - sod->targetQueueLen));
               
                printf("1 %lf %lf %u %u %ld %u %ld %lf %lf %lf %d\n", now, sod->start_time, ack, tp->snd_cwnd, sod->currentQueueLen, sod->targetQueueLen, sk->sod_diff, sod->estimatedBandwidth, sod->estimatedBandwidth * ((double)sod->baseRTT/(double)1000000 + sk->ack_var), ((double)sod->baseRTT/(double)1000000), sk->sk_state);
                 
                if (tp->snd_cwnd > 160)
                    exit(-1);
                fprintf(sod->output, "%lf %lf %u %u %ld %u %ld %lf %lf\n", now, sod->start_time, ack, tp->snd_cwnd, sod->currentQueueLen, sod->targetQueueLen, sk->sod_diff, sod->estimatedBandwidth, sod->estimatedBandwidth * ((double)sod->baseRTT/(double)1000000 + sk->ack_var));
                
            } 
            else
            {                
                
                if (!timeInterval(&sod->bwWindow, now))
                {
                    sod->estimatedBandwidth = 0;
                    sod->currentQueueLen = sod->targetQueueLen;
                }
                else
                {
                    sod->estimatedBandwidth = sod->bwWindow._total / timeInterval(&sod->bwWindow, now);    
                    sod->currentQueueLen = init_cwnd - (sod->estimatedBandwidth + bw_dev)* ((double)sod->baseRTT/(double)1000000 + sk->ack_var) + sk->sod_diff;
                }
                
                //sod->thruput = sod->estimatedBandwidth;
                tp->snd_cwnd = ((int32_t)tp->snd_cwnd <= sod->currentQueueLen - sod->targetQueueLen ? 0 : tp->snd_cwnd - (sod->currentQueueLen - sod->targetQueueLen));
                
                printf("2. %lf %lf %u %u %ld %u %ld %lf %lf %lf %d\n", now, sod->start_time, ack, tp->snd_cwnd, sod->currentQueueLen, sod->targetQueueLen, sk->sod_diff, sod->estimatedBandwidth, sod->estimatedBandwidth * ((double)sod->baseRTT/(double)1000000 + sk->ack_var), ((double)sod->baseRTT/(double)1000000), sk->sk_state);
                
                fprintf(sod->output, "%lf %lf %u %u %ld %u %ld %lf %lf\n", now, sod->start_time, ack, tp->snd_cwnd, sod->currentQueueLen, sod->targetQueueLen, sk->sod_diff, sod->estimatedBandwidth, sod->estimatedBandwidth * ((double)sod->baseRTT/(double)1000000 + sk->ack_var));
                
            }
                                                           
            sod->start_time = now;
                        
        }
			                        
        if (tp->snd_cwnd < 2)
                tp->snd_cwnd = 2;
        else if (tp->snd_cwnd > tp->snd_cwnd_clamp)
                tp->snd_cwnd = tp->snd_cwnd_clamp;
	
	//sod->minRTT = 0x7fffffff;
                       
                
}


/* Extract info for Tcp socket info provided via netlink. */
void tcp_sod_get_info(struct sock *sk, u32 ext, struct sk_buff *skb)
{
/*
	const struct vegas *ca = inet_csk_ca(sk);
	if (ext & (1 << (INET_DIAG_VEGASINFO - 1))) {
		struct tcpvegas_info info = {
			.tcpv_enabled = ca->doing_vegas_now,
			.tcpv_rttcnt = ca->cntRTT,
			.tcpv_rtt = ca->baseRTT,
			.tcpv_minrtt = ca->minRTT,
		};

		nla_put(skb, INET_DIAG_VEGASINFO, sizeof(info), &info);
	}
*/
}
EXPORT_SYMBOL_GPL(tcp_sod_get_info);

/* SOD MD phase */
static u32 tcp_sod_ssthresh(struct sock *sk)
{
	const struct tcp_sock *tp = tcp_sk(sk);
	struct sod *sod = inet_csk_ca(sk);
        
        return tp->snd_cwnd;

}
EXPORT_SYMBOL_GPL(tcp_sod_ssthresh);

static struct tcp_congestion_ops tcp_sod = {
	.flags		= TCP_CONG_RTT_STAMP,
	.init		= tcp_sod_init,
	.ssthresh	= tcp_sod_ssthresh,
	.cong_avoid	= tcp_sod_cong_avoid,
	.pkts_acked	= tcp_sod_pkts_acked,
	.set_state	= tcp_sod_state,
	.cwnd_event	= tcp_sod_cwnd_event,
	.get_info	= tcp_sod_get_info,

	.owner		= THIS_MODULE,
	.name		= "sod",
};

static int __init tcp_sod_register(void)
{
	BUILD_BUG_ON(sizeof(struct sod) > ICSK_CA_PRIV_SIZE);
	tcp_register_congestion_control(&tcp_sod);
	return 0;
}

static void __exit tcp_sod_unregister(void)
{
	tcp_unregister_congestion_control(&tcp_sod);
}

module_init(tcp_sod_register);
module_exit(tcp_sod_unregister);

MODULE_AUTHOR("LIU Ke, Michael");
MODULE_LICENSE("CUHK");
MODULE_DESCRIPTION("QA-TCP");
#undef NS_PROTOCOL
