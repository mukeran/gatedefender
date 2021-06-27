#include "identify.h"

#include <linux/net.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>

#include "config.h"
#include "stream.h"

static u8 is_blocked_stream(struct stream* stream) {
  char buffer[30];
  if (stream == NULL)
    return 0;
  if (stream->type == DNS_TUNNELING_STREAM && strcmp(get_global_config(BLOCK_DNS_TUNNELING_STREAM), "1") == 0)
    return 1;
  else if (stream->type == ICMP_TUNNELING_STREAM && strcmp(get_global_config(BLOCK_ICMP_TUNNELING_STREAM), "1") == 0)
    return 1;
  sprintf(buffer, "block_stream_%d", stream->id);
  return strcmp(get_global_config(buffer), "1") == 0;
}

u32 identify_tcp_packet(u8 dir, struct sk_buff *skb, struct iphdr *ip_header, struct tcphdr *tcp_header) {
  u32 saddr = ip_header->saddr;
  u32 daddr = ip_header->daddr;
  u16 sport = tcp_header->source;
  u16 dport = tcp_header->dest;
  char *data = (char *)((char*)tcp_header + tcp_header->doff * 4);
  int length = ntohs(ip_header->tot_len) - ip_header->ihl * 4 - tcp_header->doff * 4;
  struct stream *stream;
  if (strcmp(get_global_config(BLOCK_TCP), "1") == 0) {
    printk(KERN_INFO "Blocked TCP packet!\n");
    return NF_DROP;
  }
  stream = identify_stream(dir, skb, IPPROTO_TCP, saddr, daddr, sport, dport, data, length);
  if (stream != NULL && is_blocked_stream(stream)) {
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
  struct stream *stream;
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
  stream = identify_stream(dir, skb, IPPROTO_UDP, saddr, daddr, sport, dport, data, ntohs(udp_header->len));
  if (stream != NULL && is_blocked_stream(stream)) {
    printk(KERN_INFO "Blocked stream %d!\n", stream->id);
    return NF_DROP;
  }
  return NF_ACCEPT;
}

u32 identify_icmp_packet(u8 dir, struct sk_buff *skb, struct iphdr *ip_header, struct icmphdr *icmp_header) {
  u32 saddr = ip_header->saddr;
  u32 daddr = ip_header->daddr;
  struct stream *stream;
  char *data = (char *)((char *)icmp_header + sizeof(struct icmphdr) + 8);
  int length = ntohs(ip_header->tot_len) - ip_header->ihl * 4 - (sizeof(struct icmphdr) + 8);
  if (strcmp(get_global_config(BLOCK_ICMP), "1") == 0) {
    printk(KERN_INFO "Blocked ICMP packet!\n");
    return NF_DROP;
  }
  stream = identify_stream(dir, skb, IPPROTO_ICMP, saddr, daddr, 0, 0, data, length);
  if (stream != NULL && is_blocked_stream(stream)) {
    printk(KERN_INFO "Blocked stream %d!\n", stream->id);
    return NF_DROP;
  }
  return NF_ACCEPT;
}
