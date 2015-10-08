/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2015  ZeroTier, Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * --
 *
 * ZeroTier may be used and distributed under the terms of the GPLv3, which
 * are available at: http://www.gnu.org/licenses/gpl-3.0.html
 *
 * If you would like to embed ZeroTier into a commercial application or
 * redistribute it in a modified binary form, please contact ZeroTier Networks
 * LLC. Start here: http://www.zerotier.com/
 */

#ifndef ZT_NETCONETHERNETTAP_HPP
#define ZT_NETCONETHERNETTAP_HPP

#ifdef ZT_ENABLE_NETCON

#include <stdio.h>
#include <stdlib.h>

#include <string>
#include <vector>
#include <stdexcept>

#include "../node/Constants.hpp"
#include "../node/MulticastGroup.hpp"
#include "../node/Mutex.hpp"
#include "../node/InetAddress.hpp"
#include "../osdep/Thread.hpp"
#include "../osdep/Phy.hpp"

#include "NetconService.hpp"
#include "NetconUtilities.hpp"

#include "netif/etharp.h"

namespace ZeroTier {

class NetconEthernetTap;

/**
 * Network Containers instance -- emulates an Ethernet tap device as far as OneService knows
 */
class NetconEthernetTap
{
	friend class Phy<NetconEthernetTap *>;

public:
	NetconEthernetTap(
		const char *homePath,
		const MAC &mac,
		unsigned int mtu,
		unsigned int metric,
		uint64_t nwid,
		const char *friendlyName,
		void (*handler)(void *,uint64_t,const MAC &,const MAC &,unsigned int,unsigned int,const void *,unsigned int),
		void *arg);

	~NetconEthernetTap();

	void setEnabled(bool en);
	bool enabled() const;
	bool addIp(const InetAddress &ip);
	bool removeIp(const InetAddress &ip);
	std::vector<InetAddress> ips() const;
	void put(const MAC &from,const MAC &to,unsigned int etherType,const void *data,unsigned int len);
	std::string deviceName() const;
	void setFriendlyName(const char *friendlyName);
	void scanMulticastGroups(std::vector<MulticastGroup> &added,std::vector<MulticastGroup> &removed);

	void threadMain()
		throw();

	LWIPStack *lwipstack;
  uint64_t _nwid;
  void (*_handler)(void *,uint64_t,const MAC &,const MAC &,unsigned int,unsigned int,const void *,unsigned int);
  void *_arg;

private:

	// LWIP callbacks
	static err_t nc_poll(void* arg, struct tcp_pcb *tpcb);
	static err_t nc_accept(void *arg, struct tcp_pcb *newpcb, err_t err);
	static err_t nc_recved(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err);
	static void nc_err(void *arg, err_t err);
	static void nc_close(struct tcp_pcb *tpcb);
	static err_t nc_send(struct tcp_pcb *tpcb);
	static err_t nc_sent(void *arg, struct tcp_pcb *tpcb, u16_t len);
	static err_t nc_connected(void *arg, struct tcp_pcb *tpcb, err_t err);

	// RPC handlers (from NetconIntercept)
	void handle_bind(PhySocket *sock, void **uptr, struct bind_st *bind_rpc);
	void handle_listen(PhySocket *sock, void **uptr, struct listen_st *listen_rpc);
	void handle_retval(PhySocket *sock, void **uptr, unsigned char* buf);
	void handle_socket(PhySocket *sock, void **uptr, struct socket_st* socket_rpc);
	void handle_connect(PhySocket *sock, void **uptr, struct connect_st* connect_rpc);
	void handle_write(TcpConnection *conn);

	int send_return_value(TcpConnection *conn, int retval);

	void phyOnDatagram(PhySocket *sock,void **uptr,const struct sockaddr *from,void *data,unsigned long len);
	void phyOnTcpConnect(PhySocket *sock,void **uptr,bool success);
	void phyOnTcpAccept(PhySocket *sockL,PhySocket *sockN,void **uptrL,void **uptrN,const struct sockaddr *from);
	void phyOnTcpClose(PhySocket *sock,void **uptr);
	void phyOnTcpData(PhySocket *sock,void **uptr,void *data,unsigned long len);
	void phyOnTcpWritable(PhySocket *sock,void **uptr);
	void phyOnUnixAccept(PhySocket *sockL,PhySocket *sockN,void **uptrL,void **uptrN);
	void phyOnUnixClose(PhySocket *sock,void **uptr);
	void phyOnUnixData(PhySocket *sock,void **uptr,void *data,unsigned long len);
	void phyOnFileDescriptorActivity(PhySocket *sock,void **uptr,bool readable,bool writable);


	ip_addr_t convert_ip(struct sockaddr_in * addr)
	{
	  ip_addr_t conn_addr;
	  struct sockaddr_in *ipv4 = addr;
	  short a = ip4_addr1(&(ipv4->sin_addr));
	  short b = ip4_addr2(&(ipv4->sin_addr));
	  short c = ip4_addr3(&(ipv4->sin_addr));
	  short d = ip4_addr4(&(ipv4->sin_addr));
	  IP4_ADDR(&conn_addr, a,b,c,d);
	  return conn_addr;
	}

	// Client helpers
	TcpConnection *getConnectionByTheirFD(int fd);
	TcpConnection *getConnectionByPCB(struct tcp_pcb *pcb);
	void closeConnection(TcpConnection *conn);
	void closeAll();

	void closeClient(PhySocket *sock);

	Phy<NetconEthernetTap *> _phy;
	PhySocket *_unixListenSocket;

	std::vector<TcpConnection*> tcp_connections;
	std::vector<PhySocket*> rpc_sockets;

	netif interface;

	MAC _mac;
	Thread _thread;
	std::string _homePath;
	std::string _dev; // path to Unix domain socket

	std::vector<MulticastGroup> _multicastGroups;
	Mutex _multicastGroups_m;

	std::vector<InetAddress> _ips;
	Mutex _ips_m;

	unsigned int _mtu;
	volatile bool _enabled;
	volatile bool _run;
};


/*------------------------------------------------------------------------------
------------------------ low-level Interface functions -------------------------
------------------------------------------------------------------------------*/

static err_t low_level_output(struct netif *netif, struct pbuf *p);

static err_t tapif_init(struct netif *netif)
{
  // Actual init functionality is in addIp() of tap
  return ERR_OK;
}

static err_t low_level_output(struct netif *netif, struct pbuf *p)
{
	//fprintf(stderr, "low_level_output()\n");
  struct pbuf *q;
  char buf[ZT_MAX_MTU+32];
  char *bufptr;
  int tot_len = 0;

  ZeroTier::NetconEthernetTap *tap = (ZeroTier::NetconEthernetTap*)netif->state;

  /* initiate transfer(); */
  bufptr = buf;

  for(q = p; q != NULL; q = q->next) {
    /* Send the data from the pbuf to the interface, one pbuf at a
       time. The size of the data in each pbuf is kept in the ->len
       variable. */
    /* send data from(q->payload, q->len); */
    memcpy(bufptr, q->payload, q->len);
    bufptr += q->len;
    tot_len += q->len;
  }

  // [Send packet to network]
  // Split ethernet header and feed into handler
  struct eth_hdr *ethhdr;
  ethhdr = (struct eth_hdr *)buf;

  ZeroTier::MAC src_mac;
  ZeroTier::MAC dest_mac;

  src_mac.setTo(ethhdr->src.addr, 6);
  dest_mac.setTo(ethhdr->dest.addr, 6);

  tap->_handler(tap->_arg,tap->_nwid,src_mac,dest_mac,
    Utils::ntoh((uint16_t)ethhdr->type),0,buf + sizeof(struct eth_hdr),tot_len - sizeof(struct eth_hdr));
	//printf("low_level_output(): length = %d -- ethertype = %d\n", tot_len - sizeof(struct eth_hdr), Utils::ntoh((uint16_t)ethhdr->type));
  return ERR_OK;
}

} // namespace ZeroTier

#endif // ZT_ENABLE_NETCON

#endif
