/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright 2007 University of Washington
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
 * Author:  Tom Henderson (tomhend@u.washington.edu)
 * Modified by Fabian Astudillo-Salinas <fabian.astudillos@ucuenca.edu.ec>
 */
#include "ns3/sensor-sink.h"

#include "ns3/address.h"
#include "ns3/address-utils.h"
#include "ns3/log.h"
#include "ns3/inet-socket-address.h"
#include "ns3/inet6-socket-address.h"
#include "ns3/node.h"
#include "ns3/socket.h"
#include "ns3/udp-socket.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/packet.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/lr-wpan-net-device.h"
#include "ns3/callback.h"
#include "ns3/uinteger.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("SensorSink");

NS_OBJECT_ENSURE_REGISTERED (SensorSink);

TypeId 
SensorSink::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::SensorSink")
    .SetParent<Application> ()
    .SetGroupName("Applications")
    .AddConstructor<SensorSink> ()
    .AddAttribute ("SAP",
    			   "The number of Service Access Point to use.",
                   UintegerValue (1),
				   MakeUintegerAccessor (&SensorSink::m_sap),
                   MakeUintegerChecker<uint8_t> ())
    .AddTraceSource ("Rx",
                     "A packet has been received",
                     MakeTraceSourceAccessor (&SensorSink::m_rxTrace),
                     "ns3::Packet::AddressTracedCallback")
    .AddTraceSource ("RxWithAddresses", "A packet has been received",
                     MakeTraceSourceAccessor (&SensorSink::m_rxTraceWithAddresses),
                     "ns3::Packet::TwoAddressTracedCallback")
  ;
  return tid;
}

SensorSink::SensorSink ()
{
  NS_LOG_FUNCTION (this);
  m_totalRx = 0;
  m_sap = 1;
}

SensorSink::~SensorSink()
{
  NS_LOG_FUNCTION (this);
}

uint64_t SensorSink::GetTotalRx () const
{
  NS_LOG_FUNCTION (this);
  return m_totalRx;
}

void SensorSink::DoDispose (void)
{
  NS_LOG_FUNCTION (this);

  // chain up
  Application::DoDispose ();
}


// Application Methods
void SensorSink::StartApplication ()    // Called at time specified by Start
{
  NS_LOG_FUNCTION (this);
  // Create the socket if not already

  Ptr<LrWpanNetDevice> device = DynamicCast<LrWpanNetDevice>(GetNode()->GetDevice(0));
  LrWpanNetDevice::ReceiveCallback cb = MakeCallback (&SensorSink::HandleRead, this);
  device->SetReceiveCallback(cb);
}

void SensorSink::StopApplication ()     // Called at time specified by Stop
{
  NS_LOG_FUNCTION (this);
}

bool SensorSink::HandleRead(Ptr<NetDevice> device, Ptr<const Packet> packet,
		uint16_t protocolNumber, const Address &from) {
	NS_LOG_FUNCTION(this);
	if (protocolNumber == m_sap)
	{
		if (packet->GetSize() > 0) { //EOF
			m_totalRx += packet->GetSize();

			NS_LOG_INFO(
					"At time " << Simulator::Now ().GetSeconds () << "s packet sink received " << packet->GetSize () << " bytes from "
					<< " SAP " << protocolNumber << " total Rx " << m_totalRx << " bytes");
			Ptr<LrWpanNetDevice> dev = DynamicCast<LrWpanNetDevice>(device);
			Mac16Address source = dev->GetMac()->GetShortAddress();
			m_rxTrace(packet, from);
			m_rxTraceWithAddresses(packet, from, source);
		}
	}
	return true;
}

} // Namespace ns3
