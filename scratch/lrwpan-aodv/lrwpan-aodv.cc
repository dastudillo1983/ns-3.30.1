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
 * This is an example script for AODV manet routing protocol. 
 *
 * Authors: Pavel Boyko <boyko@iitp.ru>
 */

#include <iostream>
#include <cmath>
#include <ns3/lr-wpan-module.h>
#include "sensor-application-helper.h"
#include "sensor-sink-helper.h"
#include "lrwpan-aodv-helper.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/point-to-point-module.h"
//#include "ns3/v4ping-helper.h"

using namespace ns3;

/**
 * \ingroup aodv-examples
 * \ingroup examples
 * \brief Test script.
 * 
 * This script creates 1-dimensional grid topology and then ping last node from the first one:
 * 
 * [10.0.0.1] <-- step --> [10.0.0.2] <-- step --> [10.0.0.3] <-- step --> [10.0.0.4]
 * 
 * ping 10.0.0.4
 *
 * When 1/3 of simulation time has elapsed, one of the nodes is moved out of
 * range, thereby breaking the topology.  By default, this will result in
 * only 34 of 100 pings being received.  If the step size is reduced
 * to cover the gap, then all pings can be received.
 */


class LrWpanAodvExample
{
public:
  LrWpanAodvExample ();
  /**
   * \brief Configure script parameters
   * \param argc is the command line argument count
   * \param argv is the command line arguments
   * \return true on successful configuration
  */
  bool Configure (int argc, char **argv);
  /// Run simulation
  void Run ();
  /**
   * Report results
   * \param os the output stream
   */
  void Report (std::ostream & os);

private:

  // parameters
  /// Number of nodes
  uint32_t size;
  /// Distance between nodes, meters
  double step;
  /// Simulation time, seconds
  double totalTime;
  /// Write per-device PCAP traces if true
  bool pcap;
  bool verbose;
  /// Print routes if true
  bool printRoutes;

  // network
  /// nodes used in the example
  NodeContainer nodes;
  /// devices used in the example
  NetDeviceContainer devices;
  /// interfaces used in the example
  Ipv4InterfaceContainer interfaces;

  LrWpanHelper m_lrWpanMac;

  ApplicationContainer appSink;

  ApplicationContainer app;

private:
  /// Create the nodes
  void CreateNodes ();
  /// Create the devices
  void CreateDevices ();
  /// Create the network
  void InstallRouting ();
  /// Create the simulation applications
  void InstallApplications ();

  void PhyRxEnd (std::string context, Ptr<const Packet> paquete, double SINR);

  void PhyTxEnd (std::string context, Ptr<const Packet> paquete);

  void MacTxEnqueue (std::string context, Ptr<const Packet> paquete);

  void MacTxDequeue (std::string context, Ptr<const Packet> paquete);

  void MacSentPkt (std::string context, Ptr<const Packet> paquete, uint8_t retries, uint8_t backoffs);

  void Rx (std::string context, Ptr<const Packet> paquete);

  void Tx (std::string path, Ptr<const Packet> p, const Address &address);

};

int main (int argc, char **argv)
{
  std::cout << "Ingresando ...\n";
  LrWpanAodvExample test;
  if (!test.Configure (argc, argv))
    NS_FATAL_ERROR ("Configuration failed. Aborted.");
  std::cout << "Corriendo ...\n";
  test.Run ();
  std::cout << "Creando reporte...\n";
  test.Report (std::cout);
  return 0;
}

void LrWpanAodvExample::PhyRxEnd (std::string context, Ptr<const Packet> paquete, double SINR)
{
	uint32_t size = paquete->GetSize();
	std::cout << context << " RX " << size << std::endl;
	//NS_LOG_UNCOND(context);
}

void LrWpanAodvExample::PhyTxEnd (std::string context, Ptr<const Packet> paquete)
{
	uint32_t size = paquete->GetSize();
	std::cout << context << " TX " << size << std::endl;
}

void LrWpanAodvExample::MacTxEnqueue (std::string context, Ptr<const Packet> paquete)
{
	uint32_t size = paquete->GetSize();
	std::cout << "MacTxEnqueue " << context << " " << size << " " << std::endl;
}

void LrWpanAodvExample::MacTxDequeue (std::string context, Ptr<const Packet> paquete)
{
	uint32_t size = paquete->GetSize();
	std::cout << "MacTxOk " << context << " " << size << " " << std::endl;
}

void LrWpanAodvExample::Rx (std::string context, Ptr<const Packet> paquete)
{
	uint32_t size = paquete->GetSize();
	std::cout << "Rx " << context << " " << size << " " << std::endl;
}

void LrWpanAodvExample::Tx (std::string path, Ptr<const Packet> p, const Address &address)
{
	uint32_t size = p->GetSize();
	std::cout << "Tx " << path << " " << size << " " << std::endl;
}

// number of retries, total number of csma backoffs
void LrWpanAodvExample::MacSentPkt (std::string context, Ptr<const Packet> paquete, uint8_t retries, uint8_t backoffs)
{
	uint32_t size = paquete->GetSize();
	std::cout << "MacSentPkt " << context << " " << size << " retries: " << (uint32_t)retries << " backoffs " << (uint32_t)backoffs << std::endl;
}

//-----------------------------------------------------------------------------
LrWpanAodvExample::LrWpanAodvExample () :
  size (4), // numero de nodos
  step (5),
  totalTime (100),
  pcap (false),
  verbose(true),
  printRoutes (true)
{
}

bool
LrWpanAodvExample::Configure (int argc, char **argv)
{
  // Enable AODV logs by default. Comment this if too noisy
  // LogComponentEnable("AodvRoutingProtocol", LOG_LEVEL_ALL);

  //SeedManager::SetSeed (12345);
  CommandLine cmd;

  cmd.AddValue ("verbose", "Turn on all log components", verbose);
  cmd.AddValue ("pcap", "Write PCAP traces.", pcap);
  cmd.AddValue ("printRoutes", "Print routing table dumps.", printRoutes);
  cmd.AddValue ("size", "Number of nodes.", size);
  cmd.AddValue ("time", "Simulation time, s.", totalTime);
  cmd.AddValue ("step", "Grid step, m", step);

  cmd.Parse (argc, argv);
  return true;
}

void
LrWpanAodvExample::Run ()
{
//  Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", UintegerValue (1)); // enable rts cts all the time.
  std::cout << "Creando nodos ...\n";
  CreateNodes ();
  std::cout << "Creando dispositivos ...\n";
  CreateDevices ();
  std::cout << "Instalando protocolo de enrutamiento ...\n";
  InstallRouting ();
  std::cout << "Instalando aplicaciones...\n";
  InstallApplications ();

  std::cout << "Starting simulation for " << totalTime << " s ...\n";

  Simulator::Stop (Seconds (totalTime));
  Simulator::Run ();
  //Simulator::Destroy ();
}

void
LrWpanAodvExample::Report (std::ostream &)
{ 
}

void
LrWpanAodvExample::CreateNodes ()
{
  std::cout << "Creating " << (unsigned)size << " nodes " << step << " m apart.\n";
  nodes.Create (size);
  // Name nodes
  for (uint32_t i = 0; i < size; ++i)
    {
      std::ostringstream os;
      os << "node-" << i;
      Names::Add (os.str (), nodes.Get (i));
    }
  // Create static grid
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc =
    CreateObject<ListPositionAllocator>();
  positionAlloc->Add (Vector (0.0, 0.0, 0.0));
  positionAlloc->Add (Vector (0.0, 80, 0.0));
  positionAlloc->Add (Vector (0.0, 160, 0.0));
  positionAlloc->Add (Vector (1.0, 160, 0.0));
  mobility.SetPositionAllocator (positionAlloc);

//  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
//                                 "MinX", DoubleValue (0.0),
//                                 "MinY", DoubleValue (0.0),
//                                 "DeltaX", DoubleValue (step),
//                                 "DeltaY", DoubleValue (0),
//                                 "GridWidth", UintegerValue (size),
//                                 "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);
}

void
LrWpanAodvExample::CreateDevices ()
{
  devices = m_lrWpanMac.Install(nodes);

  m_lrWpanMac.AssociateToPan(devices, 0);

  if (pcap)
    {
	  m_lrWpanMac.EnablePcapAll (std::string ("lrwpanaodv"));
    }
  if (verbose)
  {
	  LogComponentEnableAll (LOG_PREFIX_TIME);
	  LogComponentEnableAll (LOG_PREFIX_FUNC);
//	  LogComponentEnable ("LrWpanCsmaCa", LOG_LEVEL_ALL);
//	  LogComponentEnable ("LrWpanErrorModel", LOG_LEVEL_ALL);
//	  LogComponentEnable ("LrWpanInterferenceHelper", LOG_LEVEL_ALL);
//	  LogComponentEnable ("LrWpanMac", LOG_LEVEL_ALL);
//	  LogComponentEnable ("LrWpanNetDevice", LOG_LEVEL_ALL);
//	  LogComponentEnable ("LrWpanPhy", LOG_LEVEL_ALL);
//	  LogComponentEnable ("LrWpanSpectrumSignalParameters", LOG_LEVEL_ALL);
//	  LogComponentEnable ("LrWpanSpectrumValueHelper", LOG_LEVEL_ALL);
//	  LogComponentEnable ("LrWpanAodvRoutingProtocol", LOG_LEVEL_ALL);
	  LogComponentEnable ("SensorApplication", LOG_LEVEL_ALL);
	  LogComponentEnable ("SensorSink", LOG_LEVEL_ALL);
  }
}

void
LrWpanAodvExample::InstallRouting ()
{
  LrWpanAodvHelper aodv;
  // you can configure AODV attributes here using aodv.Set(name, value)
  aodv.Install(nodes);

  if (printRoutes)
    {
      Ptr<OutputStreamWrapper> routingStream = Create<OutputStreamWrapper> ("aodv.routes", std::ios::out);
      aodv.PrintRoutingTableAllAt (Seconds (8), routingStream);
    }
}

void
LrWpanAodvExample::InstallApplications ()
{
  uint8_t pps = 1; // paquetes por segundo
  uint8_t ps = 20;  // tamano del paquete
  SensorSinkHelper sensorSinkHelper (1);
  appSink = sensorSinkHelper.Install (nodes.Get(0));
  appSink.Start (Seconds (0.10));
//  appSink.Stop (Seconds (10.0));

  DataRate dr = DataRate(pps*ps*8);
  Ptr<LrWpanNetDevice> dev = DynamicCast<LrWpanNetDevice>(nodes.Get(0)->GetDevice(0));
  SensorApplicationHelper sensorAppHelper (1, dev->GetMac()->GetShortAddress());
  sensorAppHelper.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  sensorAppHelper.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]")); // Todo el tiempo esta activa
  sensorAppHelper.SetAttribute ("DataRate",  DataRateValue (dr));
  sensorAppHelper.SetAttribute ("PacketSize", UintegerValue (ps));
  app = sensorAppHelper.Install (nodes.Get(3));
  app.Start(Seconds (0.15));

  //Config::Connect ("/NodeList/*/DeviceList/*/$ns3::LrWpanNetDevice/Phy/PhyRxEnd", MakeCallback (&LrWpanAodvExample::PhyRxEnd, this));
  //Config::Connect ("/NodeList/*/DeviceList/*/$ns3::LrWpanNetDevice/Phy/PhyTxEnd", MakeCallback (&LrWpanAodvExample::PhyTxEnd, this));
  //Config::Connect ("/NodeList/*/DeviceList/*/$ns3::LrWpanNetDevice/Mac/MacTxEnqueue", MakeCallback (&LrWpanAodvExample::MacTxEnqueue, this));
  //Config::Connect ("/NodeList/*/DeviceList/*/$ns3::LrWpanNetDevice/Mac/MacTxDequeue", MakeCallback (&LrWpanAodvExample::MacTxDequeue, this));
  //Config::Connect ("/NodeList/*/DeviceList/*/$ns3::LrWpanNetDevice/Mac/MacSentPkt", MakeCallback (&LrWpanAodvExample::MacSentPkt, this));
  //Config::Connect ("/NodeList/*/DeviceList/*/$ns3::LrWpanNetDevice/Mac/MacTxOk", MakeCallback (&LrWpanAodvExample::MacTxDequeue, this));

  //Config::Connect ("/NodeList/*/ApplicationList/*/$ns3::SensorApplication/Tx", MakeCallback (&LrWpanAodvExample::Tx, this));
  //Config::Connect ("/NodeList/*/ApplicationList/*/$ns3::SensorSink/Rx", MakeCallback (&LrWpanAodvExample::Rx, this));


  //Config::Connect ("/NodeList/*/DeviceList/*/$ns3::lrwpanaodv::AODVLrWpanRoutingProtocol/HelloInterval", MakeCallback (&HelloInterval));


//  for (NodeContainer::Iterator i = nodes.Begin (); i != nodes.End (); i=i+2)
//    {
//      Ptr<Node> node1 = *i;
//      Ptr<Node> node2 = *(i+1);
//      Ptr<LrWpanNetDevice> dev1 = DynamicCast<LrWpanNetDevice>(node1->GetDevice(0));
//      Ptr<LrWpanNetDevice> dev2 = DynamicCast<LrWpanNetDevice>(node2->GetDevice(0));
//      Mac16Address mac1 = dev1->GetMac()->GetShortAddress();
//      Mac16Address mac2 = dev2->GetMac()->GetShortAddress();
//      node1->GetApplication(1)->SetAttribute("Remote", AddressValue(mac2));
//      node2->GetApplication(1)->SetAttribute("Remote", AddressValue(mac1));
//    }

  // move node away
  //Ptr<Node> node = nodes.Get (size/2);
  //Ptr<MobilityModel> mob = node->GetObject<MobilityModel> ();
  //Simulator::Schedule (Seconds (totalTime/3), &MobilityModel::SetPosition, mob, Vector (1e5, 1e5, 1e5));
}

