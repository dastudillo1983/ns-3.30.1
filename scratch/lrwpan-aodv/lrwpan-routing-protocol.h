/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2009 University of Washington
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#ifndef LRWPAN_ROUTING_PROTOCOL_H
#define LRWPAN_ROUTING_PROTOCOL_H

#include "ns3/packet.h"
#include "ns3/callback.h"
#include "ns3/object.h"
#include "ns3/socket.h"
#include "ns3/lr-wpan-mac-header.h"
#include "ns3/ipv4-interface-address.h"
#include "ns3/output-stream-wrapper.h"
#include "ns3/nstime.h"

namespace ns3 {

//class Ipv4MulticastRoute;
class LrWpanRoute;
class NetDevice;
class LrWpan;

/**
 * \ingroup internet
 * \defgroup ipv4Routing IPv4 Routing Protocols.
 *
 * The classes in this group implement different routing protocols
 * for IPv4. Other modules could implement further protocols
 * (e.g., AODV, OLSR, etc.).
 */

/**
 * \ingroup ipv4Routing
 * \brief Abstract base class for IPv4 routing protocols. 
 * 
 * Defines two virtual functions for packet routing and forwarding.  The first, 
 * RouteOutput(), is used for locally originated packets, and the second,
 * RouteInput(), is used for forwarding and/or delivering received packets. 
 * Also defines the signatures of four callbacks used in RouteInput().
 *
 */
class LrWpanRoutingProtocol : public Object
{
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);

  /// Callback for unicast packets to be forwarded
  typedef Callback<void, Ptr<LrWpanRoute>, Ptr<const Packet>, const LrWpanMacHeader &> UnicastForwardCallback;

  /// Callback for routing errors (e.g., no route found)
  typedef Callback<void, Ptr<const Packet>, const LrWpanMacHeader &, Socket::SocketErrno > ErrorCallback;

  /**
   * \brief Query routing cache for an existing route, for an outbound packet
   *
   * This lookup is used by transport protocols.  It does not cause any
   * packet to be forwarded, and is synchronous.  Can be used for
   * multicast or unicast.  The Linux equivalent is ip_route_output()
   *
   * The header input parameter may have an uninitialized value
   * for the source address, but the destination address should always be 
   * properly set by the caller.
   *
   * \param p packet to be routed.  Note that this method may modify the packet.
   *          Callers may also pass in a null pointer. 
   * \param header input parameter (used to form key to search for the route)
   * \param oif Output interface Netdevice.  May be zero, or may be bound via
   *            socket options to a particular output interface.
   * \param sockerr Output parameter; socket errno 
   *
   * \returns a code that indicates what happened in the lookup
   */
  virtual Ptr<LrWpanRoute> RouteOutput (Ptr<Packet> p, const LrWpanMacHeader &header, Ptr<NetDevice> oif, Socket::SocketErrno &sockerr) = 0;

  /**
   * \brief Route an input packet (to be forwarded or locally delivered)
   *
   * This lookup is used in the forwarding process.  The packet is
   * handed over to the LrWpanRoutingProtocol, and will get forwarded onward
   * by one of the callbacks.  The Linux equivalent is ip_route_input().
   * There are four valid outcomes, and a matching callbacks to handle each.
   *
   * \param p received packet
   * \param header input parameter used to form a search key for a route
   * \param idev Pointer to ingress network device
   * \param ucb Callback for the case in which the packet is to be forwarded
   *            as unicast
   * \param mcb Callback for the case in which the packet is to be forwarded
   *            as multicast
   * \param lcb Callback for the case in which the packet is to be locally
   *            delivered
   * \param ecb Callback to call if there is an error in forwarding
   * \returns true if the LrWpanRoutingProtocol takes responsibility for
   *          forwarding or delivering the packet, false otherwise
   */ 
  virtual bool RouteInput  (Ptr<const Packet> p, const LrWpanMacHeader &header, Ptr<const NetDevice> idev,
                            UnicastForwardCallback ucb,
                            ErrorCallback ecb) = 0;

  /**
   * \param ipv4 the ipv4 object this routing protocol is being associated with
   * 
   * Typically, invoked directly or indirectly from ns3::Ipv4::SetRoutingProtocol
   */
  //virtual void SetLrWpan (Ptr<LrWpan> lrwpan) = 0;

  /**
   * \brief Print the Routing Table entries
   *
   * \param stream The ostream the Routing table is printed to
   * \param unit The time unit to be used in the report
   */
  virtual void PrintRoutingTable (Ptr<OutputStreamWrapper> stream, Time::Unit unit = Time::S) const = 0;

  const Ptr<Node>& GetNode() const;

  void SetNode(const Ptr<Node> &node);

private:

  Ptr<Node> m_node;                                     ///< The node ptr

};

} // namespace ns3

#endif /* LRWPAN_ROUTING_PROTOCOL_H */
