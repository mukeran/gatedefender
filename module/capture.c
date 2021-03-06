/**
 * capture.c - Use netfilter to capture network packet | 使用 netfilter 抓包
 * @author mukeran
 */
#include "capture.h"

#include <linux/net.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <linux/icmp.h>

#include "identify.h"

static struct nf_hook_ops nf_in, nf_out;

/**
 * Handle network packets which input from outside
 * 捕获外部进入网卡的包
 */
static unsigned int hook_network_in(const struct nf_hook_ops *ops,
                                    struct sk_buff *skb,
                                    const struct net_device *in,
                                    const struct net_device *out,
                                    int (*okfn)(struct sk_buff *)) {
  struct ethhdr *eth;
  struct iphdr *ip_header;
  struct icmphdr *icmp_header;
  struct tcphdr *tcp_header;
  struct udphdr *udp_header;
  struct timeval tv;
  do_gettimeofday(&tv);
  if (strcmp(in->name, "lo") == 0) // Ignore packets from local loopback
    return NF_ACCEPT;
  skb->tstamp = timeval_to_ktime(tv); // Set tstamp for identify_stream
  eth = (struct ethhdr *)skb_mac_header(skb);
  ip_header = (struct iphdr *)skb_network_header(skb);
  if (ip_header->protocol == IPPROTO_ICMP) {
    icmp_header = icmp_hdr(skb);
    return identify_icmp_packet(IN, skb, ip_header, icmp_header);
  } else if (ip_header->protocol == IPPROTO_TCP) {
    tcp_header = tcp_hdr(skb);
    return identify_tcp_packet(IN, skb, ip_header, tcp_header);
  } else if (ip_header->protocol == IPPROTO_UDP) {
    udp_header = udp_hdr(skb);
    return identify_udp_packet(IN, skb, ip_header, udp_header);
  }
  return NF_ACCEPT;
}

/**
 * Handle network packets which output from network card
 * 捕获离开网卡的包
 */
static unsigned int hook_network_out(const struct nf_hook_ops *ops,
                                    struct sk_buff *skb,
                                    const struct net_device *in,
                                    const struct net_device *out,
                                    int (*okfn)(struct sk_buff *)) {
  struct ethhdr *eth;
  struct iphdr *ip_header;
  struct icmphdr *icmp_header;
  struct tcphdr *tcp_header;
  struct udphdr *udp_header;
  struct timeval tv;
  do_gettimeofday(&tv);
  if (strcmp(out->name, "lo") == 0) // Ignore packets from local loopback
    return NF_ACCEPT;
  skb->tstamp = timeval_to_ktime(tv); // Set tstamp for identify_stream
  eth = (struct ethhdr *)skb_mac_header(skb);
  ip_header = (struct iphdr *)skb_network_header(skb);
  if (ip_header->protocol == IPPROTO_ICMP) {
    icmp_header = icmp_hdr(skb);
    return identify_icmp_packet(OUT, skb, ip_header, icmp_header);
  } else if (ip_header->protocol == IPPROTO_TCP) {
    tcp_header = tcp_hdr(skb);
    return identify_tcp_packet(OUT, skb, ip_header, tcp_header);
  } else if (ip_header->protocol == IPPROTO_UDP) {
    udp_header = udp_hdr(skb);
    return identify_udp_packet(OUT, skb, ip_header, udp_header);
  }
  return NF_ACCEPT;
}

/**
 * Register hook functions through nf_register_hook
 * 调用 nf_register_hook 注册钩子函数
 */
void register_capture(void) {
  nf_in.hook = (nf_hookfn *)hook_network_in;
  nf_in.hooknum = NF_INET_LOCAL_IN;
  nf_in.pf = PF_INET;
  nf_in.priority = NF_IP_PRI_FIRST;
  nf_register_hook(&nf_in);
  nf_out.hook = (nf_hookfn *)hook_network_out;
  nf_out.hooknum = NF_INET_LOCAL_OUT;
  nf_out.pf = PF_INET;
  nf_out.priority = NF_IP_PRI_FIRST;
  nf_register_hook(&nf_out);
}
