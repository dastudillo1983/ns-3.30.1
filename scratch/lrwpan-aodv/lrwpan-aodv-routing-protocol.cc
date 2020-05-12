/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2009 IITP RAS
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
 * Based on
 *      NS-2 AODV model developed by the CMU/MONARCH group and optimized and
 *      tuned by Samir Das and Mahesh Marina, University of Cincinnati;
 *
 *      AODV-UU implementation by Erik Nordström of Uppsala University
 *      http://core.it.uu.se/core/index.php/AODV-UU
 *
 * Authors: Elena Buchatskaia <borovkovaes@iitp.ru>
 *          Pavel Boyko <boyko@iitp.ru>
 */
#define NS_LOG_APPEND_CONTEXT                                   \
//  if (m_ipv4) { std::clog << "[node " << m_ipv4->GetObject<Node> ()->GetId () << "] "; }

#include "lrwpan-aodv-routing-protocol.h"
#include "ns3/log.h"
#include "ns3/boolean.h"
#include "ns3/random-variable-stream.h"
#include "ns3/inet-socket-address.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/udp-l4-protocol.h"
#include "ns3/udp-header.h"
#include "ns3/wifi-net-device.h"
#include "ns3/adhoc-wifi-mac.h"
#include "ns3/string.h"
#include "ns3/pointer.h"
#include <algorithm>
#include <limits>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("LrWpanAodvRoutingProtocol");

namespace lrwpanaodv {
NS_OBJECT_ENSURE_REGISTERED (AODVLrWpanRoutingProtocol);

/// UDP Port for AODV control traffic
const uint32_t AODVLrWpanRoutingProtocol::LRWPAN_AODV_SAP = 54; // Puerto 654

/**
* \ingroup aodv
* \brief Tag used by AODV implementation
*/
class DeferredRouteOutputTag : public Tag
{

public:
  /**
   * \brief Constructor
   * \param o the output interface
   */
  DeferredRouteOutputTag (int32_t o = -1) : Tag (),
                                            m_oif (o)
  {
  }

  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId ()
  {
    static TypeId tid = TypeId ("ns3::lrwpanaodv::DeferredRouteOutputTag")
      .SetParent<Tag> ()
      .SetGroupName ("LrWpan")
      .AddConstructor<DeferredRouteOutputTag> ()
    ;
    return tid;
  }

  TypeId  GetInstanceTypeId () const
  {
    return GetTypeId ();
  }

  /**
   * \brief Get the output interface
   * \return the output interface
   */
  int32_t GetInterface () const
  {
    return m_oif;
  }

  /**
   * \brief Set the output interface
   * \param oif the output interface
   */
  void SetInterface (int32_t oif)
  {
    m_oif = oif;
  }

  uint32_t GetSerializedSize () const
  {
    return sizeof(int32_t);
  }

  void  Serialize (TagBuffer i) const
  {
    i.WriteU32 (m_oif);
  }

  void  Deserialize (TagBuffer i)
  {
    m_oif = i.ReadU32 ();
  }

  void  Print (std::ostream &os) const
  {
    os << "DeferredRouteOutputTag: output interface = " << m_oif;
  }

private:
  /// Positive if output device is fixed in RouteOutput
  int32_t m_oif;
};

NS_OBJECT_ENSURE_REGISTERED (DeferredRouteOutputTag);


//-----------------------------------------------------------------------------
AODVLrWpanRoutingProtocol::AODVLrWpanRoutingProtocol ()
  : m_rreqRetries (2),
    m_ttlStart (1),
    m_ttlIncrement (2),
    m_ttlThreshold (7),
    m_timeoutBuffer (2),
    m_rreqRateLimit (10),
    m_rerrRateLimit (10),
    m_activeRouteTimeout (Seconds (3)),
    m_netDiameter (35),
    m_nodeTraversalTime (MilliSeconds (40)),
    m_netTraversalTime (Time ((2 * m_netDiameter) * m_nodeTraversalTime)),
    m_pathDiscoveryTime ( Time (2 * m_netTraversalTime)),
    m_myRouteTimeout (Time (2 * std::max (m_pathDiscoveryTime, m_activeRouteTimeout))),
    m_helloInterval (Seconds (1)),
    m_allowedHelloLoss (2),
    m_deletePeriod (Time (5 * std::max (m_activeRouteTimeout, m_helloInterval))),
    m_nextHopWait (m_nodeTraversalTime + MilliSeconds (10)),
    m_blackListTimeout (Time (m_rreqRetries * m_netTraversalTime)),
    m_maxQueueLen (64),
    m_maxQueueTime (Seconds (30)),
    m_destinationOnly (false),
    m_gratuitousReply (true),
    m_enableHello (false),
    m_routingTable (m_deletePeriod),
    m_queue (m_maxQueueLen, m_maxQueueTime),
    m_requestId (0),
    m_seqNo (0),
    m_rreqIdCache (m_pathDiscoveryTime),
    m_dpd (m_pathDiscoveryTime),
    m_nb (m_helloInterval),
    m_rreqCount (0),
    m_rerrCount (0),
    m_htimer (Timer::CANCEL_ON_DESTROY),
    m_rreqRateLimitTimer (Timer::CANCEL_ON_DESTROY),
    m_rerrRateLimitTimer (Timer::CANCEL_ON_DESTROY),
    m_lastBcastTime (Seconds (0))
{
  m_nb.SetCallback (MakeCallback (&AODVLrWpanRoutingProtocol::SendRerrWhenBreaksLinkToNextHop, this));
}

TypeId
AODVLrWpanRoutingProtocol::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::lrwpanaodv::AODVLrWpanRoutingProtocol")
    .SetParent<LrWpanRoutingProtocol> ()
    .SetGroupName ("LrWpan")
    .AddConstructor<AODVLrWpanRoutingProtocol> ()
    .AddAttribute ("HelloInterval", "HELLO messages emission interval.",
                   TimeValue (Seconds (1)),
                   MakeTimeAccessor (&AODVLrWpanRoutingProtocol::m_helloInterval),
                   MakeTimeChecker ())
    .AddAttribute ("TtlStart", "Initial TTL value for RREQ.",
                   UintegerValue (1),
                   MakeUintegerAccessor (&AODVLrWpanRoutingProtocol::m_ttlStart),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("TtlIncrement", "TTL increment for each attempt using the expanding ring search for RREQ dissemination.",
                   UintegerValue (2),
                   MakeUintegerAccessor (&AODVLrWpanRoutingProtocol::m_ttlIncrement),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("TtlThreshold", "Maximum TTL value for expanding ring search, TTL = NetDiameter is used beyond this value.",
                   UintegerValue (7),
                   MakeUintegerAccessor (&AODVLrWpanRoutingProtocol::m_ttlThreshold),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("TimeoutBuffer", "Provide a buffer for the timeout.",
                   UintegerValue (2),
                   MakeUintegerAccessor (&AODVLrWpanRoutingProtocol::m_timeoutBuffer),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("RreqRetries", "Maximum number of retransmissions of RREQ to discover a route",
                   UintegerValue (2),
                   MakeUintegerAccessor (&AODVLrWpanRoutingProtocol::m_rreqRetries),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("RreqRateLimit", "Maximum number of RREQ per second.",
                   UintegerValue (10),
                   MakeUintegerAccessor (&AODVLrWpanRoutingProtocol::m_rreqRateLimit),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("RerrRateLimit", "Maximum number of RERR per second.",
                   UintegerValue (10),
                   MakeUintegerAccessor (&AODVLrWpanRoutingProtocol::m_rerrRateLimit),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("NodeTraversalTime", "Conservative estimate of the average one hop traversal time for packets and should include "
                   "queuing delays, interrupt processing times and transfer times.",
                   TimeValue (MilliSeconds (40)),
                   MakeTimeAccessor (&AODVLrWpanRoutingProtocol::m_nodeTraversalTime),
                   MakeTimeChecker ())
    .AddAttribute ("NextHopWait", "Period of our waiting for the neighbour's RREP_ACK = 10 ms + NodeTraversalTime",
                   TimeValue (MilliSeconds (50)),
                   MakeTimeAccessor (&AODVLrWpanRoutingProtocol::m_nextHopWait),
                   MakeTimeChecker ())
    .AddAttribute ("ActiveRouteTimeout", "Period of time during which the route is considered to be valid",
                   TimeValue (Seconds (3)),
                   MakeTimeAccessor (&AODVLrWpanRoutingProtocol::m_activeRouteTimeout),
                   MakeTimeChecker ())
    .AddAttribute ("MyRouteTimeout", "Value of lifetime field in RREP generating by this node = 2 * max(ActiveRouteTimeout, PathDiscoveryTime)",
                   TimeValue (Seconds (11.2)),
                   MakeTimeAccessor (&AODVLrWpanRoutingProtocol::m_myRouteTimeout),
                   MakeTimeChecker ())
    .AddAttribute ("BlackListTimeout", "Time for which the node is put into the blacklist = RreqRetries * NetTraversalTime",
                   TimeValue (Seconds (5.6)),
                   MakeTimeAccessor (&AODVLrWpanRoutingProtocol::m_blackListTimeout),
                   MakeTimeChecker ())
    .AddAttribute ("DeletePeriod", "DeletePeriod is intended to provide an upper bound on the time for which an upstream node A "
                   "can have a neighbor B as an active next hop for destination D, while B has invalidated the route to D."
                   " = 5 * max (HelloInterval, ActiveRouteTimeout)",
                   TimeValue (Seconds (15)),
                   MakeTimeAccessor (&AODVLrWpanRoutingProtocol::m_deletePeriod),
                   MakeTimeChecker ())
    .AddAttribute ("NetDiameter", "Net diameter measures the maximum possible number of hops between two nodes in the network",
                   UintegerValue (35),
                   MakeUintegerAccessor (&AODVLrWpanRoutingProtocol::m_netDiameter),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("NetTraversalTime", "Estimate of the average net traversal time = 2 * NodeTraversalTime * NetDiameter",
                   TimeValue (Seconds (2.8)),
                   MakeTimeAccessor (&AODVLrWpanRoutingProtocol::m_netTraversalTime),
                   MakeTimeChecker ())
    .AddAttribute ("PathDiscoveryTime", "Estimate of maximum time needed to find route in network = 2 * NetTraversalTime",
                   TimeValue (Seconds (5.6)),
                   MakeTimeAccessor (&AODVLrWpanRoutingProtocol::m_pathDiscoveryTime),
                   MakeTimeChecker ())
    .AddAttribute ("MaxQueueLen", "Maximum number of packets that we allow a routing protocol to buffer.",
                   UintegerValue (64),
                   MakeUintegerAccessor (&AODVLrWpanRoutingProtocol::SetMaxQueueLen,
                                         &AODVLrWpanRoutingProtocol::GetMaxQueueLen),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("MaxQueueTime", "Maximum time packets can be queued (in seconds)",
                   TimeValue (Seconds (30)),
                   MakeTimeAccessor (&AODVLrWpanRoutingProtocol::SetMaxQueueTime,
                                     &AODVLrWpanRoutingProtocol::GetMaxQueueTime),
                   MakeTimeChecker ())
    .AddAttribute ("AllowedHelloLoss", "Number of hello messages which may be loss for valid link.",
                   UintegerValue (2),
                   MakeUintegerAccessor (&AODVLrWpanRoutingProtocol::m_allowedHelloLoss),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("GratuitousReply", "Indicates whether a gratuitous RREP should be unicast to the node originated route discovery.",
                   BooleanValue (true),
                   MakeBooleanAccessor (&AODVLrWpanRoutingProtocol::SetGratuitousReplyFlag,
                                        &AODVLrWpanRoutingProtocol::GetGratuitousReplyFlag),
                   MakeBooleanChecker ())
    .AddAttribute ("DestinationOnly", "Indicates only the destination may respond to this RREQ.",
                   BooleanValue (false),
                   MakeBooleanAccessor (&AODVLrWpanRoutingProtocol::SetDestinationOnlyFlag,
                                        &AODVLrWpanRoutingProtocol::GetDestinationOnlyFlag),
                   MakeBooleanChecker ())
    .AddAttribute ("EnableHello", "Indicates whether a hello messages enable.",
                   BooleanValue (true),
                   MakeBooleanAccessor (&AODVLrWpanRoutingProtocol::SetHelloEnable,
                                        &AODVLrWpanRoutingProtocol::GetHelloEnable),
                   MakeBooleanChecker ())
    .AddAttribute ("EnableBroadcast", "Indicates whether a broadcast data packets forwarding enable.",
                   BooleanValue (true),
                   MakeBooleanAccessor (&AODVLrWpanRoutingProtocol::SetBroadcastEnable,
                                        &AODVLrWpanRoutingProtocol::GetBroadcastEnable),
                   MakeBooleanChecker ())
    .AddAttribute ("UniformRv",
                   "Access to the underlying UniformRandomVariable",
                   StringValue ("ns3::UniformRandomVariable"),
                   MakePointerAccessor (&AODVLrWpanRoutingProtocol::m_uniformRandomVariable),
                   MakePointerChecker<UniformRandomVariable> ())
	.AddTraceSource ("Send",
					 "Trace send packet",
					 MakeTraceSourceAccessor (&AODVLrWpanRoutingProtocol::m_send),
					 "ns3::Packet::TracedCallback")
  ;
  return tid;
}

void
AODVLrWpanRoutingProtocol::SetMaxQueueLen (uint32_t len)
{
  m_maxQueueLen = len;
  m_queue.SetMaxQueueLen (len);
}
void
AODVLrWpanRoutingProtocol::SetMaxQueueTime (Time t)
{
  m_maxQueueTime = t;
  m_queue.SetQueueTimeout (t);
}

AODVLrWpanRoutingProtocol::~AODVLrWpanRoutingProtocol ()
{
}

void
AODVLrWpanRoutingProtocol::DoDispose ()
{
  //m_lrwpan = 0;
  for (std::map<Ptr<Socket>, Ipv4InterfaceAddress>::iterator iter =
         m_socketAddresses.begin (); iter != m_socketAddresses.end (); iter++)
    {
      iter->first->Close ();
    }
  m_socketAddresses.clear ();
  for (std::map<Ptr<Socket>, Ipv4InterfaceAddress>::iterator iter =
         m_socketSubnetBroadcastAddresses.begin (); iter != m_socketSubnetBroadcastAddresses.end (); iter++)
    {
      iter->first->Close ();
    }
  m_socketSubnetBroadcastAddresses.clear ();
  LrWpanRoutingProtocol::DoDispose ();
}

void
AODVLrWpanRoutingProtocol::PrintRoutingTable (Ptr<OutputStreamWrapper> stream, Time::Unit unit) const
{
  *stream->GetStream () << "Node: " << GetNode()->GetId ()
                        << "; Time: " << Now ().As (unit)
                        << ", Local time: " << GetObject<Node> ()->GetLocalTime ().As (unit)
                        << ", AODV Routing table" << std::endl;

  m_routingTable.Print (stream);
  *stream->GetStream () << std::endl;
}

int64_t
AODVLrWpanRoutingProtocol::AssignStreams (int64_t stream)
{
  NS_LOG_FUNCTION (this << stream);
  m_uniformRandomVariable->SetStream (stream);
  return 1;
}

void
AODVLrWpanRoutingProtocol::Start ()
{
  NS_LOG_FUNCTION (this);
  m_device = DynamicCast<LrWpanNetDevice>(GetNode()->GetDevice(0));
  m_device->SetReceiveCallback(MakeCallback (&AODVLrWpanRoutingProtocol::RecvAodv, this));


  if (m_enableHello)
    {
      m_nb.ScheduleTimer ();
    }
  m_rreqRateLimitTimer.SetFunction (&AODVLrWpanRoutingProtocol::RreqRateLimitTimerExpire,
                                    this);
  m_rreqRateLimitTimer.Schedule (Seconds (1));

  m_rerrRateLimitTimer.SetFunction (&AODVLrWpanRoutingProtocol::RerrRateLimitTimerExpire,
                                    this);
  m_rerrRateLimitTimer.Schedule (Seconds (1));

}

Ptr<LrWpanRoute>
AODVLrWpanRoutingProtocol::RouteOutput (Ptr<Packet> p, const LrWpanMacHeader &header,
                              Ptr<NetDevice> oif, Socket::SocketErrno &sockerr)
{
  NS_LOG_FUNCTION (this << header << (oif ? oif->GetIfIndex () : 0));
  if (!p)
    {
	  sockerr = Socket::ERROR_NOROUTETOHOST;
      NS_LOG_DEBUG ("Packet is == 0");
      // TODO: Que pasa cuando el paquete es null, no hay direccion de loopback
      Ptr<LrWpanRoute> route;
      return route; // later
    }
  sockerr = Socket::ERROR_NOTERROR;
  Ptr<LrWpanRoute> route;
  Mac16Address dst = header.GetShortDstAddr ();
  LrWpanRoutingTableEntry rt;
  if (m_routingTable.LookupValidRoute (dst, rt))
    {
      route = rt.GetRoute ();
      NS_ASSERT (route != 0);
      NS_LOG_DEBUG ("Exist route to " << route->GetDestination() << " from interface " << route->GetSource());
      if (oif != 0 && route->GetOutputDevice () != oif)
        {
          NS_LOG_DEBUG ("Output device doesn't match. Dropped.");
          sockerr = Socket::ERROR_NOROUTETOHOST;
          return Ptr<LrWpanRoute> ();
        }
      UpdateRouteLifeTime (dst, m_activeRouteTimeout);
      UpdateRouteLifeTime (route->GetPANCoordinator(), m_activeRouteTimeout);
      return route;
    }

  sockerr = Socket::ERROR_NOROUTETOHOST;
  NS_LOG_DEBUG ("Valid Route not found");
  return route;
}

void
AODVLrWpanRoutingProtocol::DeferredRouteOutput (Ptr<const Packet> p, const LrWpanMacHeader & header,
                                      UnicastForwardCallback ucb, ErrorCallback ecb)
{
  NS_LOG_FUNCTION (this << p << header);
  NS_ASSERT (p != 0 && p != Ptr<Packet> ());

  LrWpanQueueEntry newEntry (p, header, ucb, ecb);
  bool result = m_queue.Enqueue (newEntry);
  if (result)
    {
      NS_LOG_LOGIC ("Add packet " << p->GetUid () << " to queue. ");
      LrWpanRoutingTableEntry rt;
      bool result = m_routingTable.LookupRoute (header.GetShortDstAddr (), rt);
      if (!result || ((rt.GetFlag () != IN_SEARCH) && result))
        {
          NS_LOG_LOGIC ("Send new RREQ for outbound packet to " << header.GetShortDstAddr ());
          SendRequest (header.GetShortDstAddr ());
        }
    }
}

bool
AODVLrWpanRoutingProtocol::RouteInput (Ptr<const Packet> p, const LrWpanMacHeader &header,
                             Ptr<const NetDevice> idev, UnicastForwardCallback ucb, ErrorCallback ecb)
{
  NS_LOG_FUNCTION (this << p->GetUid () << header.GetShortDstAddr () << idev->GetAddress ());
  if (m_socketAddresses.empty ())
    {
      NS_LOG_LOGIC ("No aodv interfaces");
      return false;
    }
  //NS_ASSERT (m_lrwpan != 0);
  NS_ASSERT (p != 0);

  Mac16Address dst = header.GetShortDstAddr ();
  Mac16Address origin = header.GetShortSrcAddr ();

  // Duplicate of own packet
  if (IsMyOwnAddress (origin))
    {
      return true;
    }

  if (dst.IsBroadcast ())
  {
	  if (m_dpd.IsDuplicate (p, header))
	  {
		  NS_LOG_DEBUG ("Duplicated packet " << p->GetUid () << " from " << origin << ". Drop.");
		  return true;
	  }
	  UpdateRouteLifeTime (origin, m_activeRouteTimeout);
	  Ptr<Packet> packet = p->Copy ();
	  if (!m_enableBroadcast)
	  {
		  return true;
	  }
	  // TODO: Hay que ver como gestionar el TTL en el header de AODV, ya que no estamos trabajando sobre IP
//	  if (header.GetTtl () > 1)
//	  {
		  NS_LOG_LOGIC ("Forward broadcast."); // TTL " << (uint16_t) header.GetTtl ());
		  LrWpanRoutingTableEntry toBroadcast;
		  if (m_routingTable.LookupRoute (dst, toBroadcast))
		  {
			  Ptr<LrWpanRoute> route = toBroadcast.GetRoute ();
			  ucb (route, packet, header);
		  }
		  else
		  {
			  // TODO: agregar un callback para tratar los paquetes eliminados
			  NS_LOG_DEBUG ("No route to forward broadcast. Drop packet " << p->GetUid ());
		  }
//	  }
//	  else
//	  {
//		  NS_LOG_DEBUG ("TTL exceeded. Drop packet " << p->GetUid ());
//	  }
	  return true;
  }

  //TODO: Hay que revisar lo quite porque esta relacionado con el Local Delivery que no lo usamos cuando usamos el
  //      protocolo sobre la capa MAC
//  // Unicast local delivery
//  if (dst == Mac16Address("FF:FF") || dst == m_device->GetMac()->GetShortAddress())
//    {
//      UpdateRouteLifeTime (origin, m_activeRouteTimeout);
//      LrWpanRoutingTableEntry toOrigin;
//      if (m_routingTable.LookupValidRoute (origin, toOrigin))
//        {
//          UpdateRouteLifeTime (toOrigin.GetNextHop (), m_activeRouteTimeout);
//          m_nb.Update (toOrigin.GetNextHop (), m_activeRouteTimeout);
//        }
//      if (lcb.IsNull () == false)
//        {
//          NS_LOG_LOGIC ("Unicast local delivery to " << dst);
//          lcb (p, header, iif);
//        }
//      else
//        {
//          NS_LOG_ERROR ("Unable to deliver packet locally due to null callback " << p->GetUid () << " from " << origin);
//          ecb (p, header, Socket::ERROR_NOROUTETOHOST);
//        }
//      return true;
//    }

  // Forwarding
  return Forwarding (p, header, ucb, ecb);
}

bool
AODVLrWpanRoutingProtocol::Forwarding (Ptr<const Packet> p, const LrWpanMacHeader & header,
                             UnicastForwardCallback ucb, ErrorCallback ecb)
{
  NS_LOG_FUNCTION (this);
  Mac16Address dst = header.GetShortDstAddr ();
  Mac16Address origin = header.GetShortSrcAddr ();
  m_routingTable.Purge ();
  LrWpanRoutingTableEntry toDst;
  if (m_routingTable.LookupRoute (dst, toDst))
    {
      if (toDst.GetFlag () == VALID)
        {
          Ptr<LrWpanRoute> route = toDst.GetRoute ();
          NS_LOG_LOGIC (route->GetDestination() << " forwarding to " << dst << " from " << origin << " packet " << p->GetUid ());

          /*
           *  Each time a route is used to forward a data packet, its Active Route
           *  Lifetime field of the source, destination and the next hop on the
           *  path to the destination is updated to be no less than the current
           *  time plus ActiveRouteTimeout.
           */
          UpdateRouteLifeTime (origin, m_activeRouteTimeout);
          UpdateRouteLifeTime (dst, m_activeRouteTimeout);
          UpdateRouteLifeTime (route->GetPANCoordinator (), m_activeRouteTimeout);
          /*
           *  Since the route between each originator and destination pair is expected to be symmetric, the
           *  Active Route Lifetime for the previous hop, along the reverse path back to the IP source, is also updated
           *  to be no less than the current time plus ActiveRouteTimeout
           */
          LrWpanRoutingTableEntry toOrigin;
          m_routingTable.LookupRoute (origin, toOrigin);
          UpdateRouteLifeTime (toOrigin.GetNextHop (), m_activeRouteTimeout);

          m_nb.Update (route->GetPANCoordinator (), m_activeRouteTimeout);
          m_nb.Update (toOrigin.GetNextHop (), m_activeRouteTimeout);

          ucb (route, p, header);
          return true;
        }
      else
        {
          if (toDst.GetValidSeqNo ())
            {
              SendRerrWhenNoRouteToForward (dst, toDst.GetSeqNo (), origin);
              NS_LOG_DEBUG ("Drop packet " << p->GetUid () << " because no route to forward it.");
              return false;
            }
        }
    }
  NS_LOG_LOGIC ("route not found to " << dst << ". Send RERR message.");
  NS_LOG_DEBUG ("Drop packet " << p->GetUid () << " because no route to forward it.");
  SendRerrWhenNoRouteToForward (dst, 0, origin);
  return false;
}

//void
//AODVLrWpanRoutingProtocol::SetLrWpan (Ptr<LrWpan> lrwpan)
//{
//  NS_ASSERT (lrwpan != 0);
//  NS_ASSERT (m_lrwpan == 0);
//
//  m_lrwpan = lrwpan;
//
//  Simulator::ScheduleNow (&AODVLrWpanRoutingProtocol::Start, this);
//}

bool
AODVLrWpanRoutingProtocol::IsMyOwnAddress (Mac16Address src)
{
  NS_LOG_FUNCTION (this << src);
  for (std::map<Ptr<Socket>, Ipv4InterfaceAddress>::const_iterator j =
         m_socketAddresses.begin (); j != m_socketAddresses.end (); ++j)
    {
      Ipv4InterfaceAddress iface = j->second;
      if (src == iface.GetLocal ())
        {
          return true;
        }
    }
  return false;
}

void
AODVLrWpanRoutingProtocol::SendRequest (Mac16Address dst)
{
  NS_LOG_FUNCTION ( this << dst);
  // A node SHOULD NOT originate more than RREQ_RATELIMIT RREQ messages per second.
  if (m_rreqCount == m_rreqRateLimit)
    {
      Simulator::Schedule (m_rreqRateLimitTimer.GetDelayLeft () + MicroSeconds (100),
                           &AODVLrWpanRoutingProtocol::SendRequest, this, dst);
      return;
    }
  else
    {
      m_rreqCount++;
    }
  // Create RREQ header
  LrWpanRreqHeader rreqHeader;
  rreqHeader.SetDst (dst);

  LrWpanRoutingTableEntry rt;
  // Using the Hop field in Routing Table to manage the expanding ring search
  uint16_t ttl = m_ttlStart;
  if (m_routingTable.LookupRoute (dst, rt))
    {
      if (rt.GetFlag () != IN_SEARCH)
        {
          ttl = std::min<uint16_t> (rt.GetHop () + m_ttlIncrement, m_netDiameter);
        }
      else
        {
          ttl = rt.GetHop () + m_ttlIncrement;
          if (ttl > m_ttlThreshold)
            {
              ttl = m_netDiameter;
            }
        }
      if (ttl == m_netDiameter)
        {
          rt.IncrementRreqCnt ();
        }
      if (rt.GetValidSeqNo ())
        {
          rreqHeader.SetDstSeqno (rt.GetSeqNo ());
        }
      else
        {
          rreqHeader.SetUnknownSeqno (true);
        }
      rt.SetHop (ttl);
      rt.SetFlag (IN_SEARCH);
      rt.SetLifeTime (m_pathDiscoveryTime);
      m_routingTable.Update (rt);
    }
  else
    {
      rreqHeader.SetUnknownSeqno (true);
      Ptr<NetDevice> dev = 0;
      LrWpanRoutingTableEntry newEntry (/*device=*/ dev, /*dst=*/ dst, /*validSeqNo=*/ false, /*seqno=*/ 0,
                                              /*hop=*/ ttl,
                                              /*nextHop=*/ Mac16Address (), /*lifeTime=*/ m_pathDiscoveryTime);
      // Check if TtlStart == NetDiameter
      if (ttl == m_netDiameter)
        {
          newEntry.IncrementRreqCnt ();
        }
      newEntry.SetFlag (IN_SEARCH);
      m_routingTable.AddRoute (newEntry);
    }

  if (m_gratuitousReply)
    {
      rreqHeader.SetGratuitousRrep (true);
    }
  if (m_destinationOnly)
    {
      rreqHeader.SetDestinationOnly (true);
    }

  m_seqNo++;
  rreqHeader.SetOriginSeqno (m_seqNo);
  m_requestId++;
  rreqHeader.SetId (m_requestId);

  // Send RREQ as subnet directed broadcast from each interface used by aodv
  rreqHeader.SetOrigin (m_device->GetMac()->GetShortAddress());
  m_rreqIdCache.IsDuplicate (m_device->GetMac()->GetShortAddress(), m_requestId);

  Ptr<Packet> packet = Create<Packet> ();
  SocketIpTtlTag tag;
  tag.SetTtl (ttl);
  packet->AddPacketTag (tag);
  packet->AddHeader (rreqHeader);
  LrWpanTypeHeader tHeader (AODVTYPE_RREQ);
  packet->AddHeader (tHeader);
  // Send to all-hosts broadcast if on FFFF addr
  Mac16Address destination = Mac16Address("FF:FF");
  NS_LOG_DEBUG ("Send RREQ with id " << rreqHeader.GetId () << " to socket");
  m_lastBcastTime = Simulator::Now ();
  Simulator::Schedule (Time (MilliSeconds (m_uniformRandomVariable->GetInteger (0, 10))), &AODVLrWpanRoutingProtocol::SendTo, this, packet, destination);
  ScheduleRreqRetry (dst);
}

void
AODVLrWpanRoutingProtocol::SendTo (Ptr<Packet> packet, Mac16Address destination)
{
  m_send(packet, destination, LRWPAN_AODV_SAP);
  m_device->Send(packet, destination, LRWPAN_AODV_SAP);
}
void
AODVLrWpanRoutingProtocol::ScheduleRreqRetry (Mac16Address dst)
{
  NS_LOG_FUNCTION (this << dst);
  if (m_addressReqTimer.find (dst) == m_addressReqTimer.end ())
    {
      Timer timer (Timer::CANCEL_ON_DESTROY);
      m_addressReqTimer[dst] = timer;
    }
  m_addressReqTimer[dst].SetFunction (&AODVLrWpanRoutingProtocol::RouteRequestTimerExpire, this);
  m_addressReqTimer[dst].Remove ();
  m_addressReqTimer[dst].SetArguments (dst);
  LrWpanRoutingTableEntry rt;
  m_routingTable.LookupRoute (dst, rt);
  Time retry;
  if (rt.GetHop () < m_netDiameter)
    {
      retry = 2 * m_nodeTraversalTime * (rt.GetHop () + m_timeoutBuffer);
    }
  else
    {
      NS_ABORT_MSG_UNLESS (rt.GetRreqCnt () > 0, "Unexpected value for GetRreqCount ()");
      uint16_t backoffFactor = rt.GetRreqCnt () - 1;
      NS_LOG_LOGIC ("Applying binary exponential backoff factor " << backoffFactor);
      retry = m_netTraversalTime * (1 << backoffFactor);
    }
  m_addressReqTimer[dst].Schedule (retry);
  NS_LOG_LOGIC ("Scheduled RREQ retry in " << retry.GetSeconds () << " seconds");
}

bool
AODVLrWpanRoutingProtocol::RecvAodv (Ptr<NetDevice> device, Ptr<const Packet> pkt,
		uint16_t protocolNumber, const Address &from)
{

  Ptr<Packet> packet = pkt->Copy();
  Address sourceAddress;
  Mac16Address sender =  Mac16Address::ConvertFrom(from);
  Mac16Address receiver = m_device->GetMac()->GetShortAddress();

  NS_LOG_DEBUG ("AODV node " << this << " received a AODV packet from " << sender << " to " << receiver);

  UpdateRouteToNeighbor (sender, receiver);
  LrWpanTypeHeader tHeader (AODVTYPE_RREQ);
  packet->RemoveHeader (tHeader);
  if (!tHeader.IsValid ())
    {
      NS_LOG_DEBUG ("AODV message " << packet->GetUid () << " with unknown type received: " << tHeader.Get () << ". Drop");
      return false; // drop
    }
  switch (tHeader.Get ())
    {
    case AODVTYPE_RREQ:
      {
        RecvRequest (packet, receiver, sender);
        break;
      }
    case AODVTYPE_RREP:
      {
        RecvReply (packet, receiver, sender);
        break;
      }
    case AODVTYPE_RERR:
      {
        RecvError (packet, sender);
        break;
      }
    case AODVTYPE_RREP_ACK:
      {
        RecvReplyAck (sender);
        break;
      }
    }
  return true;
}

bool
AODVLrWpanRoutingProtocol::UpdateRouteLifeTime (Mac16Address addr, Time lifetime)
{
  NS_LOG_FUNCTION (this << addr << lifetime);
  LrWpanRoutingTableEntry rt;
  if (m_routingTable.LookupRoute (addr, rt))
    {
      if (rt.GetFlag () == VALID)
        {
          NS_LOG_DEBUG ("Updating VALID route");
          rt.SetRreqCnt (0);
          rt.SetLifeTime (std::max (lifetime, rt.GetLifeTime ()));
          m_routingTable.Update (rt);
          return true;
        }
    }
  return false;
}

void
AODVLrWpanRoutingProtocol::UpdateRouteToNeighbor (Mac16Address sender, Mac16Address receiver)
{
  NS_LOG_FUNCTION (this << "sender " << sender << " receiver " << receiver);
  LrWpanRoutingTableEntry toNeighbor;
  if (!m_routingTable.LookupRoute (sender, toNeighbor))
    {
      LrWpanRoutingTableEntry newEntry (/*device=*/ m_device, /*dst=*/ sender, /*know seqno=*/ false, /*seqno=*/ 0,
                                              /*hops=*/ 1, /*next hop=*/ sender, /*lifetime=*/ m_activeRouteTimeout);
      m_routingTable.AddRoute (newEntry);
    }
  else
    {
      if (toNeighbor.GetValidSeqNo () && (toNeighbor.GetHop () == 1) && (toNeighbor.GetOutputDevice () == m_device))  // TODO: posiblemente haya como eliminar GetOutputDevice
        {
          toNeighbor.SetLifeTime (std::max (m_activeRouteTimeout, toNeighbor.GetLifeTime ()));
        }
      else
        {
          LrWpanRoutingTableEntry newEntry (/*device=*/ m_device, /*dst=*/ sender, /*know seqno=*/ false, /*seqno=*/ 0,
                                                  /*hops=*/ 1, /*next hop=*/ sender, /*lifetime=*/ std::max (m_activeRouteTimeout, toNeighbor.GetLifeTime ()));
          m_routingTable.Update (newEntry);
        }
    }

}

void
AODVLrWpanRoutingProtocol::RecvRequest (Ptr<Packet> p, Mac16Address receiver, Mac16Address src)
{
  NS_LOG_FUNCTION (this);
  LrWpanRreqHeader rreqHeader;
  p->RemoveHeader (rreqHeader);

  // A node ignores all RREQs received from any node in its blacklist
  LrWpanRoutingTableEntry toPrev;
  if (m_routingTable.LookupRoute (src, toPrev))
    {
      if (toPrev.IsUnidirectional ())
        {
          NS_LOG_DEBUG ("Ignoring RREQ from node in blacklist");
          return;
        }
    }

  uint32_t id = rreqHeader.GetId ();
  Mac16Address origin = rreqHeader.GetOrigin ();

  /*
   *  Node checks to determine whether it has received a RREQ with the same Originator IP Address and RREQ ID.
   *  If such a RREQ has been received, the node silently discards the newly received RREQ.
   */
  if (m_rreqIdCache.IsDuplicate (origin, id))
    {
      NS_LOG_DEBUG ("Ignoring RREQ due to duplicate");
      return;
    }

  // Increment RREQ hop count
  uint8_t hop = rreqHeader.GetHopCount () + 1;
  rreqHeader.SetHopCount (hop);

  /*
   *  When the reverse route is created or updated, the following actions on the route are also carried out:
   *  1. the Originator Sequence Number from the RREQ is compared to the corresponding destination sequence number
   *     in the route table entry and copied if greater than the existing value there
   *  2. the valid sequence number field is set to true;
   *  3. the next hop in the routing table becomes the node from which the  RREQ was received
   *  4. the hop count is copied from the Hop Count in the RREQ message;
   *  5. the Lifetime is set to be the maximum of (ExistingLifetime, MinimalLifetime), where
   *     MinimalLifetime = current time + 2*NetTraversalTime - 2*HopCount*NodeTraversalTime
   */
  LrWpanRoutingTableEntry toOrigin;
  if (!m_routingTable.LookupRoute (origin, toOrigin))
    {
      LrWpanRoutingTableEntry newEntry (/*device=*/ m_device, /*dst=*/ origin, /*validSeno=*/ true, /*seqNo=*/ rreqHeader.GetOriginSeqno (),
                                              /*hops=*/ hop,
                                              /*nextHop*/ src, /*timeLife=*/ Time ((2 * m_netTraversalTime - 2 * hop * m_nodeTraversalTime)));
      m_routingTable.AddRoute (newEntry);
    }
  else
    {
      if (toOrigin.GetValidSeqNo ())
        {
          if (int32_t (rreqHeader.GetOriginSeqno ()) - int32_t (toOrigin.GetSeqNo ()) > 0)
            {
              toOrigin.SetSeqNo (rreqHeader.GetOriginSeqno ());
            }
        }
      else
        {
          toOrigin.SetSeqNo (rreqHeader.GetOriginSeqno ());
        }
      toOrigin.SetValidSeqNo (true);
      toOrigin.SetNextHop (src);
      toOrigin.SetOutputDevice (m_device);
      toOrigin.SetHop (hop);
      toOrigin.SetLifeTime (std::max (Time (2 * m_netTraversalTime - 2 * hop * m_nodeTraversalTime),
                                      toOrigin.GetLifeTime ()));
      m_routingTable.Update (toOrigin);
      //m_nb.Update (src, Time (AllowedHelloLoss * HelloInterval));
    }


  LrWpanRoutingTableEntry toNeighbor;
  if (!m_routingTable.LookupRoute (src, toNeighbor))
    {
      NS_LOG_DEBUG ("Neighbor:" << src << " not found in routing table. Creating an entry");
      LrWpanRoutingTableEntry newEntry (m_device, src, false, rreqHeader.GetOriginSeqno (),
                                  1, src, m_activeRouteTimeout);
      m_routingTable.AddRoute (newEntry);
    }
  else
    {
      toNeighbor.SetLifeTime (m_activeRouteTimeout);
      toNeighbor.SetValidSeqNo (false);
      toNeighbor.SetSeqNo (rreqHeader.GetOriginSeqno ());
      toNeighbor.SetFlag (VALID);
      toNeighbor.SetOutputDevice (m_device);
      toNeighbor.SetHop (1);
      toNeighbor.SetNextHop (src);
      m_routingTable.Update (toNeighbor);
    }
  m_nb.Update (src, Time (m_allowedHelloLoss * m_helloInterval));

  NS_LOG_LOGIC (receiver << " receive RREQ with hop count " << static_cast<uint32_t> (rreqHeader.GetHopCount ())
                         << " ID " << rreqHeader.GetId ()
                         << " to destination " << rreqHeader.GetDst ());

  //  A node generates a RREP if either:
  //  (i)  it is itself the destination,
  if (IsMyOwnAddress (rreqHeader.GetDst ()))
    {
      m_routingTable.LookupRoute (origin, toOrigin);
      NS_LOG_DEBUG ("Send reply since I am the destination");
      SendReply (rreqHeader, toOrigin);
      return;
    }
  /*
   * (ii) or it has an active route to the destination, the destination sequence number in the node's existing route table entry for the destination
   *      is valid and greater than or equal to the Destination Sequence Number of the RREQ, and the "destination only" flag is NOT set.
   */
  LrWpanRoutingTableEntry toDst;
  Mac16Address dst = rreqHeader.GetDst ();
  if (m_routingTable.LookupRoute (dst, toDst))
    {
      /*
       * Drop RREQ, This node RREP will make a loop.
       */
      if (toDst.GetNextHop () == src)
        {
          NS_LOG_DEBUG ("Drop RREQ from " << src << ", dest next hop " << toDst.GetNextHop ());
          return;
        }
      /*
       * The Destination Sequence number for the requested destination is set to the maximum of the corresponding value
       * received in the RREQ message, and the destination sequence value currently maintained by the node for the requested destination.
       * However, the forwarding node MUST NOT modify its maintained value for the destination sequence number, even if the value
       * received in the incoming RREQ is larger than the value currently maintained by the forwarding node.
       */
      if ((rreqHeader.GetUnknownSeqno () || (int32_t (toDst.GetSeqNo ()) - int32_t (rreqHeader.GetDstSeqno ()) >= 0))
          && toDst.GetValidSeqNo () )
        {
          if (!rreqHeader.GetDestinationOnly () && toDst.GetFlag () == VALID)
            {
              m_routingTable.LookupRoute (origin, toOrigin);
              SendReplyByIntermediateNode (toDst, toOrigin, rreqHeader.GetGratuitousRrep ());
              return;
            }
          rreqHeader.SetDstSeqno (toDst.GetSeqNo ());
          rreqHeader.SetUnknownSeqno (false);
        }
    }

  SocketIpTtlTag tag;
  p->RemovePacketTag (tag);
  if (tag.GetTtl () < 2)
    {
      NS_LOG_DEBUG ("TTL exceeded. Drop RREQ origin " << src << " destination " << dst );
      return;
    }

  for (std::map<Ptr<Socket>, Ipv4InterfaceAddress>::const_iterator j =
         m_socketAddresses.begin (); j != m_socketAddresses.end (); ++j)
    {
      Ptr<Packet> packet = Create<Packet> ();
      SocketIpTtlTag ttl;
      ttl.SetTtl (tag.GetTtl () - 1);
      packet->AddPacketTag (ttl);
      packet->AddHeader (rreqHeader);
      LrWpanTypeHeader tHeader (AODVTYPE_RREQ);
      packet->AddHeader (tHeader);
      // Send to all-hosts broadcast if on FFFF addr, subnet-directed otherwise
      Mac16Address destination = Mac16Address("FF:FF");
      m_lastBcastTime = Simulator::Now ();
      Simulator::Schedule (Time (MilliSeconds (m_uniformRandomVariable->GetInteger (0, 10))), &AODVLrWpanRoutingProtocol::SendTo, this, packet, destination);

    }
}

void
AODVLrWpanRoutingProtocol::SendReply (LrWpanRreqHeader const & rreqHeader, LrWpanRoutingTableEntry const & toOrigin)
{
  NS_LOG_FUNCTION (this << toOrigin.GetDestination());
  /*
   * Destination node MUST increment its own sequence number by one if the sequence number in the RREQ packet is equal to that
   * incremented value. Otherwise, the destination does not change its sequence number before generating the  RREP message.
   */
  if (!rreqHeader.GetUnknownSeqno () && (rreqHeader.GetDstSeqno () == m_seqNo + 1))
    {
      m_seqNo++;
    }
  LrWpanRrepHeader rrepHeader ( /*prefixSize=*/ 0, /*hops=*/ 0, /*dst=*/ rreqHeader.GetDst (),
                                          /*dstSeqNo=*/ m_seqNo, /*origin=*/ toOrigin.GetDestination(), /*lifeTime=*/ m_myRouteTimeout);
  Ptr<Packet> packet = Create<Packet> ();
  SocketIpTtlTag tag;
  tag.SetTtl (toOrigin.GetHop ());
  packet->AddPacketTag (tag);
  packet->AddHeader (rrepHeader);
  LrWpanTypeHeader tHeader (AODVTYPE_RREP);
  packet->AddHeader (tHeader);
  m_device->Send(packet, toOrigin.GetNextHop (), LRWPAN_AODV_SAP);
}

void
AODVLrWpanRoutingProtocol::SendReplyByIntermediateNode (LrWpanRoutingTableEntry & toDst, LrWpanRoutingTableEntry & toOrigin, bool gratRep)
{
  NS_LOG_FUNCTION (this);
  LrWpanRrepHeader rrepHeader (/*prefix size=*/ 0, /*hops=*/ toDst.GetHop (), /*dst=*/ toDst.GetDestination(), /*dst seqno=*/ toDst.GetSeqNo (),
                                          /*origin=*/ toOrigin.GetDestination(), /*lifetime=*/ toDst.GetLifeTime ());
  /* If the node we received a RREQ for is a neighbor we are
   * probably facing a unidirectional link... Better request a RREP-ack
   */
  if (toDst.GetHop () == 1)
    {
      rrepHeader.SetAckRequired (true);
      LrWpanRoutingTableEntry toNextHop;
      m_routingTable.LookupRoute (toOrigin.GetNextHop (), toNextHop);
      toNextHop.m_ackTimer.SetFunction (&AODVLrWpanRoutingProtocol::AckTimerExpire, this);
      toNextHop.m_ackTimer.SetArguments (toNextHop.GetDestination(), m_blackListTimeout);
      toNextHop.m_ackTimer.SetDelay (m_nextHopWait);
    }
  toDst.InsertPrecursor (toOrigin.GetNextHop ());
  toOrigin.InsertPrecursor (toDst.GetNextHop ());
  m_routingTable.Update (toDst);
  m_routingTable.Update (toOrigin);

  Ptr<Packet> packet = Create<Packet> ();
  SocketIpTtlTag tag;
  tag.SetTtl (toOrigin.GetHop ());
  packet->AddPacketTag (tag);
  packet->AddHeader (rrepHeader);
  LrWpanTypeHeader tHeader (AODVTYPE_RREP);
  packet->AddHeader (tHeader);
  m_device->Send(packet, toOrigin.GetNextHop (), LRWPAN_AODV_SAP);

  // Generating gratuitous RREPs
  if (gratRep)
    {
      LrWpanRrepHeader gratRepHeader (/*prefix size=*/ 0, /*hops=*/ toOrigin.GetHop (), /*dst=*/ toOrigin.GetDestination(),
                                                 /*dst seqno=*/ toOrigin.GetSeqNo (), /*origin=*/ toDst.GetDestination (),
                                                 /*lifetime=*/ toOrigin.GetLifeTime ());
      Ptr<Packet> packetToDst = Create<Packet> ();
      SocketIpTtlTag gratTag;
      gratTag.SetTtl (toDst.GetHop ());
      packetToDst->AddPacketTag (gratTag);
      packetToDst->AddHeader (gratRepHeader);
      LrWpanTypeHeader type (AODVTYPE_RREP);
      packetToDst->AddHeader (type);
      NS_LOG_LOGIC ("Send gratuitous RREP " << packet->GetUid ());
      m_device->Send(packetToDst, toDst.GetNextHop (), LRWPAN_AODV_SAP);
    }
}

void
AODVLrWpanRoutingProtocol::SendReplyAck (Mac16Address neighbor)
{
  NS_LOG_FUNCTION (this << " to " << neighbor);
  RrepAckHeader h;
  LrWpanTypeHeader typeHeader (AODVTYPE_RREP_ACK);
  Ptr<Packet> packet = Create<Packet> ();
  SocketIpTtlTag tag;
  tag.SetTtl (1);
  packet->AddPacketTag (tag);
  packet->AddHeader (h);
  packet->AddHeader (typeHeader);
  LrWpanRoutingTableEntry toNeighbor;
  m_routingTable.LookupRoute (neighbor, toNeighbor);
  m_device->Send(packet, neighbor, LRWPAN_AODV_SAP);
}

void
AODVLrWpanRoutingProtocol::RecvReply (Ptr<Packet> p, Mac16Address receiver, Mac16Address sender)
{
  NS_LOG_FUNCTION (this << " src " << sender);
  LrWpanRrepHeader rrepHeader;
  p->RemoveHeader (rrepHeader);
  Mac16Address dst = rrepHeader.GetDst ();
  NS_LOG_LOGIC ("RREP destination " << dst << " RREP origin " << rrepHeader.GetOrigin ());

  uint8_t hop = rrepHeader.GetHopCount () + 1;
  rrepHeader.SetHopCount (hop);

  // If RREP is Hello message
  if (dst == rrepHeader.GetOrigin ())
    {
      ProcessHello (rrepHeader, receiver);
      return;
    }

  /*
   * If the route table entry to the destination is created or updated, then the following actions occur:
   * -  the route is marked as active,
   * -  the destination sequence number is marked as valid,
   * -  the next hop in the route entry is assigned to be the node from which the RREP is received,
   *    which is indicated by the source IP address field in the IP header,
   * -  the hop count is set to the value of the hop count from RREP message + 1
   * -  the expiry time is set to the current time plus the value of the Lifetime in the RREP message,
   * -  and the destination sequence number is the Destination Sequence Number in the RREP message.
   */
  LrWpanRoutingTableEntry newEntry (/*device=*/ m_device, /*dst=*/ dst, /*validSeqNo=*/ true, /*seqno=*/ rrepHeader.GetDstSeqno (),
                                          /*hop=*/ hop,
                                          /*nextHop=*/ sender, /*lifeTime=*/ rrepHeader.GetLifeTime ());
  LrWpanRoutingTableEntry toDst;
  if (m_routingTable.LookupRoute (dst, toDst))
    {
      /*
       * The existing entry is updated only in the following circumstances:
       * (i) the sequence number in the routing table is marked as invalid in route table entry.
       */
      if (!toDst.GetValidSeqNo ())
        {
          m_routingTable.Update (newEntry);
        }
      // (ii)the Destination Sequence Number in the RREP is greater than the node's copy of the destination sequence number and the known value is valid,
      else if ((int32_t (rrepHeader.GetDstSeqno ()) - int32_t (toDst.GetSeqNo ())) > 0)
        {
          m_routingTable.Update (newEntry);
        }
      else
        {
          // (iii) the sequence numbers are the same, but the route is marked as inactive.
          if ((rrepHeader.GetDstSeqno () == toDst.GetSeqNo ()) && (toDst.GetFlag () != VALID))
            {
              m_routingTable.Update (newEntry);
            }
          // (iv)  the sequence numbers are the same, and the New Hop Count is smaller than the hop count in route table entry.
          else if ((rrepHeader.GetDstSeqno () == toDst.GetSeqNo ()) && (hop < toDst.GetHop ()))
            {
              m_routingTable.Update (newEntry);
            }
        }
    }
  else
    {
      // The forward route for this destination is created if it does not already exist.
      NS_LOG_LOGIC ("add new route");
      m_routingTable.AddRoute (newEntry);
    }
  // Acknowledge receipt of the RREP by sending a RREP-ACK message back
  if (rrepHeader.GetAckRequired ())
    {
      SendReplyAck (sender);
      rrepHeader.SetAckRequired (false);
    }
  NS_LOG_LOGIC ("receiver " << receiver << " origin " << rrepHeader.GetOrigin ());
  if (IsMyOwnAddress (rrepHeader.GetOrigin ()))
    {
      if (toDst.GetFlag () == IN_SEARCH)
        {
          m_routingTable.Update (newEntry);
          m_addressReqTimer[dst].Remove ();
          m_addressReqTimer.erase (dst);
        }
      m_routingTable.LookupRoute (dst, toDst);
      SendPacketFromQueue (dst, toDst.GetRoute ());
      return;
    }

  LrWpanRoutingTableEntry toOrigin;
  if (!m_routingTable.LookupRoute (rrepHeader.GetOrigin (), toOrigin) || toOrigin.GetFlag () == IN_SEARCH)
    {
      return; // Impossible! drop.
    }
  toOrigin.SetLifeTime (std::max (m_activeRouteTimeout, toOrigin.GetLifeTime ()));
  m_routingTable.Update (toOrigin);

  // Update information about precursors
  if (m_routingTable.LookupValidRoute (rrepHeader.GetDst (), toDst))
    {
      toDst.InsertPrecursor (toOrigin.GetNextHop ());
      m_routingTable.Update (toDst);

      LrWpanRoutingTableEntry toNextHopToDst;
      m_routingTable.LookupRoute (toDst.GetNextHop (), toNextHopToDst);
      toNextHopToDst.InsertPrecursor (toOrigin.GetNextHop ());
      m_routingTable.Update (toNextHopToDst);

      toOrigin.InsertPrecursor (toDst.GetNextHop ());
      m_routingTable.Update (toOrigin);

      LrWpanRoutingTableEntry toNextHopToOrigin;
      m_routingTable.LookupRoute (toOrigin.GetNextHop (), toNextHopToOrigin);
      toNextHopToOrigin.InsertPrecursor (toDst.GetNextHop ());
      m_routingTable.Update (toNextHopToOrigin);
    }
  SocketIpTtlTag tag;
  p->RemovePacketTag (tag);
  if (tag.GetTtl () < 2)
    {
      NS_LOG_DEBUG ("TTL exceeded. Drop RREP destination " << dst << " origin " << rrepHeader.GetOrigin ());
      return;
    }

  Ptr<Packet> packet = Create<Packet> ();
  SocketIpTtlTag ttl;
  ttl.SetTtl (tag.GetTtl () - 1);
  packet->AddPacketTag (ttl);
  packet->AddHeader (rrepHeader);
  LrWpanTypeHeader tHeader (AODVTYPE_RREP);
  packet->AddHeader (tHeader);
  m_device->Send(packet, toOrigin.GetNextHop (), LRWPAN_AODV_SAP);
}

void
AODVLrWpanRoutingProtocol::RecvReplyAck (Mac16Address neighbor)
{
  NS_LOG_FUNCTION (this);
  LrWpanRoutingTableEntry rt;
  if (m_routingTable.LookupRoute (neighbor, rt))
    {
      rt.m_ackTimer.Cancel ();
      rt.SetFlag (VALID);
      m_routingTable.Update (rt);
    }
}

void
AODVLrWpanRoutingProtocol::ProcessHello (LrWpanRrepHeader const & rrepHeader, Mac16Address receiver )
{
  NS_LOG_FUNCTION (this << "from " << rrepHeader.GetDst ());
  /*
   *  Whenever a node receives a Hello message from a neighbor, the node
   * SHOULD make sure that it has an active route to the neighbor, and
   * create one if necessary.
   */
  LrWpanRoutingTableEntry toNeighbor;
  if (!m_routingTable.LookupRoute (rrepHeader.GetDst (), toNeighbor))
    {
      LrWpanRoutingTableEntry newEntry (/*device=*/ m_device, /*dst=*/ rrepHeader.GetDst (), /*validSeqNo=*/ true, /*seqno=*/ rrepHeader.GetDstSeqno (),
                                              /*hop=*/ 1, /*nextHop=*/ rrepHeader.GetDst (), /*lifeTime=*/ rrepHeader.GetLifeTime ());
      m_routingTable.AddRoute (newEntry);
    }
  else
    {
      toNeighbor.SetLifeTime (std::max (Time (m_allowedHelloLoss * m_helloInterval), toNeighbor.GetLifeTime ()));
      toNeighbor.SetSeqNo (rrepHeader.GetDstSeqno ());
      toNeighbor.SetValidSeqNo (true);
      toNeighbor.SetFlag (VALID);
      toNeighbor.SetOutputDevice (m_device);
      toNeighbor.SetHop (1);
      toNeighbor.SetNextHop (rrepHeader.GetDst ());
      m_routingTable.Update (toNeighbor);
    }
  if (m_enableHello)
    {
      m_nb.Update (rrepHeader.GetDst (), Time (m_allowedHelloLoss * m_helloInterval));
    }
}

void
AODVLrWpanRoutingProtocol::RecvError (Ptr<Packet> p, Mac16Address src )
{
  NS_LOG_FUNCTION (this << " from " << src);
  RerrHeader rerrHeader;
  p->RemoveHeader (rerrHeader);
  std::map<Mac16Address, uint32_t> dstWithNextHopSrc;
  std::map<Mac16Address, uint32_t> unreachable;
  m_routingTable.GetListOfDestinationWithNextHop (src, dstWithNextHopSrc);
  std::pair<Mac16Address, uint32_t> un;
  while (rerrHeader.RemoveUnDestination (un))
    {
      for (std::map<Mac16Address, uint32_t>::const_iterator i =
             dstWithNextHopSrc.begin (); i != dstWithNextHopSrc.end (); ++i)
        {
          if (i->first == un.first)
            {
              unreachable.insert (un);
            }
        }
    }

  std::vector<Mac16Address> precursors;
  for (std::map<Mac16Address, uint32_t>::const_iterator i = unreachable.begin ();
       i != unreachable.end (); )
    {
      if (!rerrHeader.AddUnDestination (i->first, i->second))
        {
          LrWpanTypeHeader typeHeader (AODVTYPE_RERR);
          Ptr<Packet> packet = Create<Packet> ();
          SocketIpTtlTag tag;
          tag.SetTtl (1);
          packet->AddPacketTag (tag);
          packet->AddHeader (rerrHeader);
          packet->AddHeader (typeHeader);
          SendRerrMessage (packet, precursors);
          rerrHeader.Clear ();
        }
      else
        {
          LrWpanRoutingTableEntry toDst;
          m_routingTable.LookupRoute (i->first, toDst);
          toDst.GetPrecursors (precursors);
          ++i;
        }
    }
  if (rerrHeader.GetDestCount () != 0)
    {
      LrWpanTypeHeader typeHeader (AODVTYPE_RERR);
      Ptr<Packet> packet = Create<Packet> ();
      SocketIpTtlTag tag;
      tag.SetTtl (1);
      packet->AddPacketTag (tag);
      packet->AddHeader (rerrHeader);
      packet->AddHeader (typeHeader);
      SendRerrMessage (packet, precursors);
    }
  m_routingTable.InvalidateRoutesWithDst (unreachable);
}

void
AODVLrWpanRoutingProtocol::RouteRequestTimerExpire (Mac16Address dst)
{
  NS_LOG_LOGIC (this);
  LrWpanRoutingTableEntry toDst;
  if (m_routingTable.LookupValidRoute (dst, toDst))
    {
      SendPacketFromQueue (dst, toDst.GetRoute ());
      NS_LOG_LOGIC ("route to " << dst << " found");
      return;
    }
  /*
   *  If a route discovery has been attempted RreqRetries times at the maximum TTL without
   *  receiving any RREP, all data packets destined for the corresponding destination SHOULD be
   *  dropped from the buffer and a Destination Unreachable message SHOULD be delivered to the application.
   */
  if (toDst.GetRreqCnt () == m_rreqRetries)
    {
      NS_LOG_LOGIC ("route discovery to " << dst << " has been attempted RreqRetries (" << m_rreqRetries << ") times with ttl " << m_netDiameter);
      m_addressReqTimer.erase (dst);
      m_routingTable.DeleteRoute (dst);
      NS_LOG_DEBUG ("Route not found. Drop all packets with dst " << dst);
      m_queue.DropPacketWithDst (dst);
      return;
    }

  if (toDst.GetFlag () == IN_SEARCH)
    {
      NS_LOG_LOGIC ("Resend RREQ to " << dst << " previous ttl " << toDst.GetHop ());
      SendRequest (dst);
    }
  else
    {
      NS_LOG_DEBUG ("Route down. Stop search. Drop packet with destination " << dst);
      m_addressReqTimer.erase (dst);
      m_routingTable.DeleteRoute (dst);
      m_queue.DropPacketWithDst (dst);
    }
}

void
AODVLrWpanRoutingProtocol::HelloTimerExpire ()
{
  NS_LOG_FUNCTION (this);
  Time offset = Time (Seconds (0));
  if (m_lastBcastTime > Time (Seconds (0)))
    {
      offset = Simulator::Now () - m_lastBcastTime;
      NS_LOG_DEBUG ("Hello deferred due to last bcast at:" << m_lastBcastTime);
    }
  else
    {
      SendHello ();
    }
  m_htimer.Cancel ();
  Time diff = m_helloInterval - offset;
  m_htimer.Schedule (std::max (Time (Seconds (0)), diff));
  m_lastBcastTime = Time (Seconds (0));
}

void
AODVLrWpanRoutingProtocol::RreqRateLimitTimerExpire ()
{
  NS_LOG_FUNCTION (this);
  m_rreqCount = 0;
  m_rreqRateLimitTimer.Schedule (Seconds (1));
}

void
AODVLrWpanRoutingProtocol::RerrRateLimitTimerExpire ()
{
  NS_LOG_FUNCTION (this);
  m_rerrCount = 0;
  m_rerrRateLimitTimer.Schedule (Seconds (1));
}

void
AODVLrWpanRoutingProtocol::AckTimerExpire (Mac16Address neighbor, Time blacklistTimeout)
{
  NS_LOG_FUNCTION (this);
  m_routingTable.MarkLinkAsUnidirectional (neighbor, blacklistTimeout);
}

void
AODVLrWpanRoutingProtocol::SendHello ()
{
  NS_LOG_FUNCTION (this);
  /* Broadcast a RREP with TTL = 1 with the RREP message fields set as follows:
   *   Destination IP Address         The node's IP address.
   *   Destination Sequence Number    The node's latest sequence number.
   *   Hop Count                      0
   *   Lifetime                       AllowedHelloLoss * HelloInterval
   */

  LrWpanRrepHeader helloHeader (/*prefix size=*/ 0, /*hops=*/ 0, /*dst=*/ m_device->GetMac()->GetShortAddress(), /*dst seqno=*/ m_seqNo,
										   /*origin=*/ m_device->GetMac()->GetShortAddress(),/*lifetime=*/ Time (m_allowedHelloLoss * m_helloInterval));
  Ptr<Packet> packet = Create<Packet> ();
  SocketIpTtlTag tag;
  tag.SetTtl (1);
  packet->AddPacketTag (tag);
  packet->AddHeader (helloHeader);
  LrWpanTypeHeader tHeader (AODVTYPE_RREP);
  packet->AddHeader (tHeader);
  // Send to all-hosts broadcast if on FF:FF addr, subnet-directed otherwise
  Mac16Address destination = Mac16Address("FF:FF");
  Time jitter = Time (MilliSeconds (m_uniformRandomVariable->GetInteger (0, 10)));
  Simulator::Schedule (jitter, &AODVLrWpanRoutingProtocol::SendTo, this, packet, destination);
}

void
AODVLrWpanRoutingProtocol::SendPacketFromQueue (Mac16Address dst, Ptr<LrWpanRoute> route)
{
  NS_LOG_FUNCTION (this);
  LrWpanQueueEntry queueEntry;
  while (m_queue.Dequeue (dst, queueEntry))
    {
      DeferredRouteOutputTag tag;
      Ptr<Packet> p = ConstCast<Packet> (queueEntry.GetPacket ());
//      if (p->RemovePacketTag (tag)
//          && tag.GetInterface () != -1
//          && tag.GetInterface () != m_lrwpan->GetInterfaceForDevice (route->GetOutputDevice ()))
//        {
//          NS_LOG_DEBUG ("Output device doesn't match. Dropped.");
//          return;
//        }
      UnicastForwardCallback ucb = queueEntry.GetUnicastForwardCallback ();
      LrWpanMacHeader header = queueEntry.GetLrWpanMacHeader();
      header.SetSrcAddrFields(route->GetPanId(), route->GetSource ());
      ucb (route, p, header);
    }
}

void
AODVLrWpanRoutingProtocol::SendRerrWhenBreaksLinkToNextHop (Mac16Address nextHop)
{
  NS_LOG_FUNCTION (this << nextHop);
  RerrHeader rerrHeader;
  std::vector<Mac16Address> precursors;
  std::map<Mac16Address, uint32_t> unreachable;

  LrWpanRoutingTableEntry toNextHop;
  if (!m_routingTable.LookupRoute (nextHop, toNextHop))
    {
      return;
    }
  toNextHop.GetPrecursors (precursors);
  rerrHeader.AddUnDestination (nextHop, toNextHop.GetSeqNo ());
  m_routingTable.GetListOfDestinationWithNextHop (nextHop, unreachable);
  for (std::map<Mac16Address, uint32_t>::const_iterator i = unreachable.begin (); i
       != unreachable.end (); )
    {
      if (!rerrHeader.AddUnDestination (i->first, i->second))
        {
          NS_LOG_LOGIC ("Send RERR message with maximum size.");
          LrWpanTypeHeader typeHeader (AODVTYPE_RERR);
          Ptr<Packet> packet = Create<Packet> ();
          SocketIpTtlTag tag;
          tag.SetTtl (1);
          packet->AddPacketTag (tag);
          packet->AddHeader (rerrHeader);
          packet->AddHeader (typeHeader);
          SendRerrMessage (packet, precursors);
          rerrHeader.Clear ();
        }
      else
        {
          LrWpanRoutingTableEntry toDst;
          m_routingTable.LookupRoute (i->first, toDst);
          toDst.GetPrecursors (precursors);
          ++i;
        }
    }
  if (rerrHeader.GetDestCount () != 0)
    {
      LrWpanTypeHeader typeHeader (AODVTYPE_RERR);
      Ptr<Packet> packet = Create<Packet> ();
      SocketIpTtlTag tag;
      tag.SetTtl (1);
      packet->AddPacketTag (tag);
      packet->AddHeader (rerrHeader);
      packet->AddHeader (typeHeader);
      SendRerrMessage (packet, precursors);
    }
  unreachable.insert (std::make_pair (nextHop, toNextHop.GetSeqNo ()));
  m_routingTable.InvalidateRoutesWithDst (unreachable);
}

void
AODVLrWpanRoutingProtocol::SendRerrWhenNoRouteToForward (Mac16Address dst,
                                               uint32_t dstSeqNo, Mac16Address origin)
{
  NS_LOG_FUNCTION (this);
  // A node SHOULD NOT originate more than RERR_RATELIMIT RERR messages per second.
  if (m_rerrCount == m_rerrRateLimit)
    {
      // Just make sure that the RerrRateLimit timer is running and will expire
      NS_ASSERT (m_rerrRateLimitTimer.IsRunning ());
      // discard the packet and return
      NS_LOG_LOGIC ("RerrRateLimit reached at " << Simulator::Now ().GetSeconds () << " with timer delay left "
                                                << m_rerrRateLimitTimer.GetDelayLeft ().GetSeconds ()
                                                << "; suppressing RERR");
      return;
    }
  RerrHeader rerrHeader;
  rerrHeader.AddUnDestination (dst, dstSeqNo);
  LrWpanRoutingTableEntry toOrigin;
  Ptr<Packet> packet = Create<Packet> ();
  SocketIpTtlTag tag;
  tag.SetTtl (1);
  packet->AddPacketTag (tag);
  packet->AddHeader (rerrHeader);
  packet->AddHeader (LrWpanTypeHeader (AODVTYPE_RERR));
  if (m_routingTable.LookupValidRoute (origin, toOrigin))
    {
      NS_LOG_LOGIC ("Unicast RERR to the source of the data transmission");
      m_device->Send(packet, toOrigin.GetNextHop (), LRWPAN_AODV_SAP);
    }
  else
    {
		NS_LOG_LOGIC(
				"Broadcast RERR message from interface " << m_device->GetMac()->GetShortAddress());
		// Send to all-hosts broadcast if on /32 addr, subnet-directed otherwise
		Mac16Address destination;
		destination = Mac16Address("FF:FF");
		m_device->Send(packet, destination, LRWPAN_AODV_SAP);
    }
}

void
AODVLrWpanRoutingProtocol::SendRerrMessage (Ptr<Packet> packet, std::vector<Mac16Address> precursors)
{
  NS_LOG_FUNCTION (this);

  if (precursors.empty ())
    {
      NS_LOG_LOGIC ("No precursors");
      return;
    }
  // A node SHOULD NOT originate more than RERR_RATELIMIT RERR messages per second.
  if (m_rerrCount == m_rerrRateLimit)
    {
      // Just make sure that the RerrRateLimit timer is running and will expire
      NS_ASSERT (m_rerrRateLimitTimer.IsRunning ());
      // discard the packet and return
      NS_LOG_LOGIC ("RerrRateLimit reached at " << Simulator::Now ().GetSeconds () << " with timer delay left "
                                                << m_rerrRateLimitTimer.GetDelayLeft ().GetSeconds ()
                                                << "; suppressing RERR");
      return;
    }
  // If there is only one precursor, RERR SHOULD be unicast toward that precursor
  if (precursors.size () == 1)
    {
      LrWpanRoutingTableEntry toPrecursor;
      if (m_routingTable.LookupValidRoute (precursors.front (), toPrecursor))
        {
          NS_LOG_LOGIC ("one precursor => unicast RERR to " << toPrecursor.GetDestination() << " from " << m_device->GetMac()->GetShortAddress());
          Simulator::Schedule (Time (MilliSeconds (m_uniformRandomVariable->GetInteger (0, 10))), &AODVLrWpanRoutingProtocol::SendTo, this, packet, precursors.front ());
          m_rerrCount++;
        }
      return;
    }

  //  Should only transmit RERR on those interfaces which have precursor nodes for the broken route
  // TODO: revisar esta parte, pienso que no es necesario
//  LrWpanRoutingTableEntry toPrecursor;
//  for (std::vector<Mac16Address>::const_iterator i = precursors.begin (); i != precursors.end (); ++i)
//    {
//      if (m_routingTable.LookupValidRoute (*i, toPrecursor))
//        {
//          ifaces.push_back (toPrecursor.GetInterface ());
//        }
//    }

  NS_LOG_LOGIC ("Broadcast RERR message from interface " << m_device->GetMac()->GetShortAddress());
  // std::cout << "Broadcast RERR message from interface " << i->GetLocal () << std::endl;
  // Send to all-hosts broadcast if on /32 addr, subnet-directed otherwise
  Ptr<Packet> p = packet->Copy ();
  Mac16Address destination = Mac16Address("FF:FF");
  Simulator::Schedule (Time (MilliSeconds (m_uniformRandomVariable->GetInteger (0, 10))), &AODVLrWpanRoutingProtocol::SendTo, this, p, destination);

}

Ptr<Socket>
AODVLrWpanRoutingProtocol::FindSocketWithInterfaceAddress (Ipv4InterfaceAddress addr ) const
{
  NS_LOG_FUNCTION (this << addr);
  for (std::map<Ptr<Socket>, Ipv4InterfaceAddress>::const_iterator j =
         m_socketAddresses.begin (); j != m_socketAddresses.end (); ++j)
    {
      Ptr<Socket> socket = j->first;
      Ipv4InterfaceAddress iface = j->second;
      if (iface == addr)
        {
          return socket;
        }
    }
  Ptr<Socket> socket;
  return socket;
}

Ptr<Socket>
AODVLrWpanRoutingProtocol::FindSubnetBroadcastSocketWithInterfaceAddress (Ipv4InterfaceAddress addr ) const
{
  NS_LOG_FUNCTION (this << addr);
  for (std::map<Ptr<Socket>, Ipv4InterfaceAddress>::const_iterator j =
         m_socketSubnetBroadcastAddresses.begin (); j != m_socketSubnetBroadcastAddresses.end (); ++j)
    {
      Ptr<Socket> socket = j->first;
      Ipv4InterfaceAddress iface = j->second;
      if (iface == addr)
        {
          return socket;
        }
    }
  Ptr<Socket> socket;
  return socket;
}

void
AODVLrWpanRoutingProtocol::DoInitialize (void)
{
  NS_LOG_FUNCTION (this);
  Simulator::ScheduleNow (&AODVLrWpanRoutingProtocol::Start, this);
  uint32_t startTime;
  if (m_enableHello)
    {
      m_htimer.SetFunction (&AODVLrWpanRoutingProtocol::HelloTimerExpire, this);
      startTime = m_uniformRandomVariable->GetInteger (0, 100);
      NS_LOG_DEBUG ("Starting at time " << startTime << "ms");
      m_htimer.Schedule (MilliSeconds (startTime));
    }
  LrWpanRoutingProtocol::DoInitialize ();
}

} //namespace aodv
} //namespace ns3
