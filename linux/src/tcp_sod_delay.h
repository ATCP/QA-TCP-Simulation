/*
 * TCP SOD congestion control interface
 */
#ifndef __TCP_SOD_DELAY_H
#define __TCP_SOD_DELAY_H 1

/* SOD variables */
struct sod_delay {
	u32	beg_snd_nxt;	/* right edge during last RTT */
	u32	beg_snd_una;	/* left edge  during last RTT */
	u32	beg_snd_cwnd;	/* saves the size of the cwnd */
	u8	doing_sod_now;/* if true, do vegas for this RTT */
	u16	cntRTT;		/* # of RTTs measured within last RTT */
	u32	minQL;		/* min of RTTs measured within last RTT (in usec) */
	u64	minRTT;
	u32	curQL;
	u64	baseRTT;	/* the min of all Vegas RTT measurements seen (in usec) */
	u32	prev_QL;

};

extern void tcp_sod_delay_init(struct sock *sk);
extern void tcp_sod_delay_state(struct sock *sk, u8 ca_state);
extern void tcp_sod_delay_pkts_acked(struct sock *sk, u32 cnt, ktime_t last);
extern void tcp_sod_delay_cwnd_event(struct sock *sk, enum tcp_ca_event event);
extern void tcp_sod_delay_get_info(struct sock *sk, u32 ext, struct sk_buff *skb);

#endif	/* __TCP_SOD_DELAY_H */
