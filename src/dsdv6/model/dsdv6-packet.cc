/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2010 Hemanth Narra
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
#include "dsdv6-packet.h"
#include "ns3/address-utils.h"
#include "ns3/packet.h"

namespace ns3 {
namespace dsdv6 {

NS_OBJECT_ENSURE_REGISTERED (Dsdv6Header);

Dsdv6Header::Dsdv6Header (Ipv6Address dst, uint32_t hopCount, uint32_t dstSeqNo)
  : m_dst (dst),
    m_hopCount (hopCount),
    m_dstSeqNo (dstSeqNo)
{
}

Dsdv6Header::~Dsdv6Header ()
{
}

TypeId
Dsdv6Header::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::dsdv6::Dsdv6Header")
    .SetParent<Header> ()
    .SetGroupName ("Dsdv6")
    .AddConstructor<Dsdv6Header> ();
  return tid;
}

TypeId
Dsdv6Header::GetInstanceTypeId () const
{
  return GetTypeId ();
}

uint32_t
Dsdv6Header::GetSerializedSize () const
{
  return 24;
}

void
Dsdv6Header::Serialize (Buffer::Iterator i) const
{
  WriteTo (i, m_dst);
  i.WriteHtonU32 (m_hopCount);
  i.WriteHtonU32 (m_dstSeqNo);

}

uint32_t
Dsdv6Header::Deserialize (Buffer::Iterator start)
{
  Buffer::Iterator i = start;

  ReadFrom (i, m_dst);
  m_hopCount = i.ReadNtohU32 ();
  m_dstSeqNo = i.ReadNtohU32 ();

  uint32_t dist = i.GetDistanceFrom (start);
  NS_ASSERT (dist == GetSerializedSize ());
  return dist;
}

void
Dsdv6Header::Print (std::ostream &os) const
{
  os << "DestinationIpv6: " << m_dst
     << " Hopcount: " << m_hopCount
     << " SequenceNumber: " << m_dstSeqNo;
}
}
}
