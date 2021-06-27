/**
 * dns.c - Function to check or parse DNS packet | 这部分代码实现了对 DNS 包中域名的解析/判定是否为 DNS Tunneling 函数
 * @author mukeran
 */
#include "dns.h"

#include <linux/slab.h>

#include "stream.h"
#include "utils.h"

/**
 * Parse domain name from DNS packet
 * 从 DNS 包中获得查询的域名
 */
static char** parse_dns_host(char *host, int *_count) {
  int i = 0, count = 0;
  u8 fragment;
  char **parts;
  do {
    fragment = host[i];
    i += fragment + 1;
    ++count;
  } while (host[i] != 0);
  parts = (char **)kzalloc(sizeof(char *) * count, GFP_ATOMIC);
  *_count = count;
  i = 0;
  do {
    fragment = host[i];
    parts[count - 1] = (char *)kzalloc(sizeof(char) * fragment, GFP_ATOMIC);
    memcpy(parts[count - 1], host + i + 1, fragment);
    i += fragment + 1;
    --count;
  } while (host[i] != 0);
  return parts;
}

/**
 * Use dot to join domain name parts
 * 将从 DNS 包中得到的几部分域名用“.”连接
 */
static inline void join_host_parts(char *target, char **start, char **end) {
  int length = 0, slen;
  char **cur = start;
  while (cur != end) {
    if (cur != start) {
      target[length] = '.';
      ++length;
    }
    slen = strlen(*cur);
    memcpy(target + length, *cur, slen);
    length += slen;
    ++cur;
  }
  target[length] = 0;
}

/**
 * Garbage collect function for DNS parts
 * DNS parts 用到的 char ** 的释放函数
 */
static inline void gc_dns_parts(char **parts, int count) {
  int i;
  for (i = 0; i < count; ++i)
    if (parts[i])
      kfree(parts[i]);
  kfree(parts);
}

/**
 * Hash domain name string
 * 哈希域名字符串
 */
static int hash_host(char *host) {
  int i, len = strlen(host), hash = 0;
  for (i = 0; i < len; ++i)
    hash = (hash * 133 + host[i]) % HASH_LIST_SIZE;
  return hash;
}

#define DNS_TUNNELING_RANDOM_HOST_THRESHOLD 20

/**
 * Check DNS SLD (test.mukeran.com -> mukeran.com) if exists in hash list
 * 检查二级域名是否存在于哈希表中
 */
u8 check_dns_sld(struct stream *stream, const char *data, int data_length) {
  int i, j, prev_total = 12, count, hash;
  u16 questions;
  char host[256], **parts, sld[256];
  struct hash_list *hl;
  if (data_length < 12)
    return 0;
  memcpy(&questions, data + 4, 4);
  questions = ntohs(questions);
  for (i = 0; i < questions; ++i) {
    for (j = prev_total; j < data_length && data[j] != 0; ++j);
    if (j >= data_length)
      return 0;
    memcpy(host, data + prev_total, j - prev_total + 1);
    prev_total += j - prev_total + 5;
    parts = parse_dns_host(host, &count);
    if (count == 1)
      continue;
    sprintf(sld, "%s.%s", parts[0], parts[1]);
    hash = hash_host(sld);
    if ((hl = check_hash(stream->meta.dns.hl, hash)))
      return 1;
    gc_dns_parts(parts, count);
  }
  return 0;
}

/**
 * Judge if DNS request is a packet of DNS tunneling
 * 判断 DNS 请求包是否为可能的 DNS Tunneling 包
 */
u8 check_dns_request_tunneling(struct stream *stream, const char *data, int data_length) {
  int i, j, prev_total = 12, count, hash;
  u16 questions;
  char host[256], **parts, sld[256];
  struct hash_list *hl;
  if (data_length < 12)
    return 0;
  memcpy(&questions, data + 4, 4);
  questions = ntohs(questions);
  for (i = 0; i < questions; ++i) {
    for (j = prev_total; j < data_length && data[j] != 0; ++j);
    if (j >= data_length)
      return 0;
    memcpy(host, data + prev_total, j - prev_total + 1);
    prev_total += j - prev_total + 5;
    parts = parse_dns_host(host, &count);
    if (count == 1)
      continue;
    sprintf(sld, "%s.%s", parts[0], parts[1]);
    hash = hash_host(sld);
    if ((hl = check_hash(stream->meta.dns.hl, hash))) {
      join_host_parts(sld, parts + 2, parts + count);
      hash = hash_host(sld);
      if (!check_hash(hl, hash)) {
        insert_hash(hl, hash);
        /* Rule: If a SLD has been queried more than DNS_TUNNELING_RANDOM_HOST_THRESHOLD different subdomain in a very short time, its DNS query might be a DNS Tunneling session */
        if (hl->count >= DNS_TUNNELING_RANDOM_HOST_THRESHOLD)
          return 1;
      }
    } else {
      hl = insert_hash(stream->meta.dns.hl, hash);
      join_host_parts(sld, parts + 2, parts + count);
      hash = hash_host(sld);
      insert_hash(hl, hash);
    }
    gc_dns_parts(parts, count);
  }
  return 0;
}
