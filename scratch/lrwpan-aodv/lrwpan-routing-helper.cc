/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2008 INRIA
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
 * Author: Mathieu Lacage <mathieu.lacage@sophia.inria.fr>
 */

#include "lrwpan-routing-helper.h"
#include "ns3/node.h"
#include "ns3/node-list.h"
#include "ns3/simulator.h"
#include "ns3/ipv4-routing-protocol.h"
#include "ns3/ipv4-list-routing.h"
#include "ns3/ipv4-l3-protocol.h"
#include "ns3/ipv4-interface.h"
#include "ns3/arp-cache.h"
#include "ns3/names.h"

namespace ns3 {

LrWpanRoutingHelper::~LrWpanRoutingHelper ()
{
}

void
LrWpanRoutingHelper::PrintRoutingTableAllAt (Time printTime, Ptr<OutputStreamWrapper> stream, Time::Unit unit)
{
  for (uint32_t i = 0; i < NodeList::GetNNodes (); i++)
    {
      Ptr<Node> node = NodeList::GetNode (i);
      Simulator::Schedule (printTime, &LrWpanRoutingHelper::Print, node, stream, unit);
    }
}

void
LrWpanRoutingHelper::PrintRoutingTableAllEvery (Time printInterval, Ptr<OutputStreamWrapper> stream, Time::Unit unit)
{
  for (uint32_t i = 0; i < NodeList::GetNNodes (); i++)
    {
      Ptr<Node> node = NodeList::GetNode (i);
      Simulator::Schedule (printInterval, &LrWpanRoutingHelper::PrintEvery, printInterval, node, stream, unit);
    }
}

void
LrWpanRoutingHelper::PrintRoutingTableAt (Time printTime, Ptr<Node> node, Ptr<OutputStreamWrapper> stream, Time::Unit unit)
{
  Simulator::Schedule (printTime, &LrWpanRoutingHelper::Print, node, stream, unit);
}

void
LrWpanRoutingHelper::PrintRoutingTableEvery (Time printInterval, Ptr<Node> node, Ptr<OutputStreamWrapper> stream, Time::Unit unit)
{
  Simulator::Schedule (printInterval, &LrWpanRoutingHelper::PrintEvery, printInterval, node, stream, unit);
}

void
LrWpanRoutingHelper::Print (Ptr<Node> node, Ptr<OutputStreamWrapper> stream, Time::Unit unit)
{
  Ptr<Ipv4> ipv4 = node->GetObject<Ipv4> ();
  if (ipv4)
    {
      Ptr<Ipv4RoutingProtocol> rp = ipv4->GetRoutingProtocol ();
      NS_ASSERT (rp);
      rp->PrintRoutingTable (stream, unit);
    }
}

void
LrWpanRoutingHelper::PrintEvery (Time printInterval, Ptr<Node> node, Ptr<OutputStreamWrapper> stream, Time::Unit unit)
{
  Ptr<Ipv4> ipv4 = node->GetObject<Ipv4> ();
  if (ipv4)
    {
      Ptr<Ipv4RoutingProtocol> rp = ipv4->GetRoutingProtocol ();
      NS_ASSERT (rp);
      rp->PrintRoutingTable (stream, unit);
      Simulator::Schedule (printInterval, &LrWpanRoutingHelper::PrintEvery, printInterval, node, stream, unit);
    }
}


} // namespace ns3
