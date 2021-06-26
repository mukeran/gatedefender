#ifndef _MODULE_IDENTIFY_H
#define _MODULE_IDENTIFY_H

#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <linux/icmp.h>

#define OUT 0
#define IN 1

u32 identify_tcp_packet(u8 dir, struct sk_buff *skb, struct iphdr *ip_header, struct tcphdr *tcp_header);
u32 identify_udp_packet(u8 dir, struct sk_buff *skb, struct iphdr *ip_header, struct udphdr *udp_header);
u32 identify_icmp_packet(u8 dir, struct sk_buff *skb, struct iphdr *ip_header, struct icmphdr *icmp_header);

#endif
