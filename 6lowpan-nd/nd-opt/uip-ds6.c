/**
 * \addtogroup uip6
 * @{
 */

/**
 * \file
 *         IPv6 data structures handling functions
 *         Comprises part of the Neighbor discovery (RFC 4861)
 *         and auto configuration (RFC 4862 )state machines
 * \author Mathilde Durvy <mdurvy@cisco.com>
 * \author Julien Abeille <jabeille@cisco.com>
 */
/*
 * Copyright (c) 2006, Swedish Institute of Computer Science.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *   may be used to endorse or promote products derived from this software
 *   without specific prior written permission.
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
 */
#include <string.h>
#include <stdlib.h>
#include "lib/random.h"
#include "nd-opt/uip-nd6.h"
#include "nd-opt/uip-ds6.h"
#include "net/uip-packetqueue.h"

#define DEBUG 0
#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#define PRINT6ADDR(addr) PRINTF(" %02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x ", ((u8_t *)addr)[0], ((u8_t *)addr)[1], ((u8_t *)addr)[2], ((u8_t *)addr)[3], ((u8_t *)addr)[4], ((u8_t *)addr)[5], ((u8_t *)addr)[6], ((u8_t *)addr)[7], ((u8_t *)addr)[8], ((u8_t *)addr)[9], ((u8_t *)addr)[10], ((u8_t *)addr)[11], ((u8_t *)addr)[12], ((u8_t *)addr)[13], ((u8_t *)addr)[14], ((u8_t *)addr)[15])
#define PRINTLLADDR(lladdr) PRINTF(" %02x:%02x:%02x:%02x:%02x:%02x ",lladdr->addr[0], lladdr->addr[1], lladdr->addr[2], lladdr->addr[3],lladdr->addr[4], lladdr->addr[5])
#else
#define PRINTF(...)
#define PRINT6ADDR(addr)
#define PRINTLLADDR(addr)
#endif

#ifdef UIP_CONF_DS6_NEIGHBOR_STATE_CHANGED
#define NEIGHBOR_STATE_CHANGED(n) UIP_CONF_DS6_NEIGHBOR_STATE_CHANGED(n)
void NEIGHBOR_STATE_CHANGED(uip_ds6_nbr_t *n);
#else
#define NEIGHBOR_STATE_CHANGED(n)
#endif /* UIP_DS6_CONF_NEIGHBOR_STATE_CHANGED */

struct etimer uip_ds6_timer_periodic;                           /** \brief Timer for maintenance of data structures */

#if UIP_CONF_ROUTER
struct stimer uip_ds6_timer_ra;                                 /** \brief RA timer, to schedule RA sending */
#if UIP_ND6_SEND_RA
static u8_t racount;                                         /** \brief number of RA already sent */
static u16_t rand_time;                                      /** \brief random time value for timers */
#endif
#else /* UIP_CONF_ROUTER */
struct timer uip_ds6_timer_rs;                                 /** \brief RS timer, to schedule RS sending */
u8_t rscount = 0;                                         /** \brief number of rs already sent */
#endif /* UIP_CONF_ROUTER */

/** \name "DS6" Data structures */
/** @{ */
uip_ds6_netif_t uip_ds6_if;                                       /** \brief The single interface */
uip_ds6_nbr_t uip_ds6_nbr_cache[UIP_DS6_NBR_NB];                  /** \brief Neighor cache */
uip_ds6_reg_t uip_ds6_reg_list[UIP_DS6_REG_LIST_SIZE];				/** \brief Registrations list */
#if CONF_6LOWPAN_ND_6CO
uip_ds6_addr_context_t uip_ds6_addr_context_table[SICSLOWPAN_CONF_MAX_ADDR_CONTEXTS]; /** \brief Contexts list */
#endif /* CONF_6LOWPAN_ND_6CO */
uip_ds6_defrt_t uip_ds6_defrt_list[UIP_DS6_DEFRT_NB];             /** \brief Default rt list */
uip_ds6_prefix_t uip_ds6_prefix_list[UIP_DS6_PREFIX_NB];          /** \brief Prefix list */
uip_ds6_route_t uip_ds6_routing_table[UIP_DS6_ROUTE_NB];          /** \brief Routing table */

/** @} */

/* "full" (as opposed to pointer) ip address used in this file,  */
static uip_ipaddr_t loc_fipaddr;

/* Pointers used in this file */
static uip_ipaddr_t *locipaddr;
static uip_ds6_addr_t *locaddr;
static uip_ds6_maddr_t *locmaddr;
#if UIP_DS6_AADDR_NB > 0
static uip_ds6_aaddr_t *locaaddr;
#endif /* UIP_DS6_AADDR_NB > 0 */
static uip_ds6_prefix_t *locprefix;
static uip_ds6_nbr_t *locnbr;
static uip_ds6_defrt_t *locdefrt;
static uip_ds6_route_t *locroute;
static uip_ds6_reg_t *locreg;
static uip_ds6_defrt_t *min_defrt; /* default router with minimum lifetime */
static unsigned long min_lifetime; /* minimum lifetime */
#if CONF_6LOWPAN_ND_6CO
static uip_ds6_addr_context_t *loccontext;
#endif /* CONF_6LOWPAN_ND_6CO */
/*---------------------------------------------------------------------------*/
void
uip_ds6_init(void)
{
  PRINTF("Init of IPv6 data structures\n");
  PRINTF("%u neighbors\n%u default routers\n%u prefixes\n%u routes\n%u unicast addresses\n%u multicast addresses\n%u anycast addresses\n",
     UIP_DS6_NBR_NB, UIP_DS6_DEFRT_NB, UIP_DS6_PREFIX_NB, UIP_DS6_ROUTE_NB,
     UIP_DS6_ADDR_NB, UIP_DS6_MADDR_NB, UIP_DS6_AADDR_NB);
  memset(uip_ds6_nbr_cache, 0, sizeof(uip_ds6_nbr_cache));
	memset(uip_ds6_reg_list, 0, sizeof(uip_ds6_reg_list));
#if CONF_6LOWPAN_ND_6CO
	memset(uip_ds6_addr_context_table, 0, sizeof(uip_ds6_addr_context_table));
#endif /* CONF_6LOWPAN_ND_6CO */	
  memset(uip_ds6_defrt_list, 0, sizeof(uip_ds6_defrt_list));
  memset(uip_ds6_prefix_list, 0, sizeof(uip_ds6_prefix_list));
  memset(&uip_ds6_if, 0, sizeof(uip_ds6_if));
  memset(uip_ds6_routing_table, 0, sizeof(uip_ds6_routing_table));

  /* Set interface parameters */
  uip_ds6_if.link_mtu = UIP_LINK_MTU;
  uip_ds6_if.cur_hop_limit = UIP_TTL;
  uip_ds6_if.base_reachable_time = UIP_ND6_REACHABLE_TIME;
  uip_ds6_if.reachable_time = uip_ds6_compute_reachable_time();
  uip_ds6_if.retrans_timer = UIP_ND6_RETRANS_TIMER;

  /* Create link local address, prefix, multicast addresses, anycast addresses */
  uip_create_linklocal_prefix(&loc_fipaddr);
#if UIP_CONF_ROUTER
  uip_ds6_prefix_add(&loc_fipaddr, UIP_DEFAULT_PREFIX_LEN, 0, 0, 0, 0);
#else /* UIP_CONF_ROUTER */
  uip_ds6_prefix_add(&loc_fipaddr, UIP_DEFAULT_PREFIX_LEN, 0);
#endif /* UIP_CONF_ROUTER */
  uip_ds6_set_addr_iid(&loc_fipaddr, &uip_lladdr);
  uip_ds6_addr_add(&loc_fipaddr, 0, ADDR_AUTOCONF);

  uip_create_linklocal_allnodes_mcast(&loc_fipaddr);
  uip_ds6_maddr_add(&loc_fipaddr);
#if UIP_CONF_ROUTER
  uip_create_linklocal_allrouters_mcast(&loc_fipaddr);
  uip_ds6_maddr_add(&loc_fipaddr);
#if UIP_ND6_SEND_RA
  stimer_set(&uip_ds6_timer_ra, 2);     /* wait to have a link local IP address */
#endif /* UIP_ND6_SEND_RA */
#else /* UIP_CONF_ROUTER */
	timer_set(&uip_ds6_timer_rs,
             random_rand() % (UIP_ND6_MAX_RTR_SOLICITATION_DELAY *
                              CLOCK_SECOND));
#endif /* UIP_CONF_ROUTER */
  etimer_set(&uip_ds6_timer_periodic, UIP_DS6_PERIOD);

  return;
}


/*---------------------------------------------------------------------------*/
void
uip_ds6_periodic(void)
{
	/* This flag signals whether we allow or not to send a packet in the current 
	 * invocation. */
  u8_t allow_output = 1;

  /* minimum lifetime */
  min_lifetime = 0xFFFFFFFF;
  /* router with minimum lifetime */
  min_defrt = NULL;
	
	/* Periodic processing on registrations */
	for(locreg = uip_ds6_reg_list;
      locreg < uip_ds6_reg_list + UIP_DS6_REG_LIST_SIZE; locreg++) {
  	if (locreg->isused) {
  		if (stimer_expired(&locreg->reg_lifetime)) {
  			uip_ds6_reg_rm(locreg);
  		} else if (allow_output) {
  			/* If no output is allowed, it is pointless to enter here in this invocation */
  			if (uip_ds6_if.registration_in_progress) {
  				/* There is a registration in progress */
  				if ((locreg == uip_ds6_if.registration_in_progress) && 
  						(timer_expired(&locreg->registration_timer))) {
	      		/* We already sent a NS message for this address but there has been no response */
	      		if(locreg->reg_count >= UIP_ND6_MAX_UNICAST_SOLICIT) {
			  			/* NUD failed. Signal the need for next-hop determination by deleting the 
			   			 * NCE (RFC 4861) */
			   			uip_ds6_reg_rm(locreg); 
			  			/* And then, delete neighbor and corresponding router (as hosts only keep
			  			 * NCEs for routers in 6lowpan-nd) */ 
			  			locnbr = uip_ds6_nbr_lookup(&locreg->defrt->ipaddr); 
			  			uip_ds6_nbr_rm(locnbr);
			  			uip_ds6_defrt_rm(locreg->defrt);
			  			/* Since we are deleting a default router, we must delete also all 
			  			 * registrations with that router.
			  			 * Be careful here, uip_ds6_reg_cleanup_defrt() modifies the value of locreg!*/
			  			uip_ds6_reg_cleanup_defrt(locreg->defrt);
		      		/* We will also need to start sending RS, as specified in I-D.ietf-6lowpan-nd 
		      		 * for NUD failure case */
		      		uip_ds6_send_rs(NULL);
		      		uip_ds6_if.registration_in_progress = NULL;
  					} else {
  						locreg->reg_count++;
		      		timer_restart(&locreg->registration_timer);
		   	 			uip_nd6_ns_output(&locreg->addr->ipaddr, &locreg->defrt->ipaddr, 
		          			            &locreg->defrt->ipaddr, 1, UIP_ND6_REGISTRATION_LIFETIME);
 						}
  					allow_output = 0; /* Prevent this invocation from sending anything else */
  				}
  			} else {
  				/* There are no registrations in progress, let's see this entry needs (re)registration
  				 * or deletion */
  				if ((locreg->state == REG_GARBAGE_COLLECTIBLE) || 
  						(locreg->state == REG_TO_BE_UNREGISTERED) || 
  						((locreg->state == REG_REGISTERED) && 
	 						(stimer_remaining(&locreg->reg_lifetime) < stimer_elapsed(&locreg->reg_lifetime)))) {
 						/* Issue (re)registration */
				  	uip_ds6_if.registration_in_progress = locreg;
				  	locreg->reg_count++;
				  	timer_set(&locreg->registration_timer, (uip_ds6_if.retrans_timer / 1000) * CLOCK_SECOND);
				  	if (locreg->state == REG_TO_BE_UNREGISTERED) {
					  	uip_nd6_ns_output(&locreg->addr->ipaddr, &locreg->defrt->ipaddr, 
				      	   			        &locreg->defrt->ipaddr, 1, 0);
				  	} else {
				  		uip_nd6_ns_output(&locreg->addr->ipaddr, &locreg->defrt->ipaddr, 
				      	   			        &locreg->defrt->ipaddr, 1, UIP_ND6_REGISTRATION_LIFETIME);
				  	}   	   			       
			      allow_output = 0; /* Prevent this invocation from sending anything else */
  				}
  			}
  		}
  	}
  }

  
  /* Periodic processing on unicast addresses */
  for(locaddr = uip_ds6_if.addr_list;
      locaddr < uip_ds6_if.addr_list + UIP_DS6_ADDR_NB; locaddr++) {
    if(locaddr->isused) {
      if((!locaddr->isinfinite) && (stimer_expired(&locaddr->vlifetime))) {
        uip_ds6_addr_rm(locaddr);
      } else if (allow_output) {
        if (stimer_remaining(&locaddr->vlifetime) < min_lifetime) {
          min_lifetime = stimer_remaining(&locaddr->vlifetime);
          min_defrt = locaddr->defrt;
        }
      }
    }
  }
  
  /* Periodic processing on default routers */
	if (uip_ds6_defrt_choose() == NULL) {
	  if (allow_output) {
	    /* If default router list is empty, start sending RS */
	    uip_ds6_send_rs(NULL);
	    allow_output = 0; /* Prevent this invocation from sending anything else */
	  }
	} else {
    for(locdefrt = uip_ds6_defrt_list;
        locdefrt < uip_ds6_defrt_list + UIP_DS6_DEFRT_NB; locdefrt++) {
      if((locdefrt->isused) && (!locdefrt->isinfinite)) {
        if (stimer_expired(&(locdefrt->lifetime))) {
          uip_ds6_defrt_rm(locdefrt);
          /* If default router list is empty, we will start sending RS in
           * the next invocation of ds6_periodic() */
        } else {
          if (allow_output) {
            if (stimer_remaining(&locdefrt->lifetime) < min_lifetime) {
              min_lifetime = stimer_remaining(&locdefrt->lifetime);
              min_defrt = locdefrt;
            }
          }
        }
      }
    }
	}
	
#if !UIP_CONF_ROUTER
  /* Periodic processing on prefixes */
  for (locprefix = uip_ds6_prefix_list;
      locprefix < uip_ds6_prefix_list + UIP_DS6_PREFIX_NB; locprefix++) {
    if((locprefix->isused) && (!locprefix->isinfinite)) {
    	if (stimer_expired(&locprefix->vlifetime)) {
      	uip_ds6_prefix_rm(locprefix);
    	} else if (allow_output) {
    		if (stimer_remaining(&locprefix->vlifetime) < min_lifetime) {
    			min_lifetime = stimer_remaining(&locprefix->vlifetime);
    			min_defrt = locprefix->defrt;
    		}
    	}
    }
  }
#endif /* !UIP_CONF_ROUTER */

#if CONF_6LOWPAN_ND_6CO
	/* Periodic processing on contexts */
  for(loccontext = uip_ds6_addr_context_table;
      loccontext < uip_ds6_addr_context_table + SICSLOWPAN_CONF_MAX_ADDR_CONTEXTS; loccontext++) {
    if(loccontext->state != NOT_IN_USE) {
    	if (stimer_expired(&loccontext->vlifetime)) {
    		if (loccontext->state != EXPIRED) {
    			loccontext->state = IN_USE_UNCOMPRESS_ONLY;
    			stimer_set(&loccontext->vlifetime, 2 * loccontext->defrt_lifetime);
    		} else {
      		uip_ds6_context_rm(loccontext);
    		}
    	} else if (allow_output) {
    		if (stimer_remaining(&loccontext->vlifetime) < min_lifetime) {
    			min_lifetime = stimer_remaining(&loccontext->vlifetime);
    			min_defrt = loccontext->defrt;
    		}
    	}
    }
  }
#endif /* CONF_6LOWPAN_ND_6CO */

	/* Start sending RS well before the minimum of the lifetimes (def. router, 
	 * context, or prefix) expires */
	if ((allow_output) && (min_lifetime < UIP_DS6_LIFETIME_THRESHOLD)) {
	  /* Start sending RSs to the router with minimum lifetime (if possible) */
		uip_ds6_send_rs(min_defrt);
		allow_output = 0;
	}

  /* Periodic processing on neighbors */
  for(locnbr = uip_ds6_nbr_cache; locnbr < uip_ds6_nbr_cache + UIP_DS6_NBR_NB;
      locnbr++) {
    if(locnbr->isused) {
      switch (locnbr->state) {
#if UIP_CONF_ROUTER
/* There can not be INCOMPLETE NCEs in a host in 6lowpan-nd */
      case NBR_INCOMPLETE:
    		if (allow_output) {  
	        if(locnbr->nscount >= UIP_ND6_MAX_MULTICAST_SOLICIT) {
	          uip_ds6_nbr_rm(locnbr);
	        } else if(stimer_expired(&(locnbr->sendns))) {
	          locnbr->nscount++;
	          PRINTF("NBR_INCOMPLETE: NS %u\n", locnbr->nscount);
	          uip_nd6_ns_output(NULL, NULL, &locnbr->ipaddr);
	          stimer_set(&(locnbr->sendns), uip_ds6_if.retrans_timer / 1000);
						allow_output = 0;
	        }
    		}  
        break;
#endif /* UIP_CONF_ROUTER */
      case NBR_REACHABLE:
        if(stimer_expired(&(locnbr->reachable))) {
          PRINTF("REACHABLE: moving to STALE (");
          PRINT6ADDR(&locnbr->ipaddr);
          PRINTF(")\n");
          locnbr->state = NBR_STALE;
          NEIGHBOR_STATE_CHANGED(locnbr);
        }
        break;
      case NBR_DELAY:
    		if (allow_output) {  
	        if(stimer_expired(&(locnbr->reachable))) {
	          locnbr->state = NBR_PROBE;
	          locnbr->nscount = 1;
	          NEIGHBOR_STATE_CHANGED(locnbr);
	          PRINTF("DELAY: moving to PROBE + NS %u\n", locnbr->nscount);
	          uip_nd6_ns_output(NULL, &locnbr->ipaddr, &locnbr->ipaddr, 0, 0);
	          stimer_set(&(locnbr->sendns), uip_ds6_if.retrans_timer / 1000);
	          allow_output = 0;
	        }
    		}  
        break;
      case NBR_PROBE:
    		if (allow_output) {  
	        if(locnbr->nscount >= UIP_ND6_MAX_UNICAST_SOLICIT) {
	          PRINTF("PROBE END \n");
	          if((locdefrt = uip_ds6_defrt_lookup(&locnbr->ipaddr)) != NULL) {
	            uip_ds6_defrt_rm(locdefrt);
	          }
	          uip_ds6_nbr_rm(locnbr);
	        } else if(stimer_expired(&(locnbr->sendns))) {
	          locnbr->nscount++;
	          PRINTF("PROBE: NS %u\n", locnbr->nscount);
	          uip_nd6_ns_output(NULL, &locnbr->ipaddr, &locnbr->ipaddr, 0, 0);
	          stimer_set(&(locnbr->sendns), uip_ds6_if.retrans_timer / 1000);
	          allow_output = 0;
	        }
    		}  
        break;
      default:
        break;
      }
    }
  }

#if UIP_CONF_ROUTER & UIP_ND6_SEND_RA 
  /* Periodic RA sending */
  if(stimer_expired(&uip_ds6_timer_ra)) {
    uip_ds6_send_ra_periodic();
  }
#endif /* UIP_CONF_ROUTER & UIP_ND6_SEND_RA */
  etimer_reset(&uip_ds6_timer_periodic);
  return;
}

/*---------------------------------------------------------------------------*/
u8_t
uip_ds6_list_loop(uip_ds6_element_t * list, u8_t size,
                  u16_t elementsize, uip_ipaddr_t * ipaddr,
                  u8_t ipaddrlen, uip_ds6_element_t ** out_element)
{
  uip_ds6_element_t *element;

  *out_element = NULL;

  for(element = list;
      element <
      (uip_ds6_element_t *) ((u8_t *) list + (size * elementsize));
      element = (uip_ds6_element_t *) ((u8_t *) element + elementsize)) {
    //    printf("+ %p %d\n", &element->isused, element->isused);
    if(element->isused) {
      if(uip_ipaddr_prefixcmp(&(element->ipaddr), ipaddr, ipaddrlen)) {
        *out_element = element;
        return FOUND;
      }
    } else {
      *out_element = element;
    }
  }

  if(*out_element != NULL) {
    return FREESPACE;
  } else {
    return NOSPACE;
  }
}

/*---------------------------------------------------------------------------*/
uip_ds6_nbr_t *
uip_ds6_nbr_add(uip_ipaddr_t * ipaddr, uip_lladdr_t * lladdr,
                u8_t isrouter, u8_t state)
{
  int r;

  r = uip_ds6_list_loop
     ((uip_ds6_element_t *) uip_ds6_nbr_cache, UIP_DS6_NBR_NB,
      sizeof(uip_ds6_nbr_t), ipaddr, 128,
      (uip_ds6_element_t **) &locnbr);
  //  printf("r %d\n", r);

  if(r == FREESPACE) {
    locnbr->isused = 1;
    uip_ipaddr_copy(&(locnbr->ipaddr), ipaddr);
    if(lladdr != NULL) {
      memcpy(&(locnbr->lladdr), lladdr, UIP_LLADDR_LEN);
    } else {
      memset(&(locnbr->lladdr), 0, UIP_LLADDR_LEN);
    }
    locnbr->isrouter = isrouter;
    locnbr->state = state;
    /* timers are set separately, for now we put them in expired state */
    stimer_set(&(locnbr->reachable), 0);
    stimer_set(&(locnbr->sendns), 0);
    locnbr->nscount = 0;
    PRINTF("Adding neighbor with ip addr");
    PRINT6ADDR(ipaddr);
    PRINTF("link addr");
    PRINTLLADDR((&(locnbr->lladdr)));
    PRINTF("state %u\n", state);
    NEIGHBOR_STATE_CHANGED(locnbr);

    locnbr->last_lookup = clock_time();
    //    printf("add %p\n", locnbr);
    return locnbr;
  } else if(r == NOSPACE) {
    /* We did not find any empty slot on the neighbor list, so we need
       to remove one old entry to make room. */
    uip_ds6_nbr_t *n, *oldest;
    clock_time_t oldest_time;

    oldest = NULL;
    oldest_time = clock_time();

    for(n = uip_ds6_nbr_cache;
        n < &uip_ds6_nbr_cache[UIP_DS6_NBR_NB];
        n++) {
      if(n->isused) {
        if((n->last_lookup < oldest_time) && (uip_ds6_is_nbr_garbage_collectible(n))) {
        	/* We do not want to remove any non-garbage-collectible entry */
          oldest = n;
          oldest_time = n->last_lookup;
        }
      }
    }
    if(oldest != NULL) {
      //      printf("rm3\n");
      uip_ds6_nbr_rm(oldest);
      locdefrt = uip_ds6_defrt_lookup(&oldest->ipaddr);
      uip_ds6_defrt_rm(locdefrt);
      uip_ds6_reg_cleanup_defrt(locdefrt);
			return uip_ds6_nbr_add(ipaddr, lladdr, isrouter, state);
    }
  }
  PRINTF("uip_ds6_nbr_add drop\n");
  return NULL;
}

/*---------------------------------------------------------------------------*/
void
uip_ds6_nbr_rm(uip_ds6_nbr_t *nbr)
{
  if(nbr != NULL) {
    nbr->isused = 0;
    //    NEIGHBOR_STATE_CHANGED(nbr);
  }
  return;
}

/*---------------------------------------------------------------------------*/
uip_ds6_nbr_t *
uip_ds6_nbr_lookup(uip_ipaddr_t *ipaddr)
{
  if(uip_ds6_list_loop
     ((uip_ds6_element_t *) uip_ds6_nbr_cache, UIP_DS6_NBR_NB,
      sizeof(uip_ds6_nbr_t), ipaddr, 128,
      (uip_ds6_element_t **) & locnbr) == FOUND) {
    return locnbr;
  }
  return NULL;
}
/*---------------------------------------------------------------------------*/
/**
 * \brief						Adds a registration to the registrations list. It also 
 * 									increases the value of the number of registrations of 
 * 									the corresponding default router.
 * 
 * \param addr			The address for which we are adding the registration
 * \param defrt			The default router with which we are registering the 
 * 									address
 * \param state			The state of the registration we are adding (defined in
 * 									I-D.ietf-6lowpan-nd).
 * 
 * \returns					A pointer to the newly create registration if the 
 * 									registration was successfull, otherwise NULL. 
 */
  
uip_ds6_reg_t* 
uip_ds6_reg_add(uip_ds6_addr_t* addr, uip_ds6_defrt_t* defrt, u8_t state) {

	uip_ds6_reg_t* candidate = NULL;

	for (locreg = uip_ds6_reg_list; 
			locreg < uip_ds6_reg_list + UIP_DS6_REG_LIST_SIZE; locreg++) {
		if (!locreg->isused) {
			candidate = locreg;
			break;
		} else if (locreg->state == REG_GARBAGE_COLLECTIBLE) {
			candidate = locreg;
		}
	}
	/* If there was an entry not in use, use it; otherwise overwrite
	 * our canditate entry in Garbage-collectible state*/
	if (candidate != NULL) {
		candidate->isused = 1;
		candidate->addr = addr;
		candidate->defrt = defrt;
		candidate->state = state;
  	timer_set(&candidate->registration_timer, 0);
  	candidate->reg_count = 0;
		if(candidate->state == REG_GARBAGE_COLLECTIBLE) {
			stimer_set(&candidate->reg_lifetime, UIP_DS6_GARBAGE_COLLECTIBLE_REG_LIFETIME);
		} else if (candidate->state == REG_TENTATIVE) {
			stimer_set(&candidate->reg_lifetime, UIP_DS6_TENTATIVE_REG_LIFETIME);
		}
		defrt->registrations++;
		return candidate;
	}
	return NULL;
}

/*---------------------------------------------------------------------------*/
/**
 * \brief						Removes a registration from the registrations list. It also 
 * 									decreases the value of the number of registrations of 
 * 									the corresponding default router.
 * 
 * \param reg				The registration to be deleted.
 */ 
                               
void 
uip_ds6_reg_rm(uip_ds6_reg_t* reg){
	
	reg->defrt->registrations--;
	reg->isused = 0;
	  
}

/*---------------------------------------------------------------------------*/
/**
 * \brief						Looks for a registration in the registrations list.
 * \param addr			The address whose registration we are looking for.
 * \param defrt			The default router with which the address is registered.
 * 
 * \returns reg			The registration matching the search. NULL if there are 
 * 									no matches.
 */ 

uip_ds6_reg_t* 
uip_ds6_reg_lookup(uip_ds6_addr_t* addr, uip_ds6_defrt_t* defrt){
	
	uip_ds6_reg_t* reg;
	
	for (reg = uip_ds6_reg_list; 
			reg < uip_ds6_reg_list + UIP_DS6_REG_LIST_SIZE; reg++) {
		if ((reg->isused) && (reg->addr == addr) && (reg->defrt == defrt)) {
			return reg;
		}
	}	
	return NULL;
}
/*---------------------------------------------------------------------------*/
/**
 * \brief 				Removes all registrations with defrt from the registration
 * 								list.
 * 
 * \param defrt 	The router whose registrations we want to remove.
 * 
 */

void 
uip_ds6_reg_cleanup_defrt(uip_ds6_defrt_t* defrt) {
	
	uip_ds6_reg_t* reg;

	for (reg = uip_ds6_reg_list; 
			reg < uip_ds6_reg_list + UIP_DS6_REG_LIST_SIZE; reg++) {
		if ((reg->isused) && (reg->defrt == defrt)) {
			uip_ds6_reg_rm(reg);
		}
	}
}

/*---------------------------------------------------------------------------*/
/**
 * \brief 				Removes all resgitrations of address addr from the 
 * 								registration list. If the registration is in REGISTERED
 * 								state, we can not just delete it, but we MUST first send
 * 								a NS with ARO lifetime = 0. As there may be more than one,
 * 								we mark it as TO_BE_UNREGISTERED so uip_ds6_periodic can 
 * 								process	them properly.
 * 
 * \param addr 		The address whose registrationes we want to remove.
 * 
 */
	
void 
uip_ds6_reg_cleanup_addr(uip_ds6_addr_t* addr) {
	
	uip_ds6_reg_t* reg;	
	
	for (reg = uip_ds6_reg_list; 
			reg < uip_ds6_reg_list + UIP_DS6_REG_LIST_SIZE; reg++) {
		if ((reg->isused) && (reg->addr == addr)) {
			if (reg->state != REG_REGISTERED) {
				uip_ds6_reg_rm(reg);
			} else {
				/* Mark it as TO_BE_UNREGISTERED */
				reg->state = REG_TO_BE_UNREGISTERED;
			}
		}
	}
}

/*---------------------------------------------------------------------------*/
/**
 * \brief 				Returns the number of addresses that are registered (or 
 * 								pending to be registered) with a router.
 * 
 * \param defrt 	The router whose number of registrations we want to check.
 * 
 * \returns			 	The number of addresses registered (or pending to be
 * 								registered) with defrt.
 */
 
u8_t uip_ds6_get_registrations(uip_ds6_defrt_t *defrt) {
	
	if ((defrt == NULL) || (!defrt->isused)) {
		return 0;
	}
	
	return defrt->registrations;
}

/*---------------------------------------------------------------------------*/
/**
 * \brief 				Checks whether a NCE can be garbage-collected or not.
 * 
 * \param nbr 		The NCE we want to check.
 * 
 * \returns			 	Returns 1 if nbr can be garbage-collected, 0 otherwise.
 */
 
u8_t uip_ds6_is_nbr_garbage_collectible(uip_ds6_nbr_t *nbr) {
	
	uip_ds6_reg_t* reg;	
	uip_ds6_defrt_t* defrt;
	
	defrt = uip_ds6_defrt_lookup(&nbr->ipaddr);
	
	for (reg = uip_ds6_reg_list; 
			reg < uip_ds6_reg_list + UIP_DS6_REG_LIST_SIZE; reg++) {
		if ((reg->isused) && 
				(reg->defrt == defrt) && 
				(reg->state != REG_GARBAGE_COLLECTIBLE)) {
			return 0;
		}
	}
	return 1;
}

#if CONF_6LOWPAN_ND_6CO
/*---------------------------------------------------------------------------*/
/**
 * \brief							Adds a context to the Context Table
 * 
 * \param length			Length of the prefix
 * 
 * \param context_id	Context Id
 * 
 * \param prefix			Context prefix
 * 
 * \param compression	Is context valid for compression?
 * 
 * \returns						A pointer to the newly created context if the 
 * 										creation was successfull, otherwise NULL.
 *  
 */
uip_ds6_addr_context_t*
uip_ds6_context_add(uip_nd6_opt_6co *context_option,
																						u16_t defrt_lifetime) {
	uip_ds6_addr_context_t* context;	
	
	context = &uip_ds6_addr_context_table[context_option->res1_c_cid & UIP_ND6_RA_CID];
	if(context->state != NOT_IN_USE) {
		/* Context aready exists */
		return NULL;
	}
  context->length = context_option->preflen;
  context->context_id = context_option->res1_c_cid & UIP_ND6_RA_CID;
  uip_ipaddr_copy(&context->prefix, &context_option->prefix);
  if (context_option->res1_c_cid & UIP_ND6_RA_FLAG_COMPRESSION) {
	 	context->state = IN_USE_COMPRESS;
  } else {
  	context->state = IN_USE_UNCOMPRESS_ONLY;
  }
  /* Prevent overflow in case we need to set the lifetime to "twice the
   * Default Router Lifetime" */
  stimer_set(&context->vlifetime, uip_ntohs(context_option->lifetime));
  context->defrt_lifetime = defrt_lifetime < 0x7FFF ? defrt_lifetime : 0x7FFF;
  return context;
}

/*---------------------------------------------------------------------------*/
/**
 * \brief 					Removes a context form the Context table.
 * 
 * \param context 	The context to be removed.
 * 
 */
void 
uip_ds6_context_rm(uip_ds6_addr_context_t *context){
	context->state = NOT_IN_USE;
}

/*---------------------------------------------------------------------------*/
/**
 * \brief 						Searches for a context by context id.
 * 
 * \param context_id 	The context id of the context to search.
 * 
 * \returns			 			If found, returns a pointer to the context. Otherwise 
 * 										returns NULL.
 */
uip_ds6_addr_context_t*
uip_ds6_context_lookup_by_id(u8_t context_id){

	if (uip_ds6_addr_context_table[context_id].state != NOT_IN_USE){
		return &uip_ds6_addr_context_table[context_id]; 
	} else {
		return NULL;
	}
}

/*---------------------------------------------------------------------------*/
/**
 * \brief 						Searches for a context by prefix.
 * 
 * \param length 			The length of the prefix that is valid
 * 
 * \param prefix 			The prefix of the context to search.
 * 
 * \returns			 			If found, returns a pointer to the context. Otherwise 
 * 										returns NULL.
 */
uip_ds6_addr_context_t *uip_ds6_context_lookup_by_prefix(uip_ipaddr_t *prefix) {
	
	uip_ds6_addr_context_t* context;
																							
	for(context = uip_ds6_addr_context_table;
      context < uip_ds6_addr_context_table + SICSLOWPAN_CONF_MAX_ADDR_CONTEXTS; context++) {
    if(context->state != NOT_IN_USE) {
			if (uip_ipaddr_prefixcmp(prefix, &context->prefix, context->length)) {
				return context;
			}
    }
  }
  return NULL;
}



#endif /* CONF_6LOWPAN_ND_6CO */
/*---------------------------------------------------------------------------*/
uip_ds6_defrt_t *
uip_ds6_defrt_add(uip_ipaddr_t *ipaddr, unsigned long interval)
{
  if(uip_ds6_list_loop
     ((uip_ds6_element_t *) uip_ds6_defrt_list, UIP_DS6_DEFRT_NB,
      sizeof(uip_ds6_defrt_t), ipaddr, 128,
      (uip_ds6_element_t **) & locdefrt) == FREESPACE) {
    locdefrt->isused = 1;
    locdefrt->sending_rs = 0;
    locdefrt->rscount = 0;
    uip_ipaddr_copy(&(locdefrt->ipaddr), ipaddr);
    if(interval != 0) {
      stimer_set(&(locdefrt->lifetime), interval);
      locdefrt->isinfinite = 0;
    } else {
      locdefrt->isinfinite = 1;
    }

    PRINTF("Adding defrouter with ip addr");
    PRINT6ADDR(&locdefrt->ipaddr);
    PRINTF("\n");
    return locdefrt;
  }
  return NULL;
}

/*---------------------------------------------------------------------------*/
void
uip_ds6_defrt_rm(uip_ds6_defrt_t * defrt)
{
  if(defrt != NULL) {
    defrt->isused = 0;
  }
  return;
}

/*---------------------------------------------------------------------------*/
uip_ds6_defrt_t *
uip_ds6_defrt_lookup(uip_ipaddr_t * ipaddr)
{
  if(uip_ds6_list_loop((uip_ds6_element_t *) uip_ds6_defrt_list,
		       UIP_DS6_DEFRT_NB, sizeof(uip_ds6_defrt_t), ipaddr, 128,
		       (uip_ds6_element_t **) & locdefrt) == FOUND) {
    return locdefrt;
  }
  return NULL;
}

/*---------------------------------------------------------------------------*/
uip_ipaddr_t *
uip_ds6_defrt_choose(void)
{
  uip_ds6_nbr_t *bestnbr;

  locipaddr = NULL;
  for(locdefrt = uip_ds6_defrt_list;
      locdefrt < uip_ds6_defrt_list + UIP_DS6_DEFRT_NB; locdefrt++) {
    if(locdefrt->isused) {
      PRINTF("Defrt, IP address ");
      PRINT6ADDR(&locdefrt->ipaddr);
      PRINTF("\n");
      bestnbr = uip_ds6_nbr_lookup(&locdefrt->ipaddr);
      if((bestnbr != NULL) && (bestnbr->state != NBR_INCOMPLETE)) {
        PRINTF("Defrt found, IP address ");
        PRINT6ADDR(&locdefrt->ipaddr);
        PRINTF("\n");
        return &locdefrt->ipaddr;
      } else {
        locipaddr = &locdefrt->ipaddr;
        PRINTF("Defrt INCOMPLETE found, IP address ");
        PRINT6ADDR(&locdefrt->ipaddr);
        PRINTF("\n");
      }
    }
  }
  return locipaddr;
}
/*---------------------------------------------------------------------------*/
/**
 * \brief					Returns a default router that meets:
 * 								-	has the minimum number of	registrations
 * 								- addr is not registered with it
 * 
 */ 
uip_ds6_defrt_t*
uip_ds6_defrt_choose_min_reg(uip_ds6_addr_t* addr)
{
	u8_t min = 0;
	uip_ds6_defrt_t* min_defrt = NULL;
	
  for(locdefrt = uip_ds6_defrt_list;
      locdefrt < uip_ds6_defrt_list + UIP_DS6_DEFRT_NB; locdefrt++) {
    if (locdefrt->isused) {
    	if (NULL == uip_ds6_reg_lookup(addr, locdefrt)) {
	      if ((min_defrt == NULL) || 
	      		((min_defrt != NULL) && (uip_ds6_get_registrations(locdefrt) < min))) {
	      	min_defrt = locdefrt;
	      	min = uip_ds6_get_registrations(locdefrt);
	      	if (min == 0) {
	      		/* We are not going to find a better candidate */
	      		return min_defrt;
	      	}		
	      }
    	}  
    }
  }
  return min_defrt;
}

#if UIP_CONF_ROUTER
/*---------------------------------------------------------------------------*/
uip_ds6_prefix_t *
uip_ds6_prefix_add(uip_ipaddr_t * ipaddr, u8_t ipaddrlen,
                   u8_t advertise, u8_t flags, unsigned long vtime,
                   unsigned long ptime)
{
  if(uip_ds6_list_loop
     ((uip_ds6_element_t *) uip_ds6_prefix_list, UIP_DS6_PREFIX_NB,
      sizeof(uip_ds6_prefix_t), ipaddr, ipaddrlen,
      (uip_ds6_element_t **) & locprefix) == FREESPACE) {
    locprefix->isused = 1;
    uip_ipaddr_copy(&(locprefix->ipaddr), ipaddr);
    locprefix->length = ipaddrlen;
    locprefix->advertise = advertise;
    locprefix->l_a_reserved = flags;
    locprefix->vlifetime = vtime;
    locprefix->plifetime = ptime;
    PRINTF("Adding prefix ");
    PRINT6ADDR(&locprefix->ipaddr);
    PRINTF("length %u, flags %x, Valid lifetime %lx, Preffered lifetime %lx\n",
       ipaddrlen, flags, vtime, ptime);
    return locprefix;
  } else {
    PRINTF("No more space in Prefix list\n");
  }
  return NULL;
}


#else /* UIP_CONF_ROUTER */
uip_ds6_prefix_t *
uip_ds6_prefix_add(uip_ipaddr_t * ipaddr, u8_t ipaddrlen,
                   unsigned long interval)
{
  if(uip_ds6_list_loop
     ((uip_ds6_element_t *) uip_ds6_prefix_list, UIP_DS6_PREFIX_NB,
      sizeof(uip_ds6_prefix_t), ipaddr, ipaddrlen,
      (uip_ds6_element_t **) & locprefix) == FREESPACE) {
    locprefix->isused = 1;
    uip_ipaddr_copy(&(locprefix->ipaddr), ipaddr);
    locprefix->length = ipaddrlen;
    if(interval != 0) {
      stimer_set(&(locprefix->vlifetime), interval);
      locprefix->isinfinite = 0;
    } else {
      locprefix->isinfinite = 1;
    }
    PRINTF("Adding prefix ");
    PRINT6ADDR(&locprefix->ipaddr);
    PRINTF("length %u, vlifetime%lu\n", ipaddrlen, interval);
  }
  return NULL;
}
#endif /* UIP_CONF_ROUTER */

/*---------------------------------------------------------------------------*/
void
uip_ds6_prefix_rm(uip_ds6_prefix_t * prefix)
{
  if(prefix != NULL) {
    prefix->isused = 0;
  }
  return;
}
/*---------------------------------------------------------------------------*/
uip_ds6_prefix_t *
uip_ds6_prefix_lookup(uip_ipaddr_t * ipaddr, u8_t ipaddrlen)
{
  if(uip_ds6_list_loop((uip_ds6_element_t *)uip_ds6_prefix_list,
		       UIP_DS6_PREFIX_NB, sizeof(uip_ds6_prefix_t),
		       ipaddr, ipaddrlen,
		       (uip_ds6_element_t **)&locprefix) == FOUND) {
    return locprefix;
  }
  return NULL;
}

/*---------------------------------------------------------------------------*/
u8_t
uip_ds6_is_addr_onlink(uip_ipaddr_t * ipaddr)
{
/* 
 * I-D.ietf-6lowpan-nd sections 5.6 - 5.7: all prefixes but the link-local 
 * prefix are always assumed to be off-link.
 */
  return uip_is_addr_link_local(ipaddr);
}

/*---------------------------------------------------------------------------*/
uip_ds6_addr_t *
uip_ds6_addr_add(uip_ipaddr_t * ipaddr, unsigned long vlifetime, u8_t type)
{
  if(uip_ds6_list_loop
     ((uip_ds6_element_t *) uip_ds6_if.addr_list, UIP_DS6_ADDR_NB,
      sizeof(uip_ds6_addr_t), ipaddr, 128,
      (uip_ds6_element_t **) & locaddr) == FREESPACE) {
    locaddr->isused = 1;
    uip_ipaddr_copy(&locaddr->ipaddr, ipaddr);
	if (uip_is_addr_link_local(ipaddr)) {
		locaddr->state = ADDR_PREFERRED;
	} else {
		locaddr->state = ADDR_TENTATIVE;	
	}
    locaddr->type = type;
    if(vlifetime == 0) {
      locaddr->isinfinite = 1;
    } else {
      locaddr->isinfinite = 0;
      stimer_set(&locaddr->vlifetime, vlifetime);
    }
#if UIP_CONF_ROUTER
	/* 
	 * If 6LoWPAN-ND optimizations are implemented, hosts do not join the 
	 * Solicited-node multicast address.
	 */
	uip_create_solicited_node(ipaddr, &loc_fipaddr);
  uip_ds6_maddr_add(&loc_fipaddr);
#endif /* UIP_CONF_ROUTER */

    return locaddr;
  }
  return NULL;
}

/*---------------------------------------------------------------------------*/
void
uip_ds6_addr_rm(uip_ds6_addr_t * addr)
{
  if(addr != NULL) {
    addr->isused = 0;
  }
  return;
}

/*---------------------------------------------------------------------------*/
uip_ds6_addr_t *
uip_ds6_addr_lookup(uip_ipaddr_t * ipaddr)
{
  if(uip_ds6_list_loop
     ((uip_ds6_element_t *) uip_ds6_if.addr_list, UIP_DS6_ADDR_NB,
      sizeof(uip_ds6_addr_t), ipaddr, 128,
      (uip_ds6_element_t **) & locaddr) == FOUND) {
    return locaddr;
  }
  return NULL;
}

/*---------------------------------------------------------------------------*/
/*
 * get a link local address -
 * state = -1 => any address is ok. Otherwise state = desired state of addr.
 * (TENTATIVE, PREFERRED, DEPRECATED)
 */
uip_ds6_addr_t *
uip_ds6_get_link_local(signed char state)
{
  for(locaddr = uip_ds6_if.addr_list;
      locaddr < uip_ds6_if.addr_list + UIP_DS6_ADDR_NB; locaddr++) {
    if((locaddr->isused) && (state == -1 || locaddr->state == state)
       && (uip_is_addr_link_local(&locaddr->ipaddr))) {
      return locaddr;
    }
  }
  return NULL;
}

/*---------------------------------------------------------------------------*/
/*
 * get a global address -
 * state = -1 => any address is ok. Otherwise state = desired state of addr.
 * (TENTATIVE, PREFERRED, DEPRECATED)
 */
uip_ds6_addr_t *
uip_ds6_get_global(signed char state)
{
  for(locaddr = uip_ds6_if.addr_list;
      locaddr < uip_ds6_if.addr_list + UIP_DS6_ADDR_NB; locaddr++) {
    if((locaddr->isused) && (state == -1 || locaddr->state == state)
       && !(uip_is_addr_link_local(&locaddr->ipaddr))) {
      return locaddr;
    }
  }
  return NULL;
}

/*---------------------------------------------------------------------------*/
uip_ds6_maddr_t *
uip_ds6_maddr_add(uip_ipaddr_t * ipaddr)
{
  if(uip_ds6_list_loop
     ((uip_ds6_element_t *) uip_ds6_if.maddr_list, UIP_DS6_MADDR_NB,
      sizeof(uip_ds6_maddr_t), ipaddr, 128,
      (uip_ds6_element_t **) & locmaddr) == FREESPACE) {
    locmaddr->isused = 1;
    uip_ipaddr_copy(&locmaddr->ipaddr, ipaddr);
    return locmaddr;
  }
  return NULL;
}

/*---------------------------------------------------------------------------*/
void
uip_ds6_maddr_rm(uip_ds6_maddr_t * maddr)
{
  if(maddr != NULL) {
    maddr->isused = 0;
  }
  return;
}

/*---------------------------------------------------------------------------*/
uip_ds6_maddr_t *
uip_ds6_maddr_lookup(uip_ipaddr_t * ipaddr)
{
  if(uip_ds6_list_loop
     ((uip_ds6_element_t *) uip_ds6_if.maddr_list, UIP_DS6_MADDR_NB,
      sizeof(uip_ds6_maddr_t), ipaddr, 128,
      (uip_ds6_element_t **) & locmaddr) == FOUND) {
    return locmaddr;
  }
  return NULL;
}


/*---------------------------------------------------------------------------*/
#if UIP_DS6_AADDR_NB > 0 /* Look uip-ds6.h, line 341 */
uip_ds6_aaddr_t *
uip_ds6_aaddr_add(uip_ipaddr_t * ipaddr)
{
  if(uip_ds6_list_loop
     ((uip_ds6_element_t *) uip_ds6_if.aaddr_list, UIP_DS6_AADDR_NB,
      sizeof(uip_ds6_aaddr_t), ipaddr, 128,
      (uip_ds6_element_t **) & locaaddr) == FREESPACE) {
    locaaddr->isused = 1;
    uip_ipaddr_copy(&locaaddr->ipaddr, ipaddr);
    return locaaddr;
  }
  return NULL;
}
/*---------------------------------------------------------------------------*/
void
uip_ds6_aaddr_rm(uip_ds6_aaddr_t * aaddr)
{
  if(aaddr != NULL) {
    aaddr->isused = 0;
  }
  return;
}

/*---------------------------------------------------------------------------*/
uip_ds6_aaddr_t *
uip_ds6_aaddr_lookup(uip_ipaddr_t * ipaddr)
{
  if(uip_ds6_list_loop((uip_ds6_element_t *) uip_ds6_if.aaddr_list,
		       UIP_DS6_AADDR_NB, sizeof(uip_ds6_aaddr_t), ipaddr, 128,
		       (uip_ds6_element_t **)&locaaddr) == FOUND) {
    return locaaddr;
  }
  return NULL;
}

#endif /* UIP_DS6_AADDR_NB > 0 */
/*---------------------------------------------------------------------------*/
uip_ds6_route_t *
uip_ds6_route_lookup(uip_ipaddr_t * destipaddr)
{
  uip_ds6_route_t *locrt = NULL;
  u8_t longestmatch = 0;

  PRINTF("DS6: Looking up route for");
  PRINT6ADDR(destipaddr);
  PRINTF("\n");

  for(locroute = uip_ds6_routing_table;
      locroute < uip_ds6_routing_table + UIP_DS6_ROUTE_NB; locroute++) {
    if((locroute->isused) && (locroute->length >= longestmatch)
       &&
       (uip_ipaddr_prefixcmp
        (destipaddr, &locroute->ipaddr, locroute->length))) {
      longestmatch = locroute->length;
      locrt = locroute;
    }
  }

  if(locrt != NULL) {
    PRINTF("DS6: Found route:");
    PRINT6ADDR(destipaddr);
    PRINTF(" via ");
    PRINT6ADDR(&locrt->nexthop);
    PRINTF("\n");
  } else {
    PRINTF("DS6: No route found ...\n");
  }

  return locrt;
}

/*---------------------------------------------------------------------------*/
uip_ds6_route_t *
uip_ds6_route_add(uip_ipaddr_t * ipaddr, u8_t length, uip_ipaddr_t * nexthop,
                  u8_t metric)
{

  if(uip_ds6_list_loop
     ((uip_ds6_element_t *) uip_ds6_routing_table, UIP_DS6_ROUTE_NB,
      sizeof(uip_ds6_route_t), ipaddr, length,
      (uip_ds6_element_t **) & locroute) == FREESPACE) {
    locroute->isused = 1;
    uip_ipaddr_copy(&(locroute->ipaddr), ipaddr);
    locroute->length = length;
    uip_ipaddr_copy(&(locroute->nexthop), nexthop);
    locroute->metric = metric;

    PRINTF("DS6: adding route:");
    PRINT6ADDR(ipaddr);
    PRINTF(" via ");
    PRINT6ADDR(nexthop);
    PRINTF("\n");

  }

  return locroute;
}

/*---------------------------------------------------------------------------*/
void
uip_ds6_route_rm(uip_ds6_route_t *route)
{
  route->isused = 0;
}
/*---------------------------------------------------------------------------*/
void
uip_ds6_route_rm_by_nexthop(uip_ipaddr_t *nexthop)
{
  for(locroute = uip_ds6_routing_table;
      locroute < uip_ds6_routing_table + UIP_DS6_ROUTE_NB; locroute++) {
    if((locroute->isused) && uip_ipaddr_cmp(&locroute->nexthop, nexthop)) {
      locroute->isused = 0;
    }
  }
}

/*---------------------------------------------------------------------------*/
void
uip_ds6_select_src(uip_ipaddr_t *src, uip_ipaddr_t *dst)
{
  u8_t best = 0;             /* number of bit in common with best match */
  u8_t n = 0;
  uip_ds6_addr_t *matchaddr = NULL;

  if(!uip_is_addr_link_local(dst) && !uip_is_addr_mcast(dst)) {
    /* find longest match */
    for(locaddr = uip_ds6_if.addr_list;
        locaddr < uip_ds6_if.addr_list + UIP_DS6_ADDR_NB; locaddr++) {
      /* Only preferred global (not link-local) addresses */
      if((locaddr->isused) && (locaddr->state == ADDR_PREFERRED) &&
         (!uip_is_addr_link_local(&locaddr->ipaddr))) {
        n = get_match_length(dst, &(locaddr->ipaddr));
        if(n >= best) {
          best = n;
          matchaddr = locaddr;
        }
      }
    }
  } else {
    matchaddr = uip_ds6_get_link_local(ADDR_PREFERRED);
  }

  /* use the :: (unspecified address) as source if no match found */
  if(matchaddr == NULL) {
    uip_create_unspecified(src);
  } else {
    uip_ipaddr_copy(src, &matchaddr->ipaddr);
  }
}

/*---------------------------------------------------------------------------*/
void
uip_ds6_set_addr_iid(uip_ipaddr_t * ipaddr, uip_lladdr_t * lladdr)
{
  /* We consider only links with IEEE EUI-64 identifier or
   * IEEE 48-bit MAC addresses */
#if (UIP_LLADDR_LEN == 8)
  memcpy(ipaddr->u8 + 8, lladdr, UIP_LLADDR_LEN);
  ipaddr->u8[8] ^= 0x02;
#elif (UIP_LLADDR_LEN == 6)
  memcpy(ipaddr->u8 + 8, lladdr, 3);
  ipaddr->u8[11] = 0xff;
  ipaddr->u8[12] = 0xfe;
  memcpy(ipaddr->u8 + 13, (u8_t *) lladdr + 3, 3);
  ipaddr->u8[8] ^= 0x02;
#else
#error uip-ds6.c cannot build interface address when UIP_LLADDR_LEN is not 6 or 8
#endif
}

/*---------------------------------------------------------------------------*/
u8_t
get_match_length(uip_ipaddr_t * src, uip_ipaddr_t * dst)
{
  u8_t j, k, x_or;
  u8_t len = 0;

  for(j = 0; j < 16; j++) {
    if(src->u8[j] == dst->u8[j]) {
      len += 8;
    } else {
      x_or = src->u8[j] ^ dst->u8[j];
      for(k = 0; k < 8; k++) {
        if((x_or & 0x80) == 0) {
          len++;
          x_or <<= 1;
        } else {
          break;
        }
      }
      break;
    }
  }
  return len;
}

/*---------------------------------------------------------------------------*/

#if UIP_CONF_ROUTER
#if UIP_ND6_SEND_RA
/*---------------------------------------------------------------------------*/
void
uip_ds6_send_ra_solicited(void)
{
  /* We have a pb here: RA timer max possible value is 1800s,
   * hence we have to use stimers. However, when receiving a RS, we
   * should delay the reply by a random value between 0 and 500ms timers.
   * stimers are in seconds, hence we cannot do this. Therefore we just send
   * the RA (setting the timer to 0 below). We keep the code logic for
   * the days contiki will support appropriate timers */
  rand_time = 0;
  PRINTF("Solicited RA, random time %u\n", rand_time);

  if(stimer_remaining(&uip_ds6_timer_ra) > rand_time) {
    if(stimer_elapsed(&uip_ds6_timer_ra) < UIP_ND6_MIN_DELAY_BETWEEN_RAS) {
      /* Ensure that the RAs are rate limited */
/*      stimer_set(&uip_ds6_timer_ra, rand_time +
                 UIP_ND6_MIN_DELAY_BETWEEN_RAS -
                 stimer_elapsed(&uip_ds6_timer_ra));
  */ } else {
      stimer_set(&uip_ds6_timer_ra, rand_time);
    }
  }
}

/*---------------------------------------------------------------------------*/
void
uip_ds6_send_ra_periodic(void)
{
  if(racount > 0) {
    /* send previously scheduled RA */
    uip_nd6_ra_output(NULL);
    PRINTF("Sending periodic RA\n");
  }

  rand_time = UIP_ND6_MIN_RA_INTERVAL + random_rand() %
    (u16_t) (UIP_ND6_MAX_RA_INTERVAL - UIP_ND6_MIN_RA_INTERVAL);
  PRINTF("Random time 1 = %u\n", rand_time);

  if(racount < UIP_ND6_MAX_INITIAL_RAS) {
    if(rand_time > UIP_ND6_MAX_INITIAL_RA_INTERVAL) {
      rand_time = UIP_ND6_MAX_INITIAL_RA_INTERVAL;
      PRINTF("Random time 2 = %u\n", rand_time);
    }
    racount++;
  }
  PRINTF("Random time 3 = %u\n", rand_time);
  stimer_set(&uip_ds6_timer_ra, rand_time);
}

#endif /* UIP_ND6_SEND_RA */
#else /* UIP_CONF_ROUTER */
/*---------------------------------------------------------------------------*/

/**
 * This function calculates the c-th term of the binary exponential backoff. 
 * The result is multiplied by factor k and truncated to trunc if greater.
 */
u16_t
beb_next(u16_t c, u16_t k, u16_t trunc)
{
	u16_t result;
	
	result = (random_rand() % (2<<(c - 1) - 1)) * k;
	return (result < trunc ? result : trunc);
}

/**
 * Returns the retransmission interval for a certain retransmission attempt
 * as specified in I.D.ietf-6lowpan-nd.
 */ 
u16_t 
rs_rtx_time(u16_t rtx_count) 
{
	if (rtx_count < UIP_ND6_MAX_RTR_SOLICITATIONS) {
		return UIP_ND6_RTR_SOLICITATION_INTERVAL;
	} else if (rtx_count > 10){
		return UIP_ND6_MAX_RTR_SOLICITATION_INTERVAL;
	} else {
		/* Do binary exponential backoff */
		return beb_next(rtx_count, 
				 		UIP_ND6_RTR_SOLICITATION_INTERVAL, 
				 		UIP_ND6_MAX_RTR_SOLICITATION_INTERVAL);
	}
}

void
uip_ds6_send_rs(uip_ds6_defrt_t *defrt)
{
  u8_t unicast_rs = 0;

	if (!timer_expired(&uip_ds6_timer_rs)) {
		return;
	}
	locdefrt = NULL;
	/* First check whether we can unicast the RS to a specific router rather than
	 * multicasting it. */
	if ((defrt != NULL) && (defrt->isused)){
	  /* Mark this router as "sending_rs" in case it wasn't marked already */
	  defrt->sending_rs = 1;
	  locdefrt = defrt;
	  unicast_rs = 1;
  } else {
    for(locdefrt = uip_ds6_defrt_list;
        locdefrt < uip_ds6_defrt_list + UIP_DS6_DEFRT_NB; locdefrt++) {
      if((locdefrt->isused) && (locdefrt->sending_rs)) {
        unicast_rs = 1;
        break;
      }
    }
  }

	if (unicast_rs) {
    if (locdefrt->rscount < UIP_ND6_MAX_RTR_SOLICITATIONS) {
      /* Unicast RS and update count and timer */
      uip_nd6_rs_output(&locdefrt->ipaddr);
      locdefrt->rscount = locdefrt->rscount > 10 ? locdefrt->rscount : locdefrt->rscount + 1;
      timer_set(&uip_ds6_timer_rs, rs_rtx_time(locdefrt->rscount) * CLOCK_SECOND);
      return;
    } else {
      /* Switch to multicast */
      locdefrt->sending_rs = 0;
      rscount = locdefrt->rscount;
      locdefrt->rscount = 0;
    }
	}
	/* Multicast RS and update RS count and timer */
	uip_nd6_rs_output(NULL);
	if(uip_ds6_defrt_choose() == NULL) {
	 	rscount = rscount > 10 ? rscount : rscount + 1;
	} else {
   	rscount = 0;
  }
  /* Make sure we do not send rs more frequently than UIP_ND6_RTR_SOLICITATION_INTERVAL */
	timer_set(&uip_ds6_timer_rs, rs_rtx_time(rscount) * CLOCK_SECOND);
}

#endif /* UIP_CONF_ROUTER */
/*---------------------------------------------------------------------------*/
u32_t
uip_ds6_compute_reachable_time(void)
{
  return (u32_t) (UIP_ND6_MIN_RANDOM_FACTOR
                     (uip_ds6_if.base_reachable_time)) +
    ((u16_t) (random_rand() << 8) +
     (u16_t) random_rand()) %
    (u32_t) (UIP_ND6_MAX_RANDOM_FACTOR(uip_ds6_if.base_reachable_time) -
                UIP_ND6_MIN_RANDOM_FACTOR(uip_ds6_if.base_reachable_time));
}


/** @} */
