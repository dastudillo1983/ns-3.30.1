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

#ifndef LLC_HEADER_H
#define LLC_HEADER_H

#include <stdint.h>
#include <string>
#include "ns3/header.h"

namespace ns3 {

/** 
 * The length in octects of the LLC header
 * Type 1 is an unacknowledged connectionless mode for a datagram service. It allows for sending frames
 * to a single destination (point-to-point or unicast transfer), to multiple destinations on the same
 * network (multicast), or to all stations of the network (broadcast).
 *
 * Control Field: Unnumbered format PDUs, or U-format PDUs, with an 8-bit control field,
 *   which are intended for connectionless applications, this is Type 1.
 */
static const uint16_t LLC_HEADER_LENGTH = 3;

/**
 * \ingroup network
 *
 * \brief Header for the LLC/SNAP encapsulation
 *
 * For a list of EtherTypes, see http://www.iana.org/assignments/ieee-802-numbers/ieee-802-numbers.xhtml
 */
class LlcHeader : public Header
{
public:
  LlcHeader ();

  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);



  virtual TypeId GetInstanceTypeId (void) const;
  virtual void Print (std::ostream &os) const;
  virtual uint32_t GetSerializedSize (void) const;
  virtual void Serialize (Buffer::Iterator start) const;
  virtual uint32_t Deserialize (Buffer::Iterator start);

  uint8_t GetControl() const;
  void SetControl(uint8_t control);
  uint8_t GetDsap() const;
  void SetDsap(uint8_t dsap);
  uint8_t GetSsap() const;
  void SetSsap(uint8_t ssap);

private:
  uint8_t m_ssap; // the SSAP
  uint8_t m_dsap; // the DSAP
  uint8_t m_control; // the Control 1 byte for 802.2 LLC Type 1
};

} // namespace ns3

#endif /* LLC_HEADER_H */
