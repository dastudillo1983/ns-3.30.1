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

#include "lrwpan-routing-protocol.h"

#include "lrwpan-route.h"
#include "ns3/assert.h"
#include "ns3/log.h"
#include "ns3/node.h"
#include "ns3/simulator.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("LrWpanRoutingProtocol");

NS_OBJECT_ENSURE_REGISTERED (LrWpanRoutingProtocol);

TypeId LrWpanRoutingProtocol::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::LrWpanRoutingProtocol")
    .SetParent<Object> 	()
    .SetGroupName ("LrWpan")
  ;
  return tid;
}

const Ptr<Node>& LrWpanRoutingProtocol::GetNode() const {
	return m_node;
}

void LrWpanRoutingProtocol::SetNode(const Ptr<Node> &node) {
	m_node = node;
}

} // namespace ns3
