/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2005 INRIA
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
 * Modified by Fabian Astudillo-Salinas <fabian.astudillos@ucuenca.edu.ec>
 */

#include "llc-header.h"

#include "ns3/assert.h"
#include "ns3/log.h"
#include <string>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("LlcHeader");

NS_OBJECT_ENSURE_REGISTERED (LlcHeader);

LlcHeader::LlcHeader ():
	m_ssap(0), m_dsap(0), m_control(0)
{
  NS_LOG_FUNCTION (this);
}

uint8_t LlcHeader::GetControl() const {
	return m_control;
}

void LlcHeader::SetControl(uint8_t control) {
	m_control = control;
}

uint8_t LlcHeader::GetDsap() const {
	return m_dsap;
}

void LlcHeader::SetDsap(uint8_t dsap) {
	m_dsap = dsap;
}

uint8_t LlcHeader::GetSsap() const {
	return m_ssap;
}

void LlcHeader::SetSsap(uint8_t ssap) {
	m_ssap = ssap;
}

uint32_t 
LlcHeader::GetSerializedSize (void) const
{
  NS_LOG_FUNCTION (this);
  return LLC_HEADER_LENGTH;
}

TypeId 
LlcHeader::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::LlcHeader")
    .SetParent<Header> ()
    .SetGroupName("Network")
    .AddConstructor<LlcHeader> ()
  ;
  return tid;
}
TypeId 
LlcHeader::GetInstanceTypeId (void) const
{
  return GetTypeId ();
}
void 
LlcHeader::Print (std::ostream &os) const
{
  NS_LOG_FUNCTION (this << &os);
  os << "SSAP 0x";
  os.setf (std::ios::hex, std::ios::basefield);
  os << m_ssap;
  os << "; DSAP 0x";
  os << m_dsap;
  os << "; control 0x";
  os << m_control;
}

void
LlcHeader::Serialize (Buffer::Iterator start) const
{
  NS_LOG_FUNCTION (this << &start);
  Buffer::Iterator i = start;
  uint8_t buf[] = { m_ssap, m_dsap, m_control};
  i.Write (buf, 3);
}
uint32_t
LlcHeader::Deserialize (Buffer::Iterator start)
{
  NS_LOG_FUNCTION (this << &start);
  Buffer::Iterator i = start;
  m_ssap = i.ReadU8();
  m_dsap = i.ReadU8();
  m_control = i.ReadU8();
  //i.Next (5+1);
  return GetSerializedSize ();
}


} // namespace ns3
