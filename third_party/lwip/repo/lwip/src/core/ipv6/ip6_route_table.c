/**
 * @file
 *
 * IPv6 static route table.
 */

/*
 * Copyright (c) 2020 Project CHIP Authors
 * Copyright (c) 2015 Nest Labs, Inc.
 */

#include "lwip/opt.h"

#if LWIP_IPV6  /* don't build if not configured for use in lwipopts.h */

#include "lwip/def.h"
#include "lwip/mem.h"
#include "lwip/netif.h"
#include "lwip/ip6.h"
#include "lwip/ip6_route_table.h"
#include "lwip/ip6_addr.h"
#include "lwip/nd6.h"
#include "lwip/debug.h"
#include "lwip/stats.h"

#include "string.h"

#if LWIP_IPV6_ROUTE_TABLE_SUPPORT

static struct ip6_route_entry static_route_table[LWIP_IPV6_NUM_ROUTE_ENTRIES];

/**
 * Add the ip6 prefix route and target netif into the static route table while
 * keeping all entries sorted in decreasing order of prefix length.
 * 1. Search from the last entry up to find the correct slot to insert while
 *    moving entries one position down to create room.
 * 2. Insert into empty slot created.
 *
 * Subsequently, a linear search down the list can be performed to retrieve a
 * matching route entry for a Longest Prefix Match.
 *
 * @param ip6_prefix the route prefix entry to add.
 * @param netif pointer to target netif.
 * @param gateway the gateway address to use to send through. Has to be link local.
 * @param index return value argument of index where route entry was added in table.
 * @return ERR_OK  if addition was successful.
 *         ERR_MEM if table is already full.
 *         ERR_ARG if passed argument is bad or route already exists in table.
 */
err_t 
ip6_add_route_entry(struct ip6_prefix *ip6_prefix, struct netif *netif, ip6_addr_t *gateway, s8_t *index)
{
  int i = -1;
  err_t retval = ERR_OK;

  if (!ip6_prefix_valid(ip6_prefix->prefix_len) || (netif == NULL)) {
    retval = ERR_ARG;
    goto exit;
  }

  /* Check if an entry already exists with matching prefix; If so, replace it. */
  for (i = 0; i < LWIP_IPV6_NUM_ROUTE_ENTRIES; i++) {
    if ((ip6_prefix->prefix_len == static_route_table[i].prefix.prefix_len) &&
        memcmp(&ip6_prefix->addr, &static_route_table[i].prefix.addr,
               ip6_prefix->prefix_len / 8) == 0) {
      //Prefix matches; replace the netif with the one being added.
      goto insert;
    }
  }

  /* Check if the table is full */
  if (static_route_table[LWIP_IPV6_NUM_ROUTE_ENTRIES - 1].netif != NULL) {
    retval = ERR_MEM;
    goto exit;
  }

  /* Shift all entries down the table until slot is found */
  for (i = LWIP_IPV6_NUM_ROUTE_ENTRIES - 1;
       i > 0 && (ip6_prefix->prefix_len > static_route_table[i - 1].prefix.prefix_len); i--) {
    SMEMCPY(&static_route_table[i], &static_route_table[i - 1], sizeof(struct ip6_route_entry));
  }

insert:
  /* Insert into the slot selected */
  SMEMCPY(&static_route_table[i].prefix, ip6_prefix, sizeof(struct ip6_prefix));
  static_route_table[i].netif = netif;
 
  /* Add gateway to route table */
  static_route_table[i].gateway = gateway;
  
  if (index != NULL) {
    *index = i;
  }

exit:  
  return retval;
}

/**
 * Removes the route entry from the static route table.
 *
 * @param ip6_prefix the route prefix entry to delete.
 */
void
ip6_remove_route_entry(struct ip6_prefix *ip6_prefix)
{
  int i, pos = -1;

  for (i = 0; i < LWIP_IPV6_NUM_ROUTE_ENTRIES; i++) {
    /* compare prefix to find position to delete */
    if (ip6_prefix->prefix_len == static_route_table[i].prefix.prefix_len &&
        memcmp(&ip6_prefix->addr, &static_route_table[i].prefix.addr,
               ip6_prefix->prefix_len / 8) == 0) {
      pos = i;
      break;
    }
  }

  if (pos >= 0) {
    /* Shift everything beyond pos one slot up */
    for (i = pos; i < LWIP_IPV6_NUM_ROUTE_ENTRIES - 1; i++) {
      SMEMCPY(&static_route_table[i], &static_route_table[i+1], sizeof(struct ip6_route_entry));
      if (static_route_table[i].netif == NULL) {
        break;
      }
    }
    /* Zero the remaining entries */
    for (; i < LWIP_IPV6_NUM_ROUTE_ENTRIES; i++) {
      ip6_addr_set_zero((&static_route_table[i].prefix.addr));
      static_route_table[i].netif = NULL;
    }
  }

  return;
}

/**
 * Finds the appropriate route entry in the static route table corresponding to the given
 * destination IPv6 address. Since the entries in the route table are kept sorted in decreasing
 * order of prefix length, a linear search down the list is performed to retrieve a matching
 * index.
 *
 * @param ip6_dest_addr the destination address to match
 * @return the index of the found route entry; -1 if not found.
 */
s8_t 
ip6_find_route_entry(const ip6_addr_t *ip6_dest_addr)
{
  int i, index = -1;

  /* Search prefix in the sorted(decreasing order of prefix length) list */
  for(i = 0; i < LWIP_IPV6_NUM_ROUTE_ENTRIES; i++) {
    if (memcmp(ip6_dest_addr, &static_route_table[i].prefix.addr,
        static_route_table[i].prefix.prefix_len / 8) == 0) {
      index = i;
      break;
    }
  }

  return index;
}

/**
 * Finds the appropriate network interface for a given IPv6 address from a routing table with
 * static IPv6 routes.
 *
 * @param src the source IPv6 address, if known
 * @param dest the destination IPv6 address for which to find the route
 * @return the netif on which to send to reach dest
 */ 
struct netif *
ip6_static_route(const ip6_addr_t *src, const ip6_addr_t *dest)
{

  int i;
  /* Perform table lookup */
  i = ip6_find_route_entry(dest);
  
  if (i >= 0) {
    return static_route_table[i].netif;
  }
  else {
    return NULL;
  }
}

/**
 * Finds the gateway IP6 address for a given destination IPv6 address and target netif
 * from a routing table with static IPv6 routes.
 *
 * @param netif the netif used for sending
 * @param dest the destination IPv6 address
 * @return the ip6 address of the gateway to forward packet to
 */ 
ip6_addr_t *
ip6_get_gateway(struct netif *netif, const ip6_addr_t *dest)
{
  ip6_addr_t *ret_gw = NULL;
  const int i = ip6_find_route_entry(dest);
  
  if (i >= 0) {
    if (static_route_table[i].gateway != NULL) {
      ret_gw = static_route_table[i].gateway;
    }
  }

  return ret_gw;
}

/**
 * Returns the top of the route table.
 * This should be used for debug printing only.
 *
 * @return the top of the route table.
 */
struct ip6_route_entry *
ip6_get_route_table(void)
{
    return static_route_table;
}

#endif /* LWIP_IPV6_ROUTE_TABLE_SUPPORT */

#endif /* LWIP_IPV6 */
