/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2010 Hemanth Narra, Yufei Cheng
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
 *
 * Author: Hemanth Narra <hemanth@ittc.ku.com>
 * Author: Yufei Cheng   <yfcheng@ittc.ku.edu>
 *
 * James P.G. Sterbenz <jpgs@ittc.ku.edu>, director
 * ResiliNets Research Group  http://wiki.ittc.ku.edu/resilinets
 * Information and Telecommunication Technology Center (ITTC)
 * and Department of Electrical Engineering and Computer Science
 * The University of Kansas Lawrence, KS USA.
 *
 * Work supported in part by NSF FIND (Future Internet Design) Program
 * under grant CNS-0626918 (Postmodern Internet Architecture),
 * NSF grant CNS-1050226 (Multilayer Network Resilience Analysis and Experimentation on GENI),
 * US Department of Defense (DoD), and ITTC at The University of Kansas.
 */

#ifndef DSDV6_ROUTING_PROTOCOL_H
#define DSDV6_ROUTING_PROTOCOL_H

#include "dsdv6-rtable.h"
#include "dsdv6-packet-queue.h"
#include "dsdv6-packet.h"
#include "ns3/node.h"
#include "ns3/random-variable-stream.h"
#include "ns3/ipv6-routing-protocol.h"
#include "ns3/ipv6-interface.h"
#include "ns3/ipv6-l3-protocol.h"
#include "ns3/output-stream-wrapper.h"

namespace ns3 {
namespace dsdv6 {

/**
 * \ingroup dsdv6
 * \brief DSDV6 routing protocol.
 */
class RoutingProtocol : public Ipv6RoutingProtocol
{
public:
  static TypeId
  GetTypeId (void);
  static const uint32_t DSDV6_PORT;

  /// c-tor
  RoutingProtocol ();
  virtual
  ~RoutingProtocol ();
  virtual void
  DoDispose ();

  // From Ipv6RoutingProtocol
  Ptr<Ipv6Route> RouteOutput (Ptr<Packet> p, const Ipv6Header &header, Ptr<NetDevice> oif, Socket::SocketErrno &sockerr);
  bool RouteInput (Ptr<const Packet> p, const Ipv6Header &header, Ptr<const NetDevice> idev, UnicastForwardCallback ucb,
                   MulticastForwardCallback mcb, LocalDeliverCallback lcb, ErrorCallback ecb);
  virtual void PrintRoutingTable (Ptr<OutputStreamWrapper> stream, Time::Unit unit = Time::S) const;
  virtual void NotifyInterfaceUp (uint32_t interface);
  virtual void NotifyInterfaceDown (uint32_t interface);
  virtual void NotifyAddAddress (uint32_t interface, Ipv6InterfaceAddress address);
  virtual void NotifyRemoveAddress (uint32_t interface, Ipv6InterfaceAddress address);
  virtual void SetIpv6 (Ptr<Ipv6> ipv6);
  virtual void NotifyAddRoute (Ipv6Address dst, Ipv6Prefix mask, Ipv6Address nextHop, uint32_t interface, Ipv6Address prefixToUse = Ipv6Address::GetZero ());
  virtual void NotifyRemoveRoute (Ipv6Address dst, Ipv6Prefix mask, Ipv6Address nextHop, uint32_t interface, Ipv6Address prefixToUse = Ipv6Address::GetZero ());

  // Methods to handle protocol parameters
  void SetEnableBufferFlag (bool f);
  bool GetEnableBufferFlag () const;
  void SetWSTFlag (bool f);
  bool GetWSTFlag () const;
  void SetEnableRAFlag (bool f);
  bool GetEnableRAFlag () const;

  /**
   * Assign a fixed random variable stream number to the random variables
   * used by this model.  Return the number of streams (possibly zero) that
   * have been assigned.
   *
   * \param stream first stream index to use
   * \return the number of stream indices assigned by this model
   */
  int64_t AssignStreams (int64_t stream);

private:
  // Protocol parameters.
  /// Holdtimes is the multiplicative factor of PeriodicUpdateInterval for which the node waits since the last update
  /// before flushing a route from the routing table. If PeriodicUpdateInterval is 8s and Holdtimes is 3, the node
  /// waits for 24s since the last update to flush this route from its routing table.
  uint32_t Holdtimes;
  /// PeriodicUpdateInterval specifies the periodic time interval between which the a node broadcasts
  /// its entire routing table.
  Time m_periodicUpdateInterval;
  /// SettlingTime specifies the time for which a node waits before propagating an update.
  /// It waits for this time interval in hope of receiving an update with a better metric.
  Time m_settlingTime;
  /// Nodes IP address
  Ipv6Address m_mainAddress;
  /// IP protocol
  Ptr<Ipv6> m_ipv6;
  /// Raw socket per each IP interface, map socket -> iface address (IP + mask)
  std::map<Ptr<Socket>, Ipv6InterfaceAddress> m_socketAddresses;
  /// Loopback device used to defer route requests until a route is found
  Ptr<NetDevice> m_lo;
  /// Main Routing table for the node
  RoutingTable m_routingTable;
  /// Advertised Routing table for the node
  RoutingTable m_advRoutingTable;
  /// The maximum number of packets that we allow a routing protocol to buffer.
  uint32_t m_maxQueueLen;
  /// The maximum number of packets that we allow per destination to buffer.
  uint32_t m_maxQueuedPacketsPerDst;
  /// The maximum period of time that a routing protocol is allowed to buffer a packet for.
  Time m_maxQueueTime;
  /// A "drop front on full" queue used by the routing layer to buffer packets to which it does not have a route.
  PacketQueue m_queue;
  /// Flag that is used to enable or disable buffering
  bool EnableBuffering;
  /// Flag that is used to enable or disable Weighted Settling Time
  bool EnableWST;
  /// This is the wighted factor to determine the weighted settling time
  double m_weightedFactor;
  /// This is a flag to enable route aggregation. Route aggregation will aggregate all routes for
  /// 'RouteAggregationTime' from the time an update is received by a node and sends them as a single update .
  bool EnableRouteAggregation;
  /// Parameter that holds the route aggregation time interval
  Time m_routeAggregationTime;
  /// Unicast callback for own packets
  UnicastForwardCallback m_scb;
  /// Error callback for own packets
  ErrorCallback m_ecb;

private:
  /// Start protocol operation
  void
  Start ();
  /// Queue packet untill we find a route
  void
  DeferredRouteOutput (Ptr<const Packet> p, const Ipv6Header & header, UnicastForwardCallback ucb, ErrorCallback ecb);
  /// Look for any queued packets to send them out
  void
  LookForQueuedPackets (ns3::Ptr<const ns3::NetDevice>);
  /**
   * Send packet from queue
   * \param dst - destination address to which we are sending the packet to
   * \param route - route identified for this packet
   */
  void
  SendPacketFromQueue (ns3::Ptr<const ns3::NetDevice>, Ipv6Address dst, Ptr<Ipv6Route> route);
  /// Find socket with local interface address iface
  Ptr<Socket>
  FindSocketWithInterfaceAddress (Ipv6InterfaceAddress iface) const;

  // Receive dsdv6 control packets
  /// Receive and process dsdv6 control packet
  void
  RecvDsdv6 (Ptr<Socket> socket);

  void
  Send (ns3::Ptr<const ns3::NetDevice>, Ptr<Ipv6Route>, Ptr<const Packet>, const Ipv6Header &);
  /// Create loopback route for given header
  Ptr<Ipv6Route>
  LoopbackRoute (const Ipv6Header & header, Ptr<NetDevice> oif) const;
  /**
   * Get settlingTime for a destination
   * \param dst - destination address
   * \return settlingTime for the destination if found
   */
  Time
  GetSettlingTime (Ipv6Address dst);
  /// Sends trigger update from a node
  void
  SendTriggeredUpdate ();
  /// Broadcasts the entire routing table for every PeriodicUpdateInterval
  void
  SendPeriodicUpdate ();
  void
  MergeTriggerPeriodicUpdates ();
  /// Notify that packet is dropped for some reason
  void
  Drop (Ptr<const Packet>, const Ipv6Header &, Socket::SocketErrno);
  /// Timer to trigger periodic updates from a node
  Timer m_periodicUpdateTimer;
  /// Timer used by the trigger updates in case of Weighted Settling Time is used
  Timer m_triggeredExpireTimer;

  /// Provides uniform random variables.
  Ptr<UniformRandomVariable> m_uniformRandomVariable;
};

}
}

#endif /* DSDV6_ROUTING_PROTOCOL_H */
