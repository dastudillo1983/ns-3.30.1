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

#ifndef LRWPAN_ROUTING_HELPER_H
#define LRWPAN_ROUTING_HELPER_H

#include "ns3/ptr.h"
#include "ns3/nstime.h"
#include "ns3/output-stream-wrapper.h"

namespace ns3 {

class LrWpanRoutingProtocol;
class Node;

/**
 * \ingroup LrWpanHelpers
 *
 * \brief a factory to create ns3::LrWpanRoutingProtocol objects
 *
 * For each new routing protocol created as a subclass of 
 * ns3::LrWpanRoutingProtocol, you need to create a subclass of
 * ns3::LrWpanRoutingHelper which can be used by
 * ns3::InternetStackHelper::SetRoutingHelper and 
 * ns3::InternetStackHelper::Install.
 */
class LrWpanRoutingHelper
{
public:
  /*
   * Destroy an instance of an LrWpanRoutingHelper
   */
  virtual ~LrWpanRoutingHelper ();

  /**
   * \brief virtual constructor
   * \returns pointer to clone of this LrWpanRoutingHelper
   * 
   * This method is mainly for internal use by the other helpers;
   * clients are expected to free the dynamic memory allocated by this method
   */
  virtual LrWpanRoutingHelper* Copy (void) const = 0;

  /**
   * \param node the node within which the new routing protocol will run
   * \returns a newly-created routing protocol
   */
  virtual Ptr<LrWpanRoutingProtocol> Create (Ptr<Node> node) const = 0;

  /**
   * \brief prints the routing tables of all nodes at a particular time.
   * \param printTime the time at which the routing table is supposed to be printed.
   * \param stream The output stream object to use 
   * \param unit The time unit to be used in the report
   *
   * This method calls the PrintRoutingTable() method of the 
   * LrWpanRoutingProtocol stored in the LrWpan object, for all nodes at the
   * specified time; the output format is routing protocol-specific.
   */
  static void PrintRoutingTableAllAt (Time printTime, Ptr<OutputStreamWrapper> stream, Time::Unit unit = Time::S);

  /**
   * \brief prints the routing tables of all nodes at regular intervals specified by user.
   * \param printInterval the time interval for which the routing table is supposed to be printed.
   * \param stream The output stream object to use
   * \param unit The time unit to be used in the report
   *
   * This method calls the PrintRoutingTable() method of the 
   * LrWpanRoutingProtocol stored in the LrWpan object, for all nodes at the
   * specified time interval; the output format is routing protocol-specific.
   */
  static void PrintRoutingTableAllEvery (Time printInterval, Ptr<OutputStreamWrapper> stream, Time::Unit unit = Time::S);

  /**
   * \brief prints the routing tables of a node at a particular time.
   * \param printTime the time at which the routing table is supposed to be printed.
   * \param node The node ptr for which we need the routing table to be printed
   * \param stream The output stream object to use
   * \param unit The time unit to be used in the report
   *
   * This method calls the PrintRoutingTable() method of the 
   * LrWpanRoutingProtocol stored in the LrWpan object, for the selected node
   * at the specified time; the output format is routing protocol-specific.
   */
  static void PrintRoutingTableAt (Time printTime, Ptr<Node> node, Ptr<OutputStreamWrapper> stream, Time::Unit unit = Time::S);

  /**
   * \brief prints the routing tables of a node at regular intervals specified by user.
   * \param printInterval the time interval for which the routing table is supposed to be printed.
   * \param node The node ptr for which we need the routing table to be printed
   * \param stream The output stream object to use
   * \param unit The time unit to be used in the report
   *
   * This method calls the PrintRoutingTable() method of the 
   * LrWpanRoutingProtocol stored in the LrWpan object, for the selected node
   * at the specified interval; the output format is routing protocol-specific.
   */
  static void PrintRoutingTableEvery (Time printInterval, Ptr<Node> node, Ptr<OutputStreamWrapper> stream, Time::Unit unit = Time::S);
  
private:
  /**
   * \brief prints the routing tables of a node.
   * \param node The node ptr for which we need the routing table to be printed
   * \param stream The output stream object to use
   * \param unit The time unit to be used in the report
   *
   * This method calls the PrintRoutingTable() method of the
   * LrWpanRoutingProtocol stored in the LrWpan object;
   * the output format is routing protocol-specific.
   */
  static void Print (Ptr<Node> node, Ptr<OutputStreamWrapper> stream, Time::Unit unit = Time::S);

  /**
   * \brief prints the routing tables of a node at regular intervals specified by user.
   * \param printInterval the time interval for which the routing table is supposed to be printed.
   * \param node The node ptr for which we need the routing table to be printed
   * \param stream The output stream object to use
   * \param unit The time unit to be used in the report
   *
   * This method calls the PrintRoutingTable() method of the
   * LrWpanRoutingProtocol stored in the LrWpan object, for the selected node
   * at the specified interval; the output format is routing protocol-specific.
   */
  static void PrintEvery (Time printInterval, Ptr<Node> node, Ptr<OutputStreamWrapper> stream, Time::Unit unit = Time::S);

};


} // namespace ns3


#endif /* LRWPAN_ROUTING_HELPER_H */
