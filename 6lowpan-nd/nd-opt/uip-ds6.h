/**
 * \addtogroup uip6
 * @{
 */

/**
 * \file
 *         Network interface and stateless autoconfiguration (RFC 4862)
 * \author Mathilde Durvy <mdurvy@cisco.com>
 * \author Julien Abeille <jabeille@cisco.com>
 *
 */
/*
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *
 */

#ifndef __UIP_DS6_H__
#define __UIP_DS6_H__

#include "net/uip.h"
#include "sys/stimer.h"
#include "nd-opt/uip-nd6.h"

/*--------------------------------------------------*/
/** Configuration. For all tables (Neighbor cache, Prefix List, Routing Table,
 * Default Router List, Unicast address list, multicast address list, anycast address list),
 * we define:
 * - the number of elements requested by the user in contiki configuration (name suffixed by _NBU)
 * - the number of elements assigned by the system (name suffixed by _NBS)
 * - the total number of elements is the sum (name suffixed by _NB)
*/
/* Neighbor cache */
#define UIP_DS6_NBR_NBS 0
#ifndef UIP_CONF_DS6_NBR_NBU
#define UIP_DS6_NBR_NBU  4
#else
#define UIP_DS6_NBR_NBU UIP_CONF_DS6_NBR_NBU
#endif
#define UIP_DS6_NBR_NB UIP_DS6_NBR_NBS + UIP_DS6_NBR_NBU

/* Default router list */
#define UIP_DS6_DEFRT_NBS 0
#ifndef UIP_CONF_DS6_DEFRT_NBU
#define UIP_DS6_DEFRT_NBU 2
#else
#define UIP_DS6_DEFRT_NBU UIP_CONF_DS6_DEFRT_NBU
#endif
#define UIP_DS6_DEFRT_NB UIP_DS6_DEFRT_NBS + UIP_DS6_DEFRT_NBU

/* Prefix list */
#define UIP_DS6_PREFIX_NBS  1
#ifndef UIP_CONF_DS6_PREFIX_NBU
#define UIP_DS6_PREFIX_NBU  2
#else
#define UIP_DS6_PREFIX_NBU UIP_CONF_DS6_PREFIX_NBU
#endif
#define UIP_DS6_PREFIX_NB UIP_DS6_PREFIX_NBS + UIP_DS6_PREFIX_NBU

/* Routing table */
#define UIP_DS6_ROUTE_NBS 0
#ifndef UIP_CONF_DS6_ROUTE_NBU
#define UIP_DS6_ROUTE_NBU 4
#else
#define UIP_DS6_ROUTE_NBU UIP_CONF_DS6_ROUTE_NBU
#endif
#define UIP_DS6_ROUTE_NB UIP_DS6_ROUTE_NBS + UIP_DS6_ROUTE_NBU

/* Unicast address list*/
#define UIP_DS6_ADDR_NBS 1
#ifndef UIP_CONF_DS6_ADDR_NBU
#define UIP_DS6_ADDR_NBU 2
#else
#define UIP_DS6_ADDR_NBU UIP_CONF_DS6_ADDR_NBU
#endif
#define UIP_DS6_ADDR_NB UIP_DS6_ADDR_NBS + UIP_DS6_ADDR_NBU

/* Multicast address list */
#if UIP_CONF_ROUTER
#define UIP_DS6_MADDR_NBS 2 + UIP_DS6_ADDR_NB   /* all routers + all nodes + one solicited per unicast */
#else
#define UIP_DS6_MADDR_NBS 1 /* all nodes */
#endif
#ifndef UIP_CONF_DS6_MADDR_NBU
#define UIP_DS6_MADDR_NBU 0
#else
#define UIP_DS6_MADDR_NBU UIP_CONF_DS6_MADDR_NBU
#endif
#define UIP_DS6_MADDR_NB UIP_DS6_MADDR_NBS + UIP_DS6_MADDR_NBU

/* Anycast address list */
#if UIP_CONF_ROUTER
#define UIP_DS6_AADDR_NBS UIP_DS6_PREFIX_NB - 1 /* One per non link local prefix (subnet prefix anycast address) */
#else
#define UIP_DS6_AADDR_NBS 0
#endif
#ifndef UIP_CONF_DS6_AADDR_NBU
#define UIP_DS6_AADDR_NBU 0
#else
#define UIP_DS6_AADDR_NBU UIP_CONF_DS6_AADDR_NBU
#endif
#define UIP_DS6_AADDR_NB UIP_DS6_AADDR_NBS + UIP_DS6_AADDR_NBU

/* The host SHOULD start sending Router Solicitations "well before the
 * minimum of those lifetimes" (across all the prefixes and all the
 * contexts) expire. I-D.ietf-6lowpan-nd. We define thus a threshold 
 * value to start sending RS messages (in seconds).*/
#ifdef UIP_DS6_CONF_LIFETIME_THRESHOLD
#define UIP_DS6_LIFETIME_THRESHOLD UIP_DS6_CONF_LIFETIME_THRESHOLD
#else
#define UIP_DS6_LIFETIME_THRESHOLD 60
#endif
/* 6lowpan-nd default lifetimes (in seconds)*/
#ifdef UIP_DS6_GARBAGE_COLLECTIBLE_REG_LIFETIME
#define UIP_DS6_GARBAGE_COLLECTIBLE_REG_LIFETIME UIP_DS6_GARBAGE_COLLECTIBLE_REG_LIFETIME
#else
#define UIP_DS6_GARBAGE_COLLECTIBLE_REG_LIFETIME 20
#endif
#ifdef UIP_DS6_TENTATIVE_REG_LIFETIME
#define UIP_DS6_TENTATIVE_REG_LIFETIME UIP_DS6_TENTATIVE_REG_LIFETIME
#else
#define UIP_DS6_TENTATIVE_REG_LIFETIME 20 /* Default value in I-D.ietf-6lowpan-nd*/
#endif

#ifdef UIP_DS6_CONF_REGS_PER_ADDR
#define UIP_DS6_REGS_PER_ADDR UID_DS6_CONF_REGS_PER_ADDR
#else
#define UIP_DS6_REGS_PER_ADDR UIP_DS6_DEFRT_NB 
#endif
#define UIP_DS6_REG_LIST_SIZE UIP_DS6_REGS_PER_ADDR * UIP_DS6_ADDR_NB


/*--------------------------------------------------*/
/** \brief Possible states for the nbr cache entries */
/* if 6lowpan-nd is used, new states are defined (new states are 
 * orthogonal to those defined in rfc4861) */
#define  REG_GARBAGE_COLLECTIBLE 0
#define  REG_TENTATIVE 1
#define  REG_REGISTERED 2
#define  REG_TO_BE_UNREGISTERED 3 /* Auxiliar registration entry state */


#define  NBR_INCOMPLETE 0
#define  NBR_REACHABLE 1
#define  NBR_STALE 2
#define  NBR_DELAY 3
#define  NBR_PROBE 4
/** \brief Possible states for an address  (RFC 4862) */
#define ADDR_TENTATIVE 0
#define ADDR_PREFERRED 1
#define ADDR_DEPRECATED 2

/** \brief How the address was acquired: Autoconf, DHCP or manually */
#define  ADDR_ANYTYPE 0
#define  ADDR_AUTOCONF 1
#define  ADDR_DHCP 2
#define  ADDR_MANUAL 3

/** \brief General DS6 definitions */
#define UIP_DS6_PERIOD   (CLOCK_SECOND/10)  /** Period for uip-ds6 periodic task*/
#define FOUND 0
#define FREESPACE 1
#define NOSPACE 2


/*--------------------------------------------------*/
/** \brief An entry in the nbr cache */
typedef struct uip_ds6_nbr {
  u8_t isused;
  uip_ipaddr_t ipaddr;
  uip_lladdr_t lladdr;
  struct stimer reachable;
  clock_time_t last_lookup;
  struct stimer sendns;
  u8_t nscount;
  u8_t isrouter;
  u8_t state;
} uip_ds6_nbr_t;

/** \brief An entry in the default router list */
typedef struct uip_ds6_defrt {
  u8_t isused;
  uip_ipaddr_t ipaddr;
  struct stimer lifetime;
  u8_t isinfinite;
/* 6lowpan-nd fields required to maintain a counter of RSs sent to
 * a particular default router */
  u8_t sending_rs;
  u8_t rscount;
  /* The number of registrations with a router */
  u8_t registrations;
} uip_ds6_defrt_t;

/** \brief A prefix list entry */
#if UIP_CONF_ROUTER
typedef struct uip_ds6_prefix {
  u8_t isused;
  uip_ipaddr_t ipaddr;
  u8_t length;
  u8_t advertise;
  u32_t vlifetime;
  u32_t plifetime;
  u8_t l_a_reserved; /**< on-link and autonomous flags + 6 reserved bits */
} uip_ds6_prefix_t;
#else /* UIP_CONF_ROUTER */
typedef struct uip_ds6_prefix {
  u8_t isused;
  uip_ipaddr_t ipaddr;
  u8_t length;
  struct stimer vlifetime;
  u8_t isinfinite;
  /* The router that announced this prefix */
  uip_ds6_defrt_t* defrt;
} uip_ds6_prefix_t;
#endif /*UIP_CONF_ROUTER */

/** * \brief Unicast address structure */
typedef struct uip_ds6_addr {
  u8_t isused;
  uip_ipaddr_t ipaddr;
  u8_t state;
  u8_t type;
  u8_t isinfinite;
  struct stimer vlifetime;
  /* The router that announced the prefix of this address */
  uip_ds6_defrt_t* defrt;
} uip_ds6_addr_t;

/** \brief Anycast address  */
typedef struct uip_ds6_aaddr {
  u8_t isused;
  uip_ipaddr_t ipaddr;
} uip_ds6_aaddr_t;

/** \brief A multicast address */
typedef struct uip_ds6_maddr {
  u8_t isused;
  uip_ipaddr_t ipaddr;
} uip_ds6_maddr_t;

/* Structure to handle 6lowpan-nd registrations */
typedef struct uip_ds6_reg {
  u8_t isused;
  u8_t state;
  uip_ds6_addr_t* addr;
  uip_ds6_defrt_t* defrt; 
  struct stimer reg_lifetime;
  struct timer registration_timer;
  u8_t reg_count;
} uip_ds6_reg_t;

#if CONF_6LOWPAN_ND_6CO
/**
 * \brief An address context for IPHC address compression
 * each context prefix may have upto 128 bits
 */
typedef enum uip_ds6_context_state {
	NOT_IN_USE = 0,
	IN_USE_UNCOMPRESS_ONLY,
	IN_USE_COMPRESS,
	EXPIRED,
} uip_ds6_context_state_t;
 
typedef struct uip_ds6_addr_context {
  uip_ds6_context_state_t state;
  u8_t length;
  u8_t context_id;
  uip_ipaddr_t prefix;
  struct stimer vlifetime;
  /* The router that announced this context */
  uip_ds6_defrt_t* defrt;
  /* According to I-D.ietf-6lowpan-nd, if a context valid lifetime expires,
   * it must be set to a decompression-only state during a period of "twice
   * the Default Router Lifetime". After that period, if no 6CO has been 
   * received to update that context, it should be deleted. Therefore, we 
   * need to keep the default router lifetime. Moreover, we can not use the
   * corresponding value in "defrt" because that router may have been deleted */
  u16_t defrt_lifetime;
} uip_ds6_addr_context_t;
#endif /* CONF_6LOWPAN_ND_6CO */

/** \brief define some additional RPL related route state and
 *  neighbor callback for RPL - if not a DS6_ROUTE_STATE is already set */
#ifndef UIP_DS6_ROUTE_STATE_TYPE
#define UIP_DS6_ROUTE_STATE_TYPE rpl_route_entry_t
/* Needed for the extended route entry state when using ContikiRPL */
typedef struct rpl_route_entry {
  u32_t lifetime;
  u32_t saved_lifetime;
  void *dag;
  u8_t learned_from;
} rpl_route_entry_t;
#endif /* UIP_DS6_ROUTE_STATE_TYPE */

/* only define the callback if RPL is active */
#if UIP_CONF_IPV6_RPL
#ifndef UIP_CONF_DS6_NEIGHBOR_STATE_CHANGED
#define UIP_CONF_DS6_NEIGHBOR_STATE_CHANGED rpl_ipv6_neighbor_callback
#endif /* UIP_CONF_DS6_NEIGHBOR_STATE_CHANGED */
#endif /* UIP_CONF_IPV6_RPL */



/** \brief An entry in the routing table */
typedef struct uip_ds6_route {
  u8_t isused;
  uip_ipaddr_t ipaddr;
  u8_t length;
  u8_t metric;
  uip_ipaddr_t nexthop;
#ifdef UIP_DS6_ROUTE_STATE_TYPE
  UIP_DS6_ROUTE_STATE_TYPE state;
#endif
} uip_ds6_route_t;

/** \brief  Interface structure (contains all the interface variables) */
typedef struct uip_ds6_netif {
  u32_t link_mtu;
  u8_t cur_hop_limit;
  u32_t base_reachable_time; /* in msec */
  u32_t reachable_time;      /* in msec */
  u32_t retrans_timer;       /* in msec */
  uip_ds6_reg_t* registration_in_progress;
  uip_ds6_addr_t addr_list[UIP_DS6_ADDR_NB];
#if UIP_DS6_AADDR_NB > 0 /* Some compilers interpret zero-length vectors as
                            incomplete types */
  uip_ds6_aaddr_t aaddr_list[UIP_DS6_AADDR_NB];
#endif
  uip_ds6_maddr_t maddr_list[UIP_DS6_MADDR_NB];
} uip_ds6_netif_t;

/** \brief Generic type for a DS6, to use a common loop though all DS */
typedef struct uip_ds6_element {
  u8_t isused;
  uip_ipaddr_t ipaddr;
} uip_ds6_element_t;


/*---------------------------------------------------------------------------*/
extern uip_ds6_netif_t uip_ds6_if;
extern struct etimer uip_ds6_timer_periodic;
extern uip_ds6_defrt_t uip_ds6_defrt_list[UIP_DS6_DEFRT_NB];
#if CONF_6LOWPAN_ND_6CO & SICSLOWPAN_CONF_MAX_ADDR_CONTEXTS > 0
extern uip_ds6_addr_context_t uip_ds6_addr_context_list[SICSLOWPAN_CONF_MAX_ADDR_CONTEXTS];
#endif /* CONF_6LOWPAN_ND_6CO & SICSLOWPAN_CONF_MAX_ADDR_CONTEXTS > 0 */

#if UIP_CONF_ROUTER
extern uip_ds6_prefix_t uip_ds6_prefix_list[UIP_DS6_PREFIX_NB];
#else /* UIP_CONF_ROUTER */
extern struct timer uip_ds6_timer_rs;
extern u8_t rscount;
#endif /* UIP_CONF_ROUTER */


/*---------------------------------------------------------------------------*/
/** \brief Initialize data structures */
void uip_ds6_init(void);

/** \brief Periodic processing of data structures */
void uip_ds6_periodic(void);

/** \brief Generic loop routine on an abstract data structure, which generalizes
 * all data structures used in DS6 */
u8_t uip_ds6_list_loop(uip_ds6_element_t *list, u8_t size,
                          u16_t elementsize, uip_ipaddr_t *ipaddr,
                          u8_t ipaddrlen,
                          uip_ds6_element_t **out_element);

/** \name Neighbor Cache basic routines */
/** @{ */
uip_ds6_nbr_t *uip_ds6_nbr_add(uip_ipaddr_t *ipaddr, uip_lladdr_t *lladdr,
                               u8_t isrouter, u8_t state);
void uip_ds6_nbr_rm(uip_ds6_nbr_t *nbr);
uip_ds6_nbr_t *uip_ds6_nbr_lookup(uip_ipaddr_t *ipaddr);

/** \name 6lowpan-nd registration basic routines */
/** @{ */
uip_ds6_reg_t* uip_ds6_reg_add(uip_ds6_addr_t* addr, uip_ds6_defrt_t* defrt,
                               u8_t state);
void uip_ds6_reg_rm(uip_ds6_reg_t* reg);
uip_ds6_reg_t *uip_ds6_reg_lookup(uip_ds6_addr_t* addr, uip_ds6_defrt_t* defrt);
void uip_ds6_reg_cleanup_defrt(uip_ds6_defrt_t* defrt);
void uip_ds6_reg_cleanup_addr(uip_ds6_addr_t* addr);
/** @} */

/** \name Default router list basic routines */
/** @{ */
u8_t uip_ds6_get_registrations(uip_ds6_defrt_t *defrt);
uip_ds6_defrt_t* uip_ds6_defrt_choose_min_reg(uip_ds6_addr_t* addr);
u8_t uip_ds6_is_nbr_garbage_collectible(uip_ds6_nbr_t *nbr);

#if CONF_6LOWPAN_ND_6CO
/** \name Context table basic routines */
/** @{ */
uip_ds6_addr_context_t *uip_ds6_context_add(uip_nd6_opt_6co *context_option,
																						u16_t defrt_lifetime);
void uip_ds6_context_rm(uip_ds6_addr_context_t *context);
uip_ds6_addr_context_t *uip_ds6_context_lookup_by_id(u8_t context_id);
uip_ds6_addr_context_t *uip_ds6_context_lookup_by_prefix(uip_ipaddr_t *prefix);
/** @} */
#endif /* CONF_6LOWPAN_ND_6CO */

uip_ds6_defrt_t *uip_ds6_defrt_add(uip_ipaddr_t *ipaddr,
                                   unsigned long interval);
void uip_ds6_defrt_rm(uip_ds6_defrt_t *defrt);
uip_ds6_defrt_t *uip_ds6_defrt_lookup(uip_ipaddr_t *ipaddr);
uip_ipaddr_t *uip_ds6_defrt_choose(void);

/** @} */

/** \name Prefix list basic routines */
/** @{ */
#if UIP_CONF_ROUTER
uip_ds6_prefix_t *uip_ds6_prefix_add(uip_ipaddr_t *ipaddr, u8_t length,
                                     u8_t advertise, u8_t flags,
                                     unsigned long vtime,
                                     unsigned long ptime);
#else /* UIP_CONF_ROUTER */
uip_ds6_prefix_t *uip_ds6_prefix_add(uip_ipaddr_t *ipaddr, u8_t length,
                                     unsigned long interval);
#endif /* UIP_CONF_ROUTER */
void uip_ds6_prefix_rm(uip_ds6_prefix_t *prefix);
uip_ds6_prefix_t *uip_ds6_prefix_lookup(uip_ipaddr_t *ipaddr,
                                        u8_t ipaddrlen);
u8_t uip_ds6_is_addr_onlink(uip_ipaddr_t *ipaddr);

/** @} */

/** \name Unicast address list basic routines */
/** @{ */
uip_ds6_addr_t *uip_ds6_addr_add(uip_ipaddr_t *ipaddr,
                                 unsigned long vlifetime, u8_t type);
void uip_ds6_addr_rm(uip_ds6_addr_t *addr);
uip_ds6_addr_t *uip_ds6_addr_lookup(uip_ipaddr_t *ipaddr);
uip_ds6_addr_t *uip_ds6_get_link_local(signed char state);
uip_ds6_addr_t *uip_ds6_get_global(signed char state);

/** @} */

/** \name Multicast address list basic routines */
/** @{ */
uip_ds6_maddr_t *uip_ds6_maddr_add(uip_ipaddr_t *ipaddr);
void uip_ds6_maddr_rm(uip_ds6_maddr_t *maddr);
uip_ds6_maddr_t *uip_ds6_maddr_lookup(uip_ipaddr_t *ipaddr);

/** @} */

/** \name Anycast address list basic routines */
/** @{ */
uip_ds6_aaddr_t *uip_ds6_aaddr_add(uip_ipaddr_t *ipaddr);
void uip_ds6_aaddr_rm(uip_ds6_aaddr_t *aaddr);
uip_ds6_aaddr_t *uip_ds6_aaddr_lookup(uip_ipaddr_t *ipaddr);

/** @} */


/** \name Routing Table basic routines */
/** @{ */
uip_ds6_route_t *uip_ds6_route_lookup(uip_ipaddr_t *destipaddr);
uip_ds6_route_t *uip_ds6_route_add(uip_ipaddr_t *ipaddr, u8_t length,
                                   uip_ipaddr_t *next_hop, u8_t metric);
void uip_ds6_route_rm(uip_ds6_route_t *route);
void uip_ds6_route_rm_by_nexthop(uip_ipaddr_t *nexthop);

/** @} */

/** \brief set the last 64 bits of an IP address based on the MAC address */
void uip_ds6_set_addr_iid(uip_ipaddr_t * ipaddr, uip_lladdr_t * lladdr);

/** \brief Get the number of matching bits of two addresses */
u8_t get_match_length(uip_ipaddr_t * src, uip_ipaddr_t * dst);
/** \brief Source address selection, see RFC 3484 */
void uip_ds6_select_src(uip_ipaddr_t * src, uip_ipaddr_t * dst);

#if UIP_CONF_ROUTER
#if UIP_ND6_SEND_RA
/** \brief Send a RA as an asnwer to a RS */
void uip_ds6_send_ra_solicited(void);

/** \brief Send a periodic RA */
void uip_ds6_send_ra_periodic(void);
#endif /* UIP_ND6_SEND_RA */
#else /* UIP_CONF_ROUTER */
/** \brief Trigger start sending RSs */
void uip_ds6_send_rs(uip_ds6_defrt_t *defrt);
#endif /* UIP_CONF_ROUTER */

/** \brief Compute the reachable time based on base reachable time, see RFC 4861*/
u32_t uip_ds6_compute_reachable_time(void); /** \brief compute random reachable timer */

/** \name Macros to check if an IP address (unicast, multicast or anycast) is mine */
/** @{ */
#define uip_ds6_is_my_addr(addr)  (uip_ds6_addr_lookup(addr) != NULL)
#define uip_ds6_is_my_maddr(addr) (uip_ds6_maddr_lookup(addr) != NULL)
#define uip_ds6_is_my_aaddr(addr) (uip_ds6_aaddr_lookup(addr) != NULL)
/** @} */
/** @} */

#endif /* __UIP_DS6_H__ */
