#include "identify.h"

#include <linux/net.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>

#include "config.h"
#include "stream.h"

u32 identify_tcp_packet(u8 dir, struct sk_buff *skb, struct iphdr *ip_header, struct tcphdr *tcp_header) {
  u32 saddr = ip_header->saddr;
  u32 daddr = ip_header->daddr;
  u16 sport = tcp_header->source;
  u16 dport = tcp_header->dest;
  char *data = (char *)((char*)tcp_header + tcp_header->doff);
  int length = ip_header->tot_len - ip_header->ihl * 4 - tcp_header->doff * 4;
  struct stream *stream;
  if (strcmp(get_global_config(BLOCK_TCP), "1") == 0) {
    printk(KERN_INFO "Blocked TCP packet!\n");
    return NF_DROP;
  }
  stream = identify_stream(dir, skb, IPPROTO_TCP, saddr, daddr, sport, dport, data, length);
  if (is_blocked_stream(stream->id)) {
    printk(KERN_INFO "Blocked stream %d!\n", stream->id);
    return NF_DROP;
  }
  return NF_ACCEPT;
}

u32 identify_udp_packet(u8 dir, struct sk_buff *skb, struct iphdr *ip_header, struct udphdr *udp_header) {
  u32 saddr = ip_header->saddr;
  u32 daddr = ip_header->daddr;
  u16 sport = udp_header->source;
  u16 dport = udp_header->dest;
  char *data = (char *)((char *)udp_header + 8);
  if (strcmp(get_global_config(BLOCK_UDP), "1") == 0) {
    printk(KERN_INFO "Blocked UDP packet!\n");
    return NF_DROP;
  }
  if (dport == htons(53)) {
    // identity DNS packet
    if (strcmp(get_global_config(BLOCK_DNS), "1") == 0) {
      printk(KERN_INFO "Blocked DNS packet!\n");
      return NF_DROP;
    }
  }
  return NF_ACCEPT;
}

u32 identify_icmp_packet(u8 dir, struct sk_buff *skb, struct iphdr *ip_header, struct icmphdr *icmp_header) {
  u32 saddr = ip_header->saddr;
  u32 daddr = ip_header->daddr;
  u8 type = icmp_header->type;
  u8 code = icmp_header->code;
  if (strcmp(get_global_config(BLOCK_ICMP), "1") == 0) {
    printk(KERN_INFO "Blocked ICMP packet!\n");
    return NF_DROP;
  }
  return NF_ACCEPT;
}
