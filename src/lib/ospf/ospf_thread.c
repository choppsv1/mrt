/* 
 * $Id: ospf_thread.c,v 1.1.1.1 2000/08/14 18:46:12 labovit Exp $
 */


#include <config.h>
#include <stdio.h>
#ifndef NT
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#else
#include <winsock2.h>
#ifdef HAVE_IPV6
#include <ws2ip6.h>
#endif /* HAVE_IPV6 */ 
#include <ws2tcpip.h>
#endif /* NT */

#include <mrt.h>
#include <select.h>
#include <interface.h>
#include <io.h>
#include <ospf_proto.h>


char *ospf_states[] = {"DOWN", "ATTEMPT",
		       "INIT", "2WAY", 
		       "EXSTART", "EXCHANGE",
		       "LOADING", "FULL"};

char *ospf_events[] = {"START", "HELLORECEIVED", "2WAYRECEIVED",
		       "NEGOTIATIONDONE", "EXCHANGEDONE",
		       "BADLSREQ", "LOADINGDONE", "ADJOK"};


//static void _start_ospf_thread () {
//#ifndef NT
//  sigset_t set;
 // sigemptyset (&set);
 // sigaddset (&set, SIGALRM);
//  sigaddset (&set, SIGHUP);
//  pthread_sigmask (SIG_BLOCK, &set, NULL);
//#endif /* NT */

//#ifdef HAVE_LIBPTHREAD
//  while (1) {
//    schedule_wait_for_event (OSPF.schedule);
 // }
//#endif /* HAVE_LIBPTHREAD */
 // return;
//}



//void start_ospf_thread () {
 // OSPF.schedule = New_Schedule ("opsf_schedule", OSPF.trace);

 // mrt_thread_create ("OSPF Thread", OSPF.schedule, 
//		     (void *) _start_ospf_thread, OSPF.trace);
//}


int create_ospf_socket () {
  interface_t *interface;
 
  prefix_t *multicast_prefix;
  ospf_interface_t *ospf_interface;
  struct ip_mreq mreq;
	//join_leave_group

  if ((OSPF.fd = socket (AF_INET, SOCK_RAW, IPPROTO_OSPF)) < 0) {
    trace (NORM, OSPF.trace, "Error creating socket: %s\n", strerror (errno));
    return (-1);
  }

  multicast_prefix = ascii2prefix (AF_INET, "224.0.0.5");
  memcpy (&mreq.imr_multiaddr.s_addr, prefix_tochar (multicast_prefix), 4);

  /* join all of the OSPF-ALL.MCAST.NET (224.0.0.5) multicast group */
  /*LL_Iterate (INTERFACE_MASTER->ll_interfaces, interface) {*/
  LL_Iterate (OSPF.ll_ospf_interfaces, ospf_interface) {
    interface = ospf_interface->interface;

    if ((interface == NULL) || (interface->primary->prefix == NULL)) continue;

    memcpy (&mreq.imr_interface.s_addr, prefix_tochar (interface->primary->prefix), 4);
    trace (NORM, OSPF.trace, "Join %s to %s\n", 
	   interface->name, prefix_toa (multicast_prefix));

    if (setsockopt (OSPF.fd, IPPROTO_IP, 
		    IP_ADD_MEMBERSHIP, (char *) &mreq, sizeof (mreq)) < 0) {
      trace (NORM, OSPF.trace, "Error joining mulitcast for %s: %s\n", 
	     interface->name, 
	     strerror (errno));
    }
  }
  
  
  /* have MRT tell us when there is something to read on the socket */
  select_add_fd (OSPF.fd, SELECT_READ, (void_fn_t) ospf_read_packet, NULL);
  return (1);
}



void ospf_init (trace_t *tr) {

  OSPF.trace = trace_copy (tr);
  pthread_mutex_init (&OSPF.mutex_lock, NULL);

  OSPF.router_id = prefix_tolong (INTERFACE_MASTER->default_interface->primary->prefix);
  OSPF.default_hello_interval = OSPF_DEFAULT_HELLO_INTERVAL;
  OSPF.default_dead_interval = OSPF_DEFAULT_DEAD_INTERVAL;

  OSPF.ll_ospf_interfaces = LL_Create (0);
  OSPF.ll_ospf_areas = LL_Create (0);

  OSPF.ll_router_lsas = LL_Create (LL_DestroyFunction,
				   ospf_destroy_router_lsa, NULL);
  OSPF.ll_network_lsas = LL_Create (LL_DestroyFunction,
				    ospf_destroy_network_lsa, NULL);
  OSPF.ll_summary_lsas = LL_Create (LL_DestroyFunction,
				    ospf_destroy_summary_lsa, NULL);
  OSPF.ll_external_lsas = LL_Create (LL_DestroyFunction,
				     ospf_destroy_external_lsa, NULL);

	OSPF.schedule = New_Schedule ("opsf_schedule", OSPF.trace);
  //init_ospf_config ();
	mrt_thread_create2 ("OSPF", OSPF.schedule, NULL, NULL);
  return;

}

