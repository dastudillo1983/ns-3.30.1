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
 * Authors: Pavel Boyko <boyko@iitp.ru>, written after OlsrHelper by Mathieu Lacage <mathieu.lacage@sophia.inria.fr>
 */
#include "lrwpan-aodv-helper.h"
#include "lrwpan-aodv-routing-protocol.h"
#include "ns3/node-list.h"
#include "ns3/names.h"
#include "ns3/ptr.h"

namespace ns3
{

LrWpanAodvHelper::LrWpanAodvHelper() :
  LrWpanRoutingHelper ()
{
  m_agentFactory.SetTypeId ("ns3::lrwpanaodv::AODVLrWpanRoutingProtocol");
}

LrWpanAodvHelper*
LrWpanAodvHelper::Copy (void) const
{
  return new LrWpanAodvHelper (*this);
}

Ptr<LrWpanRoutingProtocol>
LrWpanAodvHelper::Create (Ptr<Node> node) const
{
  Ptr<lrwpanaodv::AODVLrWpanRoutingProtocol> agent = m_agentFactory.Create<lrwpanaodv::AODVLrWpanRoutingProtocol> ();
  node->AggregateObject (agent);
  agent->SetNode(node);
  return agent;
}

void 
LrWpanAodvHelper::Set (std::string name, const AttributeValue &value)
{
  m_agentFactory.Set (name, value);
}

int64_t
LrWpanAodvHelper::AssignStreams (NodeContainer c, int64_t stream)
{
  int64_t currentStream = stream;
  Ptr<Node> node;
  for (NodeContainer::Iterator i = c.Begin (); i != c.End (); ++i)
    {
      node = (*i);
      Ptr<LrWpanRoutingProtocol> proto = node->GetObject<LrWpanRoutingProtocol> ();
      NS_ASSERT_MSG (proto, "LrWpan routing not installed on node");
      Ptr<lrwpanaodv::AODVLrWpanRoutingProtocol> aodv = DynamicCast<lrwpanaodv::AODVLrWpanRoutingProtocol> (proto);
      if (aodv)
        {
          currentStream += aodv->AssignStreams (currentStream);
          continue;
        }
    }
  return (currentStream - stream);
}

void
LrWpanAodvHelper::Install (NodeContainer c)
{
  for (NodeContainer::Iterator i = c.Begin (); i != c.End (); ++i)
	{
      Ptr<Node> node = *i;
      Create(node);
    }
}

}
