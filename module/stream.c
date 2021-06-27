/**
 * stream.c - Stream related functions | 与流创建/更新/展示的函数都在此实现
 * @author mukeran
 */
#include "stream.h"

#include <linux/slab.h>

#include "dns.h"
#include "proc.h"
#include "utils.h"

#define STREAMS_CAPACITY_LEVEL 50
#define BUFFER_CAPACITY_LEVEL 1024

#define UDP_LOGICAL_STREAM_TIME_GAP 60000 /* 1 min, UDP 本身是无状态的，即不存在流这一说，但是许多应用传输流式数据依然采用 UDP。我们假设 1 分钟以内保持传输给同一目标的 UDP 包为一个逻辑流 */

struct stream **streams;
int stream_count;
int stream_capacity;

/**
 * Initialize global streams
 * 初始化全局流数组 streams
 */
void init_streams(void) {
  stream_capacity = STREAMS_CAPACITY_LEVEL;
  stream_count = 0;
  streams = (struct stream **)kzalloc(sizeof(struct stream *) * stream_capacity, GFP_KERNEL);
}

/**
 * Create a new stream instance
 * 创建一个新的 stream 实例，包括对各个 field 的初始化
 */
static stream_t* alloc_stream(void) {
  int id;
  if (stream_count >= stream_capacity) {
    stream_capacity += STREAMS_CAPACITY_LEVEL;
    streams = (struct stream **)krealloc(streams, sizeof(struct stream *) * stream_capacity, GFP_KERNEL | __GFP_ZERO);
  }
  id = stream_count;
  streams[id] = (struct stream *)kzalloc(sizeof(struct stream), GFP_ATOMIC);
  streams[id]->id = id;
  ++stream_count;
  return streams[id];
}

/**
 * Append data to stream's data link list
 * 向指定流的数据包链表中添加新数据包（链表操作）
 */
static void append_data(struct stream *stream, const char *data, int length, u8 dir, u64 timestamp) {
  struct stream_data_list *entry = (struct stream_data_list *)kzalloc(sizeof(struct stream_data_list), GFP_ATOMIC);
  entry->data = memdup(data, length);
  entry->length = length;
  entry->timestamp = timestamp;
  entry->dir = dir;
  if (stream->list != NULL) {
    stream->list->prev->next = entry;
    entry->prev = stream->list->prev;
    stream->list->prev = entry;
    entry->next = stream->list;
  } else {
    entry->prev = entry;
    entry->next = entry;
    stream->list = entry;
  }
}

/**
 * Identify and create/update stream
 * 识别并创建/更新流
 */
stream_t *identify_stream(u8 dir, struct sk_buff *skb, int type, int saddr, int daddr, int sport, int dport, const char *data, int data_length) {
  int i;
  struct tcphdr *tcp_header;
  struct icmphdr *icmp_header;
  struct stream *stream = NULL;
  u64 timestamp = ktime_to_ms(skb->tstamp);
  if (dir == OUT && (type == IPPROTO_TCP || type == IPPROTO_UDP || type == IPPROTO_ICMP || type == IPPROTO_RAW)) {
    swap(saddr, daddr);
    swap(sport, dport);
  }
  /* Update stream */
  for (i = 0; i < stream_count; ++i) {
    if (!streams[i]->ended && streams[i]->addr1 == saddr && streams[i]->addr2 == daddr) {
      /* 对应 TCP 流 */
      if (type == IPPROTO_TCP && streams[i]->type == TCP_STREAM && streams[i]->port1 == sport && streams[i]->port2 == dport) {
        tcp_header = tcp_hdr(skb);
        append_data(streams[i], data, data_length, dir, timestamp);
        if (tcp_header->fin && dir == OUT)
          streams[i]->ended = 1;
        return streams[i];
      /* 对应逻辑上的 UDP 流 */
      } else if (type == IPPROTO_UDP && streams[i]->type == UDP_LOGICAL_STREAM && streams[i]->port1 == sport && (timestamp - streams[i]->list->prev->timestamp <= UDP_LOGICAL_STREAM_TIME_GAP)) {
        append_data(streams[i], data, data_length, dir, timestamp);
        return streams[i];
      /* 对应 DNS Tunneling 流 */
      } else if (type == IPPROTO_UDP && (streams[i]->type == DNS_LOGICAL_STREAM || streams[i]->type == DNS_TUNNELING_STREAM) && ntohs(sport) == 53 && (timestamp - streams[i]->list->prev->timestamp <= UDP_LOGICAL_STREAM_TIME_GAP) && check_dns_sld(streams[i], data, data_length)) {
        /* Change from DNS_LOGICAL_STREAM to DNS_TUNNELING_STREAM if check_dns_request_tunneling returns true */
        if (streams[i]->type == DNS_LOGICAL_STREAM && dir == OUT && check_dns_request_tunneling(streams[i], data, data_length))
          streams[i]->type = DNS_TUNNELING_STREAM;
        append_data(streams[i], data, data_length, dir, timestamp);
        return streams[i];
      /* 对应 ICMP echo-reply 序列 */
      } else if (type == IPPROTO_ICMP && streams[i]->type == ICMP_SEQUENCE) {
        icmp_header = icmp_hdr(skb);
        if (icmp_header->type == ICMP_ECHO || icmp_header->type == ICMP_ECHOREPLY) {
          /* Rule: Ignore system ping random ICMP echo data header 8 bytes, and check if the rest of the data is the same as previous packets (when data length is less than 32, just check all of the data (Linux ping is 48 bytes long)) */
          if (streams[i]->meta.icmp.code != icmp_header->code || streams[i]->list->length != data_length || (data_length >= 32 && memcmp(streams[i]->list->data + 8, data + 8, data_length - 8) != 0) || (data_length < 32 && memcmp(streams[i]->list->data, data, data_length) != 0)) {
              /* Change type to icmp tunneling stream */
              streams[i]->type = ICMP_TUNNELING_STREAM;
              append_data(streams[i], data, data_length, dir, timestamp);
              return streams[i];
          } else if ((icmp_header->type == ICMP_ECHO && streams[i]->meta.icmp.sequence + 1 == ntohs(icmp_header->un.echo.sequence)) || (icmp_header->type == ICMP_ECHOREPLY && streams[i]->meta.icmp.sequence == ntohs(icmp_header->un.echo.sequence))) {
            append_data(streams[i], data, data_length, dir, timestamp);
            streams[i]->meta.icmp.sequence = (icmp_header->type == ICMP_ECHO) ? (streams[i]->meta.icmp.sequence + 1) : streams[i]->meta.icmp.sequence;
            return streams[i];
          }
        }
      /* 对应 ICMP Tunneling 流 */
      } else if (type == IPPROTO_ICMP && streams[i]->type == ICMP_TUNNELING_STREAM) {
        append_data(streams[i], data, data_length, dir, timestamp);
        return streams[i];
      }
    }
  }
  /* Create stream */
  if (type == IPPROTO_TCP) {
    tcp_header = tcp_hdr(skb);
    if (!tcp_header->fin && !tcp_header->rst) {
      stream = alloc_stream();
      stream->addr1 = saddr; stream->addr2 = daddr;
      stream->port1 = sport; stream->port2 = dport;
      stream->inbound = dir;
      stream->type = TCP_STREAM;
      append_data(stream, data, data_length, dir, timestamp);
    }
  } else if (type == IPPROTO_UDP) {
    if (dir == OUT) {
      stream = alloc_stream();
      stream->addr1 = saddr; stream->addr2 = daddr;
      stream->port1 = sport; stream->port2 = dport;
      stream->inbound = dir;
      if (ntohs(sport) == 53) {
        stream->type = DNS_LOGICAL_STREAM;
        stream->meta.dns.hl = alloc_hash_list();
        if (dir == OUT)
          check_dns_request_tunneling(streams[i], data, data_length);
      } else
        stream->type = UDP_LOGICAL_STREAM;
      append_data(stream, data, data_length, dir, timestamp);
    }
  } else if (type == IPPROTO_ICMP) {
    icmp_header = icmp_hdr(skb);
    if (icmp_header->code == ICMP_ECHO || icmp_header->code == ICMP_ECHOREPLY) {
      stream = alloc_stream();
      stream->addr1 = saddr; stream->addr2 = daddr;
      stream->inbound = dir;
      stream->type = ICMP_SEQUENCE;
      stream->meta.icmp.sequence = ntohs(icmp_header->un.echo.sequence);
      append_data(stream, data, data_length, dir, timestamp);
    }
  }
  return stream;
}

/**
 * Mapping stream type id to string
 * 将流类型的 id 转换为显示字符串
 */
static inline const char *get_stream_string(int type) {
  if (type == TCP_STREAM)
    return "TCP_STREAM";
  else if (type == UDP_LOGICAL_STREAM)
    return "UDP_LOGICAL_STREAM";
  else if (type == ICMP_SEQUENCE)
    return "ICMP_SEQUENCE";
  else if (type == DNS_LOGICAL_STREAM)
    return "DNS_LOGICAL_STREAM";
  else if (type == DNS_TUNNELING_STREAM)
    return "DNS_TUNNELING_STREAM";
  else if (type == ICMP_TUNNELING_STREAM)
    return "ICMP_TUNNELING_STREAM";
  return "UNKNOWN_STREAM";
}

/**
 * Convert streams info to readable string
 * 将所有 streams 信息转换为 proc 中 streams 打印的 MD 表格形式
 */
int display_streams(char **_buffer) {
  int i, capacity = BUFFER_CAPACITY_LEVEL, length = 0, append_length;
  char *buffer, *temp_buffer = (char *)kzalloc(sizeof(char) * BUFFER_CAPACITY_LEVEL, GFP_KERNEL);
  buffer = (char *)kzalloc(sizeof(char) * capacity, GFP_KERNEL);
  length = sprintf(buffer,
    "|Stream ID|Stream Type|End1|End2|First Seen|Last Seen|Is Ended|\n"
    "|---|---|---|---|---|---|---|\n");
  for (i = 0; i < stream_count; ++i) {
    append_length = sprintf(temp_buffer,
    "|%d|%s|%pI4:%hu|%pI4:%hu|%lld|%lld|%hu|\n",
    streams[i]->id, get_stream_string(streams[i]->type), &streams[i]->addr1, ntohs(streams[i]->port1), &streams[i]->addr2, ntohs(streams[i]->port2), streams[i]->list->timestamp, streams[i]->list->prev->timestamp, streams[i]->ended);
    if (length + append_length >= capacity) {
      capacity += BUFFER_CAPACITY_LEVEL;
      buffer = (char *)krealloc(buffer, sizeof(char) * capacity, GFP_KERNEL | __GFP_ZERO);
    }
    memcpy(buffer + length, temp_buffer, append_length);
    length += append_length;
    buffer[length] = '\0';
  }
  *_buffer = buffer;
  kfree(temp_buffer);
  return length;
}

/**
 * Mapping direction int to string
 * 将 dir IN/OUT 转换为字符串
 */
static inline const char* get_direction_string(u8 dir) {
  if (dir == IN)
    return "in";
  return "out";
}

/**
 * Format stream data to proc output form
 * 将流的数据包转换为 /proc/gatedefender/stream/ 中打印的形式
 */
int get_stream_data(int id, char **_buffer) {
  int capacity = BUFFER_CAPACITY_LEVEL, length = 0, append_length, total_length;
  char *buffer, *temp_buffer = (char *)kzalloc(sizeof(char) * BUFFER_CAPACITY_LEVEL, GFP_KERNEL);
  struct stream *stream = streams[id];
  struct stream_data_list *cur = stream->list;
  if (cur == NULL) {
    kfree(temp_buffer);
    return 0;
  }
  buffer = (char *)kzalloc(sizeof(char) * capacity, GFP_KERNEL);
  do {
    if (cur->length == 0) {
      cur = cur->next;
      continue;
    }
    append_length = sprintf(temp_buffer, "direction %s, length %d, time %lld\n-----data begin-----\n", get_direction_string(cur->dir), cur->length, cur->timestamp);
    total_length = length + append_length + cur->length + 21 /* Length of "\n-----data end-----\n" */;
    if (total_length >= capacity) {
      capacity = ((total_length - 1) / BUFFER_CAPACITY_LEVEL + 1) * BUFFER_CAPACITY_LEVEL;
      buffer = (char *)krealloc(buffer, sizeof(char) * capacity, GFP_KERNEL | __GFP_ZERO);
    }
    memcpy(buffer + length, temp_buffer, append_length);
    memcpy(buffer + length + append_length, cur->data, cur->length);
    memcpy(buffer + length + append_length + cur->length, "\n-----data end-----\n", 21);
    length = total_length;
    cur = cur->next;
  } while (cur != stream->list);
  kfree(temp_buffer);
  *_buffer = buffer;
  return length;
}
