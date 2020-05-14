/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
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

#include "ns3/core-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/rng-seed-manager.h"

#include "ns3/internet-apps-module.h"
#include "ns3/sixlowpan-module.h"
#include "ns3/lr-wpan-module.h"
#include "ns3/node.h"

#include <math.h>
#include <iostream>
#include <fstream>


// Default Network Topology
//
//   LrWPAN Nodes 10.1.3.0
//                 Sink
//  *    *    *    *
//  |    |    |    |    10.1.1.0
// n5   n6   n7   n0 -------------- n1 (server)
//                   point-to-point
//

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("Reto1-RWM");

std::ofstream output;

/**
 * \class StackHelper
 * \brief Helper to set or get some IPv6 information about nodes.
 */
class StackHelper
{
public:

  /**
   * \brief Add an address to a IPv6 node.
   * \param n node
   * \param interface interface index
   * \param address IPv6 address to add
   */
  inline void AddAddress (Ptr<Node>& n, uint32_t interface, Ipv6Address address)
  {
    Ptr<Ipv6> ipv6 = n->GetObject<Ipv6> ();
    ipv6->AddAddress (interface, address);
  }

  /**
   * \brief Print the routing table.
   * \param n the node
   */
  inline void PrintRoutingTable (const Ptr<Node>& n)
  {
    Ptr<Ipv6StaticRouting> routing = 0;
    Ipv6StaticRoutingHelper routingHelper;
    Ptr<Ipv6> ipv6 = n->GetObject<Ipv6> ();
    uint32_t nbRoutes = 0;
    Ipv6RoutingTableEntry route;

    routing = routingHelper.GetStaticRouting (ipv6);

    std::cout << "Routing table of " << n << " : " << std::endl;
    std::cout << "Destination\t\t\t\t" << "Gateway\t\t\t\t\t" << "Interface\t" <<  "Prefix to use" << std::endl;

    nbRoutes = routing->GetNRoutes ();
    for (uint32_t i = 0; i < nbRoutes; i++)
      {
        route = routing->GetRoute (i);
        std::cout << route.GetDest () << "\t"
                  << route.GetGateway () << "\t"
                  << route.GetInterface () << "\t"
                  << route.GetPrefixToUse () << "\t"
                  << std::endl;
      }
  }
};



//static void DevTx (std::string context, Ptr<const Packet> paquete, Ptr<SixLowPanNetDevice> slpdev, uint32_t intindex)
//{
//	uint32_t size = paquete->GetSize();
//	std::cout << context << " TX " << size << std::endl;
//}

static void PhyTxEnd (std::string context, Ptr<const Packet> paquete)
{
	uint32_t size = paquete->GetSize();
	output << context << ",TX," << std::round(Simulator::Now().GetSeconds()) << "," << size << std::endl;
	//std::cout << context << " TX " << size << std::endl;
}

int 
main (int argc, char *argv[])
{

  RngSeedManager :: SetSeed (1);

  bool verbose = true;
  uint32_t nCsma = 3;
  uint32_t nSensorNodes = 3;
  bool tracing = false;
  uint32_t simtime = 10;

  CommandLine cmd;
  // Paramtro numsim
  cmd.AddValue ("nCsma", "Number of \"extra\" CSMA nodes/devices", nCsma);
  cmd.AddValue ("nWifi", "Number of wifi STA devices", nSensorNodes);
  cmd.AddValue ("verbose", "Tell echo applications to log if true", verbose);
  cmd.AddValue ("tracing", "Enable pcap tracing", tracing);
  cmd.AddValue ("time", "Simulation time in seconds (default time=10s)", simtime);

  cmd.Parse (argc,argv);

  output.open ("example-1.csv");


  std::cout << "Simulation time: " << simtime << "s" << std::endl;

  // The underlying restriction of 18 is due to the grid position
  // allocator's configuration; the grid layout will exceed the
  // bounding box if more than 18 nodes are provided.
  if (nSensorNodes > 18)
    {
      std::cout << "nWifi should be 18 or less; otherwise grid layout exceeds the bounding box" << std::endl;
      return 1;
    }

  if (verbose)
    {
      //LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
      //LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);
	  LogComponentEnable ("Ping6Application", LOG_LEVEL_INFO);
	  //LogComponentEnable ("Ipv6L3Protocol", LOG_LEVEL_INFO);
	  //LogComponentEnable ("Ipv6L3Protocol", LOG_LEVEL_LOGIC);
	  //LogComponentEnable ("SixLowPanNetDevice", LOG_LEVEL_INFO);
	  //LogComponentEnable ("SixLowPanNetDevice", LOG_LEVEL_LOGIC);
	  //LogComponentEnable ("SixLowPanNetDevice", LOG_LEVEL_DEBUG);
	  //LogComponentEnable ("LrWpanNetDevice", LOG_LEVEL_INFO);
	  //LogComponentEnable ("LrWpanNetDevice", LOG_LEVEL_DEBUG);
    }

  StackHelper stackHelper;

  NodeContainer p2pNodes;
  p2pNodes.Create (2);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer p2pDevices;
  p2pDevices = pointToPoint.Install (p2pNodes);

  NodeContainer lrWpanNodes;
  lrWpanNodes.Add(p2pNodes.Get (0));
  lrWpanNodes.Create (nSensorNodes);

  MobilityHelper mobility;

  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (5.0),
                                 "DeltaY", DoubleValue (10.0),
                                 "GridWidth", UintegerValue (3),
                                 "LayoutType", StringValue ("RowFirst"));

  mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                             "Bounds", RectangleValue (Rectangle (-10, 10, -10, 10)));
  mobility.Install (lrWpanNodes);

  LrWpanHelper lrWpanHelper;

  NetDeviceContainer lrwpanDevices = lrWpanHelper.Install(lrWpanNodes);

  SixLowPanHelper sixlowpan;
  sixlowpan.SetDeviceAttribute ("ForceEtherType", BooleanValue (true) );
  sixlowpan.SetDeviceAttribute ("EtherType", UintegerValue (0x00F0) );

  InternetStackHelper stack6;
  stack6.Install (lrWpanNodes);
  stack6.Install (p2pNodes.Get(1));

  //NetDeviceContainer lrwpanDevices;
  NetDeviceContainer sixlrwpanDevices = sixlowpan.Install(lrwpanDevices);

  // Fake PAN association and short address assignment.
  lrWpanHelper.AssociateToPan (lrwpanDevices, 0);

  Ipv6AddressHelper address;

  address.SetBase (Ipv6Address ("2001:3::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer p2pInterfaces = address.Assign (p2pDevices);
  p2pInterfaces.SetForwarding (0, true);
  p2pInterfaces.SetDefaultRouteInAllNodes (0);

  address.SetBase (Ipv6Address ("2001:4::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer lrwpanInterfaces = address.Assign (lrwpanDevices);
  lrwpanInterfaces.SetForwarding (0, true);
  lrwpanInterfaces.SetDefaultRouteInAllNodes (0);

  stackHelper.PrintRoutingTable (p2pNodes.Get(0));

  uint32_t packetSize = 10;
  uint32_t maxPacketCount = 5;
  Time interPacketInterval = Seconds (1.);
  Ping6Helper ping6;

  ping6.SetLocal (lrwpanInterfaces.GetAddress (1, 1));
  std::cout << lrwpanInterfaces.GetAddress (1, 1) << std::endl;

  //ping6.SetRemote (lrwpanInterfaces.GetAddress (2, 1));
  //std::cout << lrwpanInterfaces.GetAddress (2, 1) << std::endl;
  ping6.SetRemote (p2pInterfaces.GetAddress (1, 1));
  std::cout << lrwpanInterfaces.GetAddress (1, 1) << std::endl;

  ping6.SetAttribute ("MaxPackets", UintegerValue (maxPacketCount));
  ping6.SetAttribute ("Interval", TimeValue (interPacketInterval));
  ping6.SetAttribute ("PacketSize", UintegerValue (packetSize));
  ApplicationContainer apps = ping6.Install (lrWpanNodes.Get (1));

  std::cout << lrWpanNodes.Get (1)->GetObject<Ipv6>()  << std::endl;

  Config::Connect ("/NodeList/*/DeviceList/*/$ns3::LrWpanNetDevice/Phy/PhyTxBegin", MakeCallback (&PhyTxEnd));
  //Config::Connect ("/NodeList/*/DeviceList/*/$ns3::SixLowPanNetDevice/Tx", MakeCallback (&DevTx));

  apps.Start (Seconds (1.0));
  apps.Stop (Seconds (10.0));

  //Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

//  AsciiTraceHelper ascii;
//  lrWpanHelper.EnableAsciiAll (ascii.CreateFileStream ("Ping-6LoW-lr-wpan.tr"));
//  lrWpanHelper.EnablePcapAll (std::string ("Ping-6LoW-lr-wpan"), true);

  Simulator::Stop (Seconds (simtime));

  Simulator::Run ();
  Simulator::Destroy ();
  output.close();
  return 0;
}
