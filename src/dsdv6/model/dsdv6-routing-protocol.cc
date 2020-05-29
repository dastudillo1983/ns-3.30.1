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

#include "dsdv6-routing-protocol.h"
#include "ns3/log.h"
#include "ns3/inet6-socket-address.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/wifi-net-device.h"
#include "ns3/boolean.h"
#include "ns3/double.h"
#include "ns3/uinteger.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("Dsdv6RoutingProtocol");

namespace dsdv6 {

NS_OBJECT_ENSURE_REGISTERED (RoutingProtocol);

/// UDP Port for DSDV6 control traffic
const uint32_t RoutingProtocol::DSDV6_PORT = 269;

/// Tag used by DSDV6 implementation
struct DeferredRouteOutputTag : public Tag
{
  /// Positive if output device is fixed in RouteOutput
  int32_t oif;

  DeferredRouteOutputTag (int32_t o = -1)
    : Tag (),
      oif (o)
  {
  }

  static TypeId
  GetTypeId ()
  {
    static TypeId tid = TypeId ("ns3::dsdv6::DeferredRouteOutputTag")
      .SetParent<Tag> ()
      .SetGroupName ("Dsdv6")
      .AddConstructor<DeferredRouteOutputTag> ()
    ;
    return tid;
  }

  TypeId
  GetInstanceTypeId () const
  {
    return GetTypeId ();
  }

  uint32_t
  GetSerializedSize () const
  {
    return sizeof(int32_t);
  }

  void
  Serialize (TagBuffer i) const
  {
    i.WriteU32 (oif);
  }

  void
  Deserialize (TagBuffer i)
  {
    oif = i.ReadU32 ();
  }

  void
  Print (std::ostream &os) const
  {
    os << "DeferredRouteOutputTag: output interface = " << oif;
  }
};

TypeId
RoutingProtocol::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::dsdv6::RoutingProtocol")
    .SetParent<Ipv6RoutingProtocol> ()
    .SetGroupName ("Dsdv6")
    .AddConstructor<RoutingProtocol> ()
    .AddAttribute ("PeriodicUpdateInterval","Periodic interval between exchange of full routing tables among nodes. ",
                   TimeValue (Seconds (15)),
                   MakeTimeAccessor (&RoutingProtocol::m_periodicUpdateInterval),
                   MakeTimeChecker ())
    .AddAttribute ("SettlingTime", "Minimum time an update is to be stored in adv table before sending out"
                   "in case of change in metric (in seconds)",
                   TimeValue (Seconds (5)),
                   MakeTimeAccessor (&RoutingProtocol::m_settlingTime),
                   MakeTimeChecker ())
    .AddAttribute ("MaxQueueLen", "Maximum number of packets that we allow a routing protocol to buffer.",
                   UintegerValue (500 /*assuming maximum nodes in simulation is 100*/),
                   MakeUintegerAccessor (&RoutingProtocol::m_maxQueueLen),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("MaxQueuedPacketsPerDst", "Maximum number of packets that we allow per destination to buffer.",
                   UintegerValue (5),
                   MakeUintegerAccessor (&RoutingProtocol::m_maxQueuedPacketsPerDst),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("MaxQueueTime","Maximum time packets can be queued (in seconds)",
                   TimeValue (Seconds (30)),
                   MakeTimeAccessor (&RoutingProtocol::m_maxQueueTime),
                   MakeTimeChecker ())
    .AddAttribute ("EnableBuffering","Enables buffering of data packets if no route to destination is available",
                   BooleanValue (true),
                   MakeBooleanAccessor (&RoutingProtocol::SetEnableBufferFlag,
                                        &RoutingProtocol::GetEnableBufferFlag),
                   MakeBooleanChecker ())
    .AddAttribute ("EnableWST","Enables Weighted Settling Time for the updates before advertising",
                   BooleanValue (true),
                   MakeBooleanAccessor (&RoutingProtocol::SetWSTFlag,
                                        &RoutingProtocol::GetWSTFlag),
                   MakeBooleanChecker ())
    .AddAttribute ("Holdtimes","Times the forwarding Interval to purge the route.",
                   UintegerValue (3),
                   MakeUintegerAccessor (&RoutingProtocol::Holdtimes),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("WeightedFactor","WeightedFactor for the settling time if Weighted Settling Time is enabled",
                   DoubleValue (0.875),
                   MakeDoubleAccessor (&RoutingProtocol::m_weightedFactor),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("EnableRouteAggregation","Enables Weighted Settling Time for the updates before advertising",
                   BooleanValue (false),
                   MakeBooleanAccessor (&RoutingProtocol::SetEnableRAFlag,
                                        &RoutingProtocol::GetEnableRAFlag),
                   MakeBooleanChecker ())
    .AddAttribute ("RouteAggregationTime","Time to aggregate updates before sending them out (in seconds)",
                   TimeValue (Seconds (1)),
                   MakeTimeAccessor (&RoutingProtocol::m_routeAggregationTime),
                   MakeTimeChecker ());
  return tid;
}

void
RoutingProtocol::SetEnableBufferFlag (bool f)
{
  EnableBuffering = f;
}
bool
RoutingProtocol::GetEnableBufferFlag () const
{
  return EnableBuffering;
}
void
RoutingProtocol::SetWSTFlag (bool f)
{
  EnableWST = f;
}
bool
RoutingProtocol::GetWSTFlag () const
{
  return EnableWST;
}
void
RoutingProtocol::SetEnableRAFlag (bool f)
{
  EnableRouteAggregation = f;
}
bool
RoutingProtocol::GetEnableRAFlag () const
{
  return EnableRouteAggregation;
}

int64_t
RoutingProtocol::AssignStreams (int64_t stream)
{
  NS_LOG_FUNCTION (this << stream);
  m_uniformRandomVariable->SetStream (stream);
  return 1;
}

RoutingProtocol::RoutingProtocol ()
  : m_routingTable (),
    m_advRoutingTable (),
    m_queue (),
    m_periodicUpdateTimer (Timer::CANCEL_ON_DESTROY)
{
  m_uniformRandomVariable = CreateObject<UniformRandomVariable> ();
}

RoutingProtocol::~RoutingProtocol ()
{
}

void
RoutingProtocol::DoDispose ()
{
  m_ipv6 = 0;
  for (std::map<Ptr<Socket>, Ipv6InterfaceAddress>::iterator iter = m_socketAddresses.begin (); iter
       != m_socketAddresses.end (); iter++)
    {
      iter->first->Close ();
    }
  m_socketAddresses.clear ();
  Ipv6RoutingProtocol::DoDispose ();
}

void
RoutingProtocol::PrintRoutingTable (Ptr<OutputStreamWrapper> stream, Time::Unit unit) const
{
  *stream->GetStream () << "Node: " << m_ipv6->GetObject<Node> ()->GetId ()
                        << ", Time: " << Now ().As (Time::S)
                        << ", Local time: " << GetObject<Node> ()->GetLocalTime ().As (Time::S)
                        << ", DSDV6 Routing table" << std::endl;

  m_routingTable.Print (stream);
  *stream->GetStream () << std::endl;
}

void
RoutingProtocol::Start ()
{
  m_queue.SetMaxPacketsPerDst (m_maxQueuedPacketsPerDst);
  m_queue.SetMaxQueueLen (m_maxQueueLen);
  m_queue.SetQueueTimeout (m_maxQueueTime);
  m_routingTable.Setholddowntime (Time (Holdtimes * m_periodicUpdateInterval));
  m_advRoutingTable.Setholddowntime (Time (Holdtimes * m_periodicUpdateInterval));
  m_scb = MakeCallback (&RoutingProtocol::Send,this);
  m_ecb = MakeCallback (&RoutingProtocol::Drop,this);
  m_periodicUpdateTimer.SetFunction (&RoutingProtocol::SendPeriodicUpdate,this);
  m_periodicUpdateTimer.Schedule (MicroSeconds (m_uniformRandomVariable->GetInteger (0,1000)));
}

Ptr<Ipv6Route>
RoutingProtocol::RouteOutput (Ptr<Packet> p,
                              const Ipv6Header &header,
                              Ptr<NetDevice> oif,
                              Socket::SocketErrno &sockerr)
{
  NS_LOG_FUNCTION (this << header << (oif ? oif->GetIfIndex () : 0));

  if (!p)
    {
      return LoopbackRoute (header,oif);
    }
  if (m_socketAddresses.empty ())
    {
      sockerr = Socket::ERROR_NOROUTETOHOST;
      NS_LOG_LOGIC ("No dsdv6 interfaces");
      Ptr<Ipv6Route> route;
      return route;
    }
  std::map<Ipv6Address, RoutingTableEntry> removedAddresses;
  sockerr = Socket::ERROR_NOTERROR;
  Ptr<Ipv6Route> route;
  Ipv6Address dst = header.GetDestinationAddress ();
  NS_LOG_DEBUG ("Packet Size: " << p->GetSize ()
                                << ", Packet id: " << p->GetUid () << ", Destination address in Packet: " << dst);
  RoutingTableEntry rt;
  m_routingTable.Purge (removedAddresses);
  for (std::map<Ipv6Address, RoutingTableEntry>::iterator rmItr = removedAddresses.begin ();
       rmItr != removedAddresses.end (); ++rmItr)
    {
      rmItr->second.SetEntriesChanged (true);
      rmItr->second.SetSeqNo (rmItr->second.GetSeqNo () + 1);
      m_advRoutingTable.AddRoute (rmItr->second);
    }
  if (!removedAddresses.empty ())
    {
      Simulator::Schedule (MicroSeconds (m_uniformRandomVariable->GetInteger (0,1000)),&RoutingProtocol::SendTriggeredUpdate,this);
    }
  if (m_routingTable.LookupRoute (dst,rt))
    {
      if (EnableBuffering)
        {
          LookForQueuedPackets (oif);
        }
      if (rt.GetHop () == 1)
        {
          route = rt.GetRoute ();
          NS_ASSERT (route != 0);
          NS_LOG_DEBUG ("A route exists from " << route->GetSource ()
                                               << " to neighboring destination "
                                               << route->GetDestination ());
          if (oif != 0 && route->GetOutputDevice () != oif)
            {
              NS_LOG_DEBUG ("Output device doesn't match. Dropped.");
              sockerr = Socket::ERROR_NOROUTETOHOST;
              return Ptr<Ipv6Route> ();
            }
          return route;
        }
      else
        {
          RoutingTableEntry newrt;
          if (m_routingTable.LookupRoute (rt.GetNextHop (),newrt))
            {
              route = newrt.GetRoute ();
              NS_ASSERT (route != 0);
              NS_LOG_DEBUG ("A route exists from " << route->GetSource ()
                                                   << " to destination " << dst << " via "
                                                   << rt.GetNextHop ());
              if (oif != 0 && route->GetOutputDevice () != oif)
                {
                  NS_LOG_DEBUG ("Output device doesn't match. Dropped.");
                  sockerr = Socket::ERROR_NOROUTETOHOST;
                  return Ptr<Ipv6Route> ();
                }
              return route;
            }
        }
    }

  if (EnableBuffering)
    {
      uint32_t iif = (oif ? m_ipv6->GetInterfaceForDevice (oif) : -1);
      DeferredRouteOutputTag tag (iif);
      if (!p->PeekPacketTag (tag))
        {
          p->AddPacketTag (tag);
        }
    }
  return LoopbackRoute (header,oif);
}

void
RoutingProtocol::DeferredRouteOutput (Ptr<const Packet> p,
                                      const Ipv6Header & header,
                                      UnicastForwardCallback ucb,
                                      ErrorCallback ecb)
{
  NS_LOG_FUNCTION (this << p << header);
  NS_ASSERT (p != 0 && p != Ptr<Packet> ());
  QueueEntry newEntry (p,header,ucb,ecb);
  bool result = m_queue.Enqueue (newEntry);
  if (result)
    {
      NS_LOG_DEBUG ("Added packet " << p->GetUid () << " to queue.");
    }
}

bool
RoutingProtocol::RouteInput (Ptr<const Packet> p,
                             const Ipv6Header &header,
                             Ptr<const NetDevice> idev,
                             UnicastForwardCallback ucb,
                             MulticastForwardCallback mcb,
                             LocalDeliverCallback lcb,
                             ErrorCallback ecb)
{
  NS_LOG_FUNCTION (m_mainAddress << " received packet " << p->GetUid ()
                                 << " from " << header.GetSourceAddress ()
                                 << " on interface " << idev->GetAddress ()
                                 << " to destination " << header.GetDestinationAddress ());
  if (m_socketAddresses.empty ())
    {
      NS_LOG_DEBUG ("No dsdv6 interfaces");
      return false;
    }
  NS_ASSERT (m_ipv6 != 0);
  // Check if input device supports IP
  NS_ASSERT (m_ipv6->GetInterfaceForDevice (idev) >= 0);
  int32_t iif = m_ipv6->GetInterfaceForDevice (idev);

  Ipv6Address dst = header.GetDestinationAddress ();
  Ipv6Address origin = header.GetSourceAddress ();

  // DSDV6 is not a multicast routing protocol
  if (dst.IsMulticast ())
    {
      return false;
    }

  // Deferred route request
  if (EnableBuffering == true && idev == m_lo)
    {
      DeferredRouteOutputTag tag;
      if (p->PeekPacketTag (tag))
        {
          DeferredRouteOutput (p,header,ucb,ecb);
          return true;
        }
    }
  for (std::map<Ptr<Socket>, Ipv6InterfaceAddress>::const_iterator j =
         m_socketAddresses.begin (); j != m_socketAddresses.end (); ++j)
    {
      Ipv6InterfaceAddress iface = j->second;
      if (origin == iface.GetAddress ())
        {
          return true;
        }
    }
  // LOCAL DELIVARY TO DSDV6 INTERFACES
  for (std::map<Ptr<Socket>, Ipv6InterfaceAddress>::const_iterator j = m_socketAddresses.begin (); j
       != m_socketAddresses.end (); ++j)
    {
      Ipv6InterfaceAddress iface = j->second;
      if (m_ipv6->GetInterfaceForAddress (iface.GetAddress ()) == iif)
        {
          if (dst == Ipv6Address ("FF02::1"))
            {
              Ptr<Packet> packet = p->Copy ();
              if (lcb.IsNull () == false)
                {
                  NS_LOG_LOGIC ("Broadcast local delivery to " << iface.GetAddress ());
                  lcb (p, header, iif);
                  // Fall through to additional processing
                }
              else
                {
                  NS_LOG_ERROR ("Unable to deliver packet locally due to null callback " << p->GetUid () << " from " << origin);
                  ecb (p, header, Socket::ERROR_NOROUTETOHOST);
                }
              if (header.GetHopLimit () > 1)
                {
                  NS_LOG_LOGIC ("Forward broadcast. TTL " << (uint16_t) header.GetHopLimit ());
                  RoutingTableEntry toBroadcast;
                  if (m_routingTable.LookupRoute (dst,toBroadcast,true))
                    {
                      Ptr<Ipv6Route> route = toBroadcast.GetRoute ();
                      ucb (idev, route,packet,header);
                    }
                  else
                    {
                      NS_LOG_DEBUG ("No route to forward. Drop packet " << p->GetUid ());
                    }
                }
              return true;
            }
        }
    }

  /*if (m_ipv6->IsDestinationAddress (dst, iif))
    {
      if (lcb.IsNull () == false)
        {
          NS_LOG_LOGIC ("Unicast local delivery to " << dst);
          lcb (p, header, iif);
        }
      else
        {
          NS_LOG_ERROR ("Unable to deliver packet locally due to null callback " << p->GetUid () << " from " << origin);
          ecb (p, header, Socket::ERROR_NOROUTETOHOST);
        }
      return true;
    }*/
  RoutingTableEntry toDst;
  if (m_routingTable.LookupRoute (dst,toDst))
    {
      RoutingTableEntry ne;
      if (m_routingTable.LookupRoute (toDst.GetNextHop (),ne))
        {
          Ptr<Ipv6Route> route = ne.GetRoute ();
          NS_LOG_LOGIC (m_mainAddress << " is forwarding packet " << p->GetUid ()
                                      << " to " << dst
                                      << " from " << header.GetSourceAddress ()
                                      << " via nexthop neighbor " << toDst.GetNextHop ());
          ucb (idev,route,p,header);
          return true;
        }
    }
  NS_LOG_LOGIC ("Drop packet " << p->GetUid ()
                               << " as there is no route to forward it.");
  return false;
}

Ptr<Ipv6Route>
RoutingProtocol::LoopbackRoute (const Ipv6Header & hdr, Ptr<NetDevice> oif) const
{
  NS_ASSERT (m_lo != 0);
  Ptr<Ipv6Route> rt = Create<Ipv6Route> ();
  rt->SetDestination (hdr.GetDestinationAddress ());
  // rt->SetSource (hdr.GetSource ());
  //
  // Source address selection here is tricky.  The loopback route is
  // returned when DSDV6 does not have a route; this causes the packet
  // to be looped back and handled (cached) in RouteInput() method
  // while a route is found. However, connection-oriented protocols
  // like TCP need to create an endpoint four-tuple (src, src port,
  // dst, dst port) and create a pseudo-header for checksumming.  So,
  // DSDV6 needs to guess correctly what the eventual source address
  // will be.
  //
  // For single interface, single address nodes, this is not a problem.
  // When there are possibly multiple outgoing interfaces, the policy
  // implemented here is to pick the first available DSDV6 interface.
  // If RouteOutput() caller specified an outgoing interface, that
  // further constrains the selection of source address
  //
  std::map<Ptr<Socket>, Ipv6InterfaceAddress>::const_iterator j = m_socketAddresses.begin ();
  if (oif)
    {
      // Iterate to find an address on the oif device
      for (j = m_socketAddresses.begin (); j != m_socketAddresses.end (); ++j)
        {
          Ipv6Address addr = j->second.GetAddress ();
          int32_t interface = m_ipv6->GetInterfaceForAddress (addr);
          if (oif == m_ipv6->GetNetDevice (static_cast<uint32_t> (interface)))
            {
              rt->SetSource (addr);
              break;
            }
        }
    }
  else
    {
      rt->SetSource (j->second.GetAddress ());
    }
  NS_ASSERT_MSG (rt->GetSource () != Ipv6Address (), "Valid DSDV6 source address not found");
  rt->SetGateway (Ipv6Address ("::"));
  rt->SetOutputDevice (m_lo);
  return rt;
}

void
RoutingProtocol::RecvDsdv6 (Ptr<Socket> socket)
{
  Address sourceAddress;
  Ptr<Packet> advpacket = Create<Packet> ();
  Ptr<Packet> packet = socket->RecvFrom (sourceAddress);
  Inet6SocketAddress inetSourceAddr = Inet6SocketAddress::ConvertFrom (sourceAddress);
  Ipv6Address sender = inetSourceAddr.GetIpv6 ();
  Ipv6Address receiver = m_socketAddresses[socket].GetAddress ();
  Ptr<NetDevice> dev = m_ipv6->GetNetDevice (m_ipv6->GetInterfaceForAddress (receiver));
  uint32_t packetSize = packet->GetSize ();
  NS_LOG_FUNCTION (m_mainAddress << " received dsdv6 packet of size: " << packetSize
                                 << " and packet id: " << packet->GetUid ());
  uint32_t count = 0;
  for (; packetSize > 0; packetSize = packetSize - 24)
    {
      count = 0;
      Dsdv6Header dsdv6Header, tempDsdv6Header;
      packet->RemoveHeader (dsdv6Header);
      NS_LOG_DEBUG ("Processing new update for " << dsdv6Header.GetDst ());
      /*Verifying if the packets sent by me were returned back to me. If yes, discarding them!*/
      for (std::map<Ptr<Socket>, Ipv6InterfaceAddress>::const_iterator j = m_socketAddresses.begin (); j
           != m_socketAddresses.end (); ++j)
        {
          Ipv6InterfaceAddress interface = j->second;
          if (dsdv6Header.GetDst () == interface.GetAddress ())
            {
              if (dsdv6Header.GetDstSeqno () % 2 == 1)
                {
                  NS_LOG_DEBUG ("Sent Dsdv6 update back to the same Destination, "
                                "with infinite metric. Time left to send fwd update: "
                                << m_periodicUpdateTimer.GetDelayLeft ());
                  count++;
                }
              else
                {
                  NS_LOG_DEBUG ("Received update for my address. Discarding this.");
                  count++;
                }
            }
        }
      if (count > 0)
        {
          continue;
        }
      NS_LOG_DEBUG ("Received a DSDV6 packet from "
                    << sender << " to " << receiver << ". Details are: Destination: " << dsdv6Header.GetDst () << ", Seq No: "
                    << dsdv6Header.GetDstSeqno () << ", HopCount: " << dsdv6Header.GetHopCount ());
      RoutingTableEntry fwdTableEntry, advTableEntry;
      EventId event;
      bool permanentTableVerifier = m_routingTable.LookupRoute (dsdv6Header.GetDst (),fwdTableEntry);
      if (permanentTableVerifier == false)
        {
          if (dsdv6Header.GetDstSeqno () % 2 != 1)
            {
              NS_LOG_DEBUG ("Received New Route!");
              RoutingTableEntry newEntry (
                /*device=*/ dev, /*dst=*/
                dsdv6Header.GetDst (), /*seqno=*/
                dsdv6Header.GetDstSeqno (),
                /*iface=*/ m_ipv6->GetAddress (m_ipv6->GetInterfaceForAddress (receiver), 1),
                /*hops=*/ dsdv6Header.GetHopCount (), /*next hop=*/
                sender, /*lifetime=*/
                Simulator::Now (), /*settlingTime*/
                m_settlingTime, /*entries changed*/
                true);
              newEntry.SetFlag (VALID);
              m_routingTable.AddRoute (newEntry);
              NS_LOG_DEBUG ("New Route added to both tables");
              m_advRoutingTable.AddRoute (newEntry);
            }
          else
            {
              // received update not present in main routing table and also with infinite metric
              NS_LOG_DEBUG ("Discarding this update as this route is not present in "
                            "main routing table and received with infinite metric");
            }
        }
      else
        {
          if (!m_advRoutingTable.LookupRoute (dsdv6Header.GetDst (),advTableEntry))
            {
              RoutingTableEntry tr;
              std::map<Ipv6Address, RoutingTableEntry> allRoutes;
              m_advRoutingTable.GetListOfAllRoutes (allRoutes);
              for (std::map<Ipv6Address, RoutingTableEntry>::const_iterator i = allRoutes.begin (); i != allRoutes.end (); ++i)
                {
                  NS_LOG_DEBUG ("ADV table routes are:" << i->second.GetDestination ());
                }
              // present in fwd table and not in advtable
              m_advRoutingTable.AddRoute (fwdTableEntry);
              m_advRoutingTable.LookupRoute (dsdv6Header.GetDst (),advTableEntry);
            }
          if (dsdv6Header.GetDstSeqno () % 2 != 1)
            {
              if (dsdv6Header.GetDstSeqno () > advTableEntry.GetSeqNo ())
                {
                  // Received update with better seq number. Clear any old events that are running
                  if (m_advRoutingTable.ForceDeleteIpv6Event (dsdv6Header.GetDst ()))
                    {
                      NS_LOG_DEBUG ("Canceling the timer to update route with better seq number");
                    }
                  // if its a changed metric *nomatter* where the update came from, wait  for WST
                  if (dsdv6Header.GetHopCount () != advTableEntry.GetHop ())
                    {
                      advTableEntry.SetSeqNo (dsdv6Header.GetDstSeqno ());
                      advTableEntry.SetLifeTime (Simulator::Now ());
                      advTableEntry.SetFlag (VALID);
                      advTableEntry.SetEntriesChanged (true);
                      advTableEntry.SetNextHop (sender);
                      advTableEntry.SetHop (dsdv6Header.GetHopCount ());
                      NS_LOG_DEBUG ("Received update with better sequence number and changed metric.Waiting for WST");
                      Time tempSettlingtime = GetSettlingTime (dsdv6Header.GetDst ());
                      advTableEntry.SetSettlingTime (tempSettlingtime);
                      NS_LOG_DEBUG ("Added Settling Time:" << tempSettlingtime.GetSeconds ()
                                                           << "s as there is no event running for this route");
                      event = Simulator::Schedule (tempSettlingtime,&RoutingProtocol::SendTriggeredUpdate,this);
                      m_advRoutingTable.AddIpv6Event (dsdv6Header.GetDst (),event);
                      NS_LOG_DEBUG ("EventCreated EventUID: " << event.GetUid ());
                      // if received changed metric, use it but adv it only after wst
                      m_routingTable.Update (advTableEntry);
                      m_advRoutingTable.Update (advTableEntry);
                    }
                  else
                    {
                      // Received update with better seq number and same metric.
                      advTableEntry.SetSeqNo (dsdv6Header.GetDstSeqno ());
                      advTableEntry.SetLifeTime (Simulator::Now ());
                      advTableEntry.SetFlag (VALID);
                      advTableEntry.SetEntriesChanged (true);
                      advTableEntry.SetNextHop (sender);
                      advTableEntry.SetHop (dsdv6Header.GetHopCount ());
                      m_advRoutingTable.Update (advTableEntry);
                      NS_LOG_DEBUG ("Route with better sequence number and same metric received. Advertised without WST");
                    }
                }
              else if (dsdv6Header.GetDstSeqno () == advTableEntry.GetSeqNo ())
                {
                  if (dsdv6Header.GetHopCount () < advTableEntry.GetHop ())
                    {
                      /*Received update with same seq number and better hop count.
                       * As the metric is changed, we will have to wait for WST before sending out this update.
                       */
                      NS_LOG_DEBUG ("Canceling any existing timer to update route with same sequence number "
                                    "and better hop count");
                      m_advRoutingTable.ForceDeleteIpv6Event (dsdv6Header.GetDst ());
                      advTableEntry.SetSeqNo (dsdv6Header.GetDstSeqno ());
                      advTableEntry.SetLifeTime (Simulator::Now ());
                      advTableEntry.SetFlag (VALID);
                      advTableEntry.SetEntriesChanged (true);
                      advTableEntry.SetNextHop (sender);
                      advTableEntry.SetHop (dsdv6Header.GetHopCount ());
                      Time tempSettlingtime = GetSettlingTime (dsdv6Header.GetDst ());
                      advTableEntry.SetSettlingTime (tempSettlingtime);
                      NS_LOG_DEBUG ("Added Settling Time," << tempSettlingtime.GetSeconds ()
                                                           << " as there is no current event running for this route");
                      event = Simulator::Schedule (tempSettlingtime,&RoutingProtocol::SendTriggeredUpdate,this);
                      m_advRoutingTable.AddIpv6Event (dsdv6Header.GetDst (),event);
                      NS_LOG_DEBUG ("EventCreated EventUID: " << event.GetUid ());
                      // if received changed metric, use it but adv it only after wst
                      m_routingTable.Update (advTableEntry);
                      m_advRoutingTable.Update (advTableEntry);
                    }
                  else
                    {
                      /*Received update with same seq number but with same or greater hop count.
                       * Discard that update.
                       */
                      if (!m_advRoutingTable.AnyRunningEvent (dsdv6Header.GetDst ()))
                        {
                          /*update the timer only if nexthop address matches thus discarding
                           * updates to that destination from other nodes.
                           */
                          if (advTableEntry.GetNextHop () == sender)
                            {
                              advTableEntry.SetLifeTime (Simulator::Now ());
                              m_routingTable.Update (advTableEntry);
                            }
                          m_advRoutingTable.DeleteRoute (
                            dsdv6Header.GetDst ());
                        }
                      NS_LOG_DEBUG ("Received update with same seq number and "
                                    "same/worst metric for, " << dsdv6Header.GetDst () << ". Discarding the update.");
                    }
                }
              else
                {
                  // Received update with an old sequence number. Discard the update
                  if (!m_advRoutingTable.AnyRunningEvent (dsdv6Header.GetDst ()))
                    {
                      m_advRoutingTable.DeleteRoute (dsdv6Header.GetDst ());
                    }
                  NS_LOG_DEBUG (dsdv6Header.GetDst () << " : Received update with old seq number. Discarding the update.");
                }
            }
          else
            {
              NS_LOG_DEBUG ("Route with infinite metric received for "
                            << dsdv6Header.GetDst () << " from " << sender);
              // Delete route only if update was received from my nexthop neighbor
              if (sender == advTableEntry.GetNextHop ())
                {
                  NS_LOG_DEBUG ("Triggering an update for this unreachable route:");
                  std::map<Ipv6Address, RoutingTableEntry> dstsWithNextHopSrc;
                  m_routingTable.GetListOfDestinationWithNextHop (dsdv6Header.GetDst (),dstsWithNextHopSrc);
                  m_routingTable.DeleteRoute (dsdv6Header.GetDst ());
                  advTableEntry.SetSeqNo (dsdv6Header.GetDstSeqno ());
                  advTableEntry.SetEntriesChanged (true);
                  m_advRoutingTable.Update (advTableEntry);
                  for (std::map<Ipv6Address, RoutingTableEntry>::iterator i = dstsWithNextHopSrc.begin (); i
                       != dstsWithNextHopSrc.end (); ++i)
                    {
                      i->second.SetSeqNo (i->second.GetSeqNo () + 1);
                      i->second.SetEntriesChanged (true);
                      m_advRoutingTable.AddRoute (i->second);
                      m_routingTable.DeleteRoute (i->second.GetDestination ());
                    }
                }
              else
                {
                  if (!m_advRoutingTable.AnyRunningEvent (dsdv6Header.GetDst ()))
                    {
                      m_advRoutingTable.DeleteRoute (dsdv6Header.GetDst ());
                    }
                  NS_LOG_DEBUG (dsdv6Header.GetDst () <<
                                " : Discard this link break update as it was received from a different neighbor "
                                "and I can reach the destination");
                }
            }
        }
    }
  std::map<Ipv6Address, RoutingTableEntry> allRoutes;
  m_advRoutingTable.GetListOfAllRoutes (allRoutes);
  if (EnableRouteAggregation && allRoutes.size () > 0)
    {
      Simulator::Schedule (m_routeAggregationTime,&RoutingProtocol::SendTriggeredUpdate,this);
    }
  else
    {
      Simulator::Schedule (MicroSeconds (m_uniformRandomVariable->GetInteger (0,1000)),&RoutingProtocol::SendTriggeredUpdate,this);
    }
}


void
RoutingProtocol::SendTriggeredUpdate ()
{
  NS_LOG_FUNCTION (m_mainAddress << " is sending a triggered update");
  std::map<Ipv6Address, RoutingTableEntry> allRoutes;
  m_advRoutingTable.GetListOfAllRoutes (allRoutes);
  for (std::map<Ptr<Socket>, Ipv6InterfaceAddress>::const_iterator j = m_socketAddresses.begin (); j
       != m_socketAddresses.end (); ++j)
    {
      Dsdv6Header dsdv6Header;
      Ptr<Socket> socket = j->first;
      Ipv6InterfaceAddress iface = j->second;
      Ptr<Packet> packet = Create<Packet> ();
      for (std::map<Ipv6Address, RoutingTableEntry>::const_iterator i = allRoutes.begin (); i != allRoutes.end (); ++i)
        {
          NS_LOG_LOGIC ("Destination: " << i->second.GetDestination ()
                                        << " SeqNo:" << i->second.GetSeqNo () << " HopCount:"
                                        << i->second.GetHop () + 1);
          RoutingTableEntry temp = i->second;
          if ((i->second.GetEntriesChanged () == true) && (!m_advRoutingTable.AnyRunningEvent (temp.GetDestination ())))
            {
              dsdv6Header.SetDst (i->second.GetDestination ());
              dsdv6Header.SetDstSeqno (i->second.GetSeqNo ());
              dsdv6Header.SetHopCount (i->second.GetHop () + 1);
              temp.SetFlag (VALID);
              temp.SetEntriesChanged (false);
              m_advRoutingTable.DeleteIpv6Event (temp.GetDestination ());
              if (!(temp.GetSeqNo () % 2))
                {
                  m_routingTable.Update (temp);
                }
              packet->AddHeader (dsdv6Header);
              m_advRoutingTable.DeleteRoute (temp.GetDestination ());
              NS_LOG_DEBUG ("Deleted this route from the advertised table");
            }
          else
            {
              EventId event = m_advRoutingTable.GetEventId (temp.GetDestination ());
              NS_ASSERT (event.GetUid () != 0);
              NS_LOG_DEBUG ("EventID " << event.GetUid () << " associated with "
                                       << temp.GetDestination () << " has not expired, waiting in adv table");
            }
        }
      if (packet->GetSize () >= 24)
        {
          RoutingTableEntry temp2;
          m_routingTable.LookupRoute (Ipv6Address ("FF02::1"), temp2);
          dsdv6Header.SetDst (m_ipv6->GetAddress (1, 1).GetAddress ());
          dsdv6Header.SetDstSeqno (temp2.GetSeqNo ());
          dsdv6Header.SetHopCount (temp2.GetHop () + 1);
          NS_LOG_DEBUG ("Adding my update as well to the packet");
          packet->AddHeader (dsdv6Header);
          // Send to all-hosts broadcast if on /32 addr, subnet-directed otherwise
          Ipv6Address destination = Ipv6Address ("FF02::1");

          socket->SendTo (packet, 0, Inet6SocketAddress (destination, DSDV6_PORT));
          NS_LOG_FUNCTION ("Sent Triggered Update from "
                           << dsdv6Header.GetDst ()
                           << " with packet id : " << packet->GetUid () << " and packet Size: " << packet->GetSize ());
        }
      else
        {
          NS_LOG_FUNCTION ("Update not sent as there are no updates to be triggered");
        }
    }
}

void
RoutingProtocol::SendPeriodicUpdate ()
{
  std::map<Ipv6Address, RoutingTableEntry> removedAddresses, allRoutes;
  m_routingTable.Purge (removedAddresses);
  MergeTriggerPeriodicUpdates ();
  m_routingTable.GetListOfAllRoutes (allRoutes);
  if (allRoutes.empty ())
    {
      return;
    }
  NS_LOG_FUNCTION (m_mainAddress << " is sending out its periodic update");
  for (std::map<Ptr<Socket>, Ipv6InterfaceAddress>::const_iterator j = m_socketAddresses.begin (); j
       != m_socketAddresses.end (); ++j)
    {
      Ptr<Socket> socket = j->first;
      Ipv6InterfaceAddress iface = j->second;
      Ptr<Packet> packet = Create<Packet> ();
      for (std::map<Ipv6Address, RoutingTableEntry>::const_iterator i = allRoutes.begin (); i != allRoutes.end (); ++i)
        {
          Dsdv6Header dsdv6Header;
          if (i->second.GetHop () == 0)
            {
              RoutingTableEntry ownEntry;
              dsdv6Header.SetDst (m_ipv6->GetAddress (1,1).GetAddress ());
              dsdv6Header.SetDstSeqno (i->second.GetSeqNo () + 2);
              dsdv6Header.SetHopCount (i->second.GetHop () + 1);
              m_routingTable.LookupRoute (Ipv6Address ("FF02::1"),ownEntry);
              ownEntry.SetSeqNo (dsdv6Header.GetDstSeqno ());
              m_routingTable.Update (ownEntry);
              packet->AddHeader (dsdv6Header);
            }
          else
            {
              dsdv6Header.SetDst (i->second.GetDestination ());
              dsdv6Header.SetDstSeqno ((i->second.GetSeqNo ()));
              dsdv6Header.SetHopCount (i->second.GetHop () + 1);
              packet->AddHeader (dsdv6Header);
            }
          NS_LOG_DEBUG ("Forwarding the update for " << i->first);
          NS_LOG_DEBUG ("Forwarding details are, Destination: " << dsdv6Header.GetDst ()
                                                                << ", SeqNo:" << dsdv6Header.GetDstSeqno ()
                                                                << ", HopCount:" << dsdv6Header.GetHopCount ()
                                                                << ", LifeTime: " << i->second.GetLifeTime ().GetSeconds ());
        }
      for (std::map<Ipv6Address, RoutingTableEntry>::const_iterator rmItr = removedAddresses.begin (); rmItr
           != removedAddresses.end (); ++rmItr)
        {
          Dsdv6Header removedHeader;
          removedHeader.SetDst (rmItr->second.GetDestination ());
          removedHeader.SetDstSeqno (rmItr->second.GetSeqNo () + 1);
          removedHeader.SetHopCount (rmItr->second.GetHop () + 1);
          packet->AddHeader (removedHeader);
          NS_LOG_DEBUG ("Update for removed record is: Destination: " << removedHeader.GetDst ()
                                                                      << " SeqNo:" << removedHeader.GetDstSeqno ()
                                                                      << " HopCount:" << removedHeader.GetHopCount ());
        }
      socket->Send (packet);
      // Send to all-hosts broadcast if on /32 addr, subnet-directed otherwise
      Ipv6Address destination = Ipv6Address ("FF02::1");
      socket->SendTo (packet, 0, Inet6SocketAddress (destination, DSDV6_PORT));
      NS_LOG_FUNCTION ("PeriodicUpdate Packet UID is : " << packet->GetUid ());
    }
  m_periodicUpdateTimer.Schedule (m_periodicUpdateInterval + MicroSeconds (25 * m_uniformRandomVariable->GetInteger (0,1000)));
}

void
RoutingProtocol::SetIpv6 (Ptr<Ipv6> ipv6)
{
  NS_ASSERT (ipv6 != 0);
  NS_ASSERT (m_ipv6 == 0);
  m_ipv6 = ipv6;
  // Create lo route. It is asserted that the only one interface up for now is loopback
  NS_ASSERT (m_ipv6->GetNInterfaces () == 1 && m_ipv6->GetAddress (0, 0).GetAddress () == Ipv6Address ("::1"));
  m_lo = m_ipv6->GetNetDevice (0);
  NS_ASSERT (m_lo != 0);
  // Remember lo route
  RoutingTableEntry rt (
    /*device=*/ m_lo,  /*dst=*/
    Ipv6Address::GetLoopback (), /*seqno=*/
    0,
    /*iface=*/ Ipv6InterfaceAddress (Ipv6Address::GetLoopback (),Ipv6Prefix (64)),
    /*hops=*/ 0,  /*next hop=*/
    Ipv6Address::GetLoopback (),
    /*lifetime=*/ Simulator::GetMaximumSimulationTime ());
  rt.SetFlag (INVALID);
  rt.SetEntriesChanged (false);
  m_routingTable.AddRoute (rt);
  Simulator::ScheduleNow (&RoutingProtocol::Start,this);
}

void
RoutingProtocol::NotifyInterfaceUp (uint32_t i)
{
  NS_LOG_FUNCTION (this << m_ipv6->GetAddress (i, 0).GetAddress ()
                        << " interface is up");
  Ptr<Ipv6L3Protocol> l3 = m_ipv6->GetObject<Ipv6L3Protocol> ();
  Ipv6InterfaceAddress iface = l3->GetAddress (i,0);
  if (iface.GetAddress () == Ipv6Address ("::1"))
    {
      return;
    }
  iface = l3->GetAddress (i,1);
  // Create a socket to listen only on this interface
  Ptr<Socket> socket = Socket::CreateSocket (GetObject<Node> (),UdpSocketFactory::GetTypeId ());
  NS_ASSERT (socket != 0);
  socket->SetRecvCallback (MakeCallback (&RoutingProtocol::RecvDsdv6,this));
  socket->Bind (Inet6SocketAddress (Ipv6Address::GetAny (), DSDV6_PORT));
  socket->BindToNetDevice (l3->GetNetDevice (i));
  socket->SetAllowBroadcast (true);
  socket->SetAttribute ("IpTtl",UintegerValue (1));
  m_socketAddresses.insert (std::make_pair (socket,iface));
  // Add local broadcast record to the routing table
  Ptr<NetDevice> dev = m_ipv6->GetNetDevice (m_ipv6->GetInterfaceForAddress (iface.GetAddress ()));
  RoutingTableEntry rt (/*device=*/ dev, /*dst=*/ Ipv6Address ("FF02::1"), /*seqno=*/ 0,/*iface=*/ iface,/*hops=*/ 0,
                                    /*next hop=*/ Ipv6Address ("FF02::1"), /*lifetime=*/ Simulator::GetMaximumSimulationTime ());
  RoutingTableEntry rt1 (/*device=*/ dev, /*dst=*/ Ipv6Address ("FF02::2"), /*seqno=*/ 0,/*iface=*/ iface,/*hops=*/ 0,
                                     /*next hop=*/ Ipv6Address ("FF02::2"), /*lifetime=*/ Simulator::GetMaximumSimulationTime ());
  m_routingTable.AddRoute (rt);
  m_routingTable.AddRoute (rt1);
  if (m_mainAddress == Ipv6Address ())
    {
      m_mainAddress = iface.GetAddress ();
    }
  NS_ASSERT (m_mainAddress != Ipv6Address ());
}

void
RoutingProtocol::NotifyInterfaceDown (uint32_t i)
{
  Ptr<Ipv6L3Protocol> l3 = m_ipv6->GetObject<Ipv6L3Protocol> ();
  Ptr<NetDevice> dev = l3->GetNetDevice (i);
  Ptr<Socket> socket = FindSocketWithInterfaceAddress (m_ipv6->GetAddress (i,1));
  NS_ASSERT (socket);
  socket->Close ();
  m_socketAddresses.erase (socket);
  if (m_socketAddresses.empty ())
    {
      NS_LOG_LOGIC ("No dsdv6 interfaces");
      m_routingTable.Clear ();
      return;
    }
  m_routingTable.DeleteAllRoutesFromInterface (m_ipv6->GetAddress (i,1));
  m_advRoutingTable.DeleteAllRoutesFromInterface (m_ipv6->GetAddress (i,1));
}

void
RoutingProtocol::NotifyAddAddress (uint32_t i,
                                   Ipv6InterfaceAddress address)
{
  NS_LOG_FUNCTION (this << " interface " << i << " address " << address);
  Ptr<Ipv6L3Protocol> l3 = m_ipv6->GetObject<Ipv6L3Protocol> ();
  if (!l3->IsUp (i))
    {
      return;
    }
  Ipv6InterfaceAddress iface = l3->GetAddress (i,0);
  Ptr<Socket> socket = FindSocketWithInterfaceAddress (iface);
  if (!socket)
    {
      if (iface.GetAddress () == Ipv6Address ("::1"))
        {
          return;
        }
      iface = l3->GetAddress (i,1);
      Ptr<Socket> socket = Socket::CreateSocket (GetObject<Node> (),UdpSocketFactory::GetTypeId ());
      NS_ASSERT (socket != 0);
      socket->SetRecvCallback (MakeCallback (&RoutingProtocol::RecvDsdv6,this));
      // Bind to any IP address so that broadcasts can be received
      socket->Bind (Inet6SocketAddress (Ipv6Address::GetAny (), DSDV6_PORT));
      socket->BindToNetDevice (l3->GetNetDevice (i));
      socket->SetAllowBroadcast (true);
      m_socketAddresses.insert (std::make_pair (socket,iface));
      Ptr<NetDevice> dev = m_ipv6->GetNetDevice (m_ipv6->GetInterfaceForAddress (iface.GetAddress ()));
      RoutingTableEntry rt (/*device=*/ dev, /*dst=*/ Ipv6Address ("FF02::1"),/*seqno=*/ 0, /*iface=*/ iface,/*hops=*/ 0,
                                        /*next hop=*/ Ipv6Address ("FF02::1"), /*lifetime=*/ Simulator::GetMaximumSimulationTime ());
      m_routingTable.AddRoute (rt);
      RoutingTableEntry rt1 (/*device=*/ dev, /*dst=*/ Ipv6Address ("FF02::2"),/*seqno=*/ 0, /*iface=*/ iface,/*hops=*/ 0,
                                         /*next hop=*/ Ipv6Address ("FF02::2"), /*lifetime=*/ Simulator::GetMaximumSimulationTime ());
      m_routingTable.AddRoute (rt1);
    }
}

void
RoutingProtocol::NotifyRemoveAddress (uint32_t i,
                                      Ipv6InterfaceAddress address)
{
  Ptr<Socket> socket = FindSocketWithInterfaceAddress (address);
  if (socket)
    {
      m_socketAddresses.erase (socket);
      Ptr<Ipv6L3Protocol> l3 = m_ipv6->GetObject<Ipv6L3Protocol> ();
      if (l3->GetNAddresses (i))
        {
          Ipv6InterfaceAddress iface = l3->GetAddress (i,1);
          // Create a socket to listen only on this interface
          Ptr<Socket> socket = Socket::CreateSocket (GetObject<Node> (),UdpSocketFactory::GetTypeId ());
          NS_ASSERT (socket != 0);
          socket->SetRecvCallback (MakeCallback (&RoutingProtocol::RecvDsdv6,this));
          // Bind to any IP address so that broadcasts can be received
          socket->Bind (Inet6SocketAddress (Ipv6Address::GetAny (), DSDV6_PORT));
          socket->SetAllowBroadcast (true);
          m_socketAddresses.insert (std::make_pair (socket,iface));
        }
    }
}

Ptr<Socket>
RoutingProtocol::FindSocketWithInterfaceAddress (Ipv6InterfaceAddress addr) const
{
  for (std::map<Ptr<Socket>, Ipv6InterfaceAddress>::const_iterator j = m_socketAddresses.begin (); j
       != m_socketAddresses.end (); ++j)
    {
      Ptr<Socket> socket = j->first;
      Ipv6InterfaceAddress iface = j->second;
      if (iface == addr)
        {
          return socket;
        }
    }
  Ptr<Socket> socket;
  return socket;
}

void
RoutingProtocol::Send (ns3::Ptr<const ns3::NetDevice> rif, Ptr<Ipv6Route> route,
                       Ptr<const Packet> packet,
                       const Ipv6Header & header)
{
  Ptr<Ipv6L3Protocol> l3 = m_ipv6->GetObject<Ipv6L3Protocol> ();
  NS_ASSERT (l3 != 0);
  Ptr<Packet> p = packet->Copy ();
  l3->Send (p,route->GetSource (),header.GetDestinationAddress (),header.GetNextHeader (),route);
}

void
RoutingProtocol::Drop (Ptr<const Packet> packet,
                       const Ipv6Header & header,
                       Socket::SocketErrno err)
{
  NS_LOG_DEBUG (m_mainAddress << " drop packet " << packet->GetUid () << " to "
                              << header.GetDestinationAddress () << " from queue. Error " << err);
}

void
RoutingProtocol::LookForQueuedPackets (ns3::Ptr<const ns3::NetDevice> rif)
{
  NS_LOG_FUNCTION (this);
  Ptr<Ipv6Route> route;
  std::map<Ipv6Address, RoutingTableEntry> allRoutes;
  m_routingTable.GetListOfAllRoutes (allRoutes);
  for (std::map<Ipv6Address, RoutingTableEntry>::const_iterator i = allRoutes.begin (); i != allRoutes.end (); ++i)
    {
      RoutingTableEntry rt;
      rt = i->second;
      if (m_queue.Find (rt.GetDestination ()))
        {
          if (rt.GetHop () == 1)
            {
              route = rt.GetRoute ();
              NS_LOG_LOGIC ("A route exists from " << route->GetSource ()
                                                   << " to neighboring destination "
                                                   << route->GetDestination ());
              NS_ASSERT (route != 0);
            }
          else
            {
              RoutingTableEntry newrt;
              m_routingTable.LookupRoute (rt.GetNextHop (),newrt);
              route = newrt.GetRoute ();
              NS_LOG_LOGIC ("A route exists from " << route->GetSource ()
                                                   << " to destination " << route->GetDestination () << " via "
                                                   << rt.GetNextHop ());
              NS_ASSERT (route != 0);
            }
          SendPacketFromQueue (rif, rt.GetDestination (),route);
        }
    }
}

void
RoutingProtocol::SendPacketFromQueue (ns3::Ptr<const ns3::NetDevice> rif, Ipv6Address dst,
                                      Ptr<Ipv6Route> route)
{
  NS_LOG_DEBUG (m_mainAddress << " is sending a queued packet to destination " << dst);
  QueueEntry queueEntry;
  if (m_queue.Dequeue (dst,queueEntry))
    {
      DeferredRouteOutputTag tag;
      Ptr<Packet> p = ConstCast<Packet> (queueEntry.GetPacket ());
      if (p->RemovePacketTag (tag))
        {
          if (tag.oif != -1 && tag.oif != m_ipv6->GetInterfaceForDevice (route->GetOutputDevice ()))
            {
              NS_LOG_DEBUG ("Output device doesn't match. Dropped.");
              return;
            }
        }
      UnicastForwardCallback ucb = queueEntry.GetUnicastForwardCallback ();
      Ipv6Header header = queueEntry.GetIpv6Header ();
      header.SetSourceAddress (route->GetSource ());
      header.SetHopLimit (header.GetHopLimit () + 1); // compensate extra TTL decrement by fake loopback routing
      ucb (rif, route,p,header);
      if (m_queue.GetSize () != 0 && m_queue.Find (dst))
        {
          Simulator::Schedule (MilliSeconds (m_uniformRandomVariable->GetInteger (0,100)),
                               &RoutingProtocol::SendPacketFromQueue,this,rif,dst,route);
        }
    }
}

Time
RoutingProtocol::GetSettlingTime (Ipv6Address address)
{
  NS_LOG_FUNCTION ("Calculating the settling time for " << address);
  RoutingTableEntry mainrt;
  Time weightedTime;
  m_routingTable.LookupRoute (address,mainrt);
  if (EnableWST)
    {
      if (mainrt.GetSettlingTime () == Seconds (0))
        {
          return Seconds (0);
        }
      else
        {
          NS_LOG_DEBUG ("Route SettlingTime: " << mainrt.GetSettlingTime ().GetSeconds ()
                                               << " and LifeTime:" << mainrt.GetLifeTime ().GetSeconds ());
          weightedTime = Time (m_weightedFactor * mainrt.GetSettlingTime ().GetSeconds () + (1.0 - m_weightedFactor)
                               * mainrt.GetLifeTime ().GetSeconds ());
          NS_LOG_DEBUG ("Calculated weightedTime:" << weightedTime.GetSeconds ());
          return weightedTime;
        }
    }
  return mainrt.GetSettlingTime ();
}
void RoutingProtocol::NotifyAddRoute (Ipv6Address dst, Ipv6Prefix mask, Ipv6Address nextHop, uint32_t interface, Ipv6Address prefixToUse )
{
}
void RoutingProtocol::NotifyRemoveRoute (Ipv6Address dst, Ipv6Prefix mask, Ipv6Address nextHop, uint32_t interface, Ipv6Address prefixToUse)
{
}
void
RoutingProtocol::MergeTriggerPeriodicUpdates ()
{
  NS_LOG_FUNCTION ("Merging advertised table changes with main table before sending out periodic update");
  std::map<Ipv6Address, RoutingTableEntry> allRoutes;
  m_advRoutingTable.GetListOfAllRoutes (allRoutes);
  if (allRoutes.size () > 0)
    {
      for (std::map<Ipv6Address, RoutingTableEntry>::const_iterator i = allRoutes.begin (); i != allRoutes.end (); ++i)
        {
          RoutingTableEntry advEntry = i->second;
          if ((advEntry.GetEntriesChanged () == true) && (!m_advRoutingTable.AnyRunningEvent (advEntry.GetDestination ())))
            {
              if (!(advEntry.GetSeqNo () % 2))
                {
                  advEntry.SetFlag (VALID);
                  advEntry.SetEntriesChanged (false);
                  m_routingTable.Update (advEntry);
                  NS_LOG_DEBUG ("Merged update for " << advEntry.GetDestination () << " with main routing Table");
                }
              m_advRoutingTable.DeleteRoute (advEntry.GetDestination ());
            }
          else
            {
              NS_LOG_DEBUG ("Event currently running. Cannot Merge Routing Tables");
            }
        }
    }
}
}
}
