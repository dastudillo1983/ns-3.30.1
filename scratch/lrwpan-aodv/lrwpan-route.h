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
 *
 */
#ifndef LRWPAN_ROUTE_H
#define LRWPAN_ROUTE_H

#include <list>
#include <map>
#include <ostream>

#include "ns3/simple-ref-count.h"
#include "ns3/mac16-address.h"

namespace ns3 {

class NetDevice;

/**
 * \ingroup MacRouting
 *
 *\brief IPv4 route cache entry (similar to Linux struct rtable)
 *
 * This is a reference counted object.  In the future, we will add other 
 * entries from struct dst_entry, struct rtable, and struct dst_ops as needed.
 */
class LrWpanRoute : public SimpleRefCount<LrWpanRoute>
{
public:
  LrWpanRoute ();

  /**
   * \param dest Destination Mac16Address
   */
  void SetDestination (Mac16Address dest);
  /**
   * \return Destination Mac16Address of the route
   */
  Mac16Address GetDestination (void) const;

  /**
   * \param src Source Mac16Address
   */
  void SetSource (Mac16Address src);
  /**
   * \return Source Mac16Address of the route
   */
  Mac16Address GetSource (void) const;

  /**
   * \param src Source Mac16Address
   */
  void SetPanId(uint16_t src);
  /**
   * \return Source Mac16Address of the route
   */
  uint16_t GetPanId (void) const;

  /**
   * \param gw Gateway (next hop) Mac16Address
   */
  void SetPANCoordinator (Mac16Address gw);
  /**
   * \return Mac16Address of the gateway (next hop)
   */
  Mac16Address GetPANCoordinator (void) const;

  /**
   * Equivalent in Linux to dst_entry.dev
   *
   * \param outputDevice pointer to NetDevice for outgoing packets
   */
  void SetOutputDevice (Ptr<NetDevice> outputDevice);
  /**
   * \return pointer to NetDevice for outgoing packets
   */
  Ptr<NetDevice> GetOutputDevice (void) const;

#ifdef NOTYET
  // rtable.idev
  void SetInputIfIndex (uint32_t iif);
  uint32_t GetInputIfIndex (void) const;
#endif

private:
  uint16_t m_panId;
  Mac16Address m_dest;             //!< Destination address.
  Mac16Address m_source;           //!< Source address.
  Mac16Address m_pancoordinator;          //!< Gateway address.
  Ptr<NetDevice> m_outputDevice;  //!< Output device.
#ifdef NOTYET
  uint32_t m_inputIfIndex;
#endif
};

/**
 * \brief Stream insertion operator.
 *
 * \param os the reference to the output stream
 * \param route the Ipv4 route
 * \returns the reference to the output stream
 */
std::ostream& operator<< (std::ostream& os, LrWpanRoute const& route);

/**
 * \ingroup ipv4Routing
 * 
 * \brief Ipv4 multicast route cache entry (similar to Linux struct mfc_cache)
 */
//class Ipv4MulticastRoute : public SimpleRefCount<Ipv4MulticastRoute>
//{
//public:
//  Ipv4MulticastRoute ();
//
//  /**
//   * \param group Ipv4Address of the multicast group
//   */
//  void SetGroup (const Mac16Address group);
//  /**
//   * \return Ipv4Address of the multicast group
//   */
//  Ipv4Address GetGroup (void) const;
//
//  /**
//   * \param origin Ipv4Address of the origin address
//   */
//  void SetOrigin (const Ipv4Address origin);
//  /**
//   * \return Ipv4Address of the origin address
//   */
//  Ipv4Address GetOrigin (void) const;
//
//  /**
//   * \param iif Parent (input interface) for this route
//   */
//  void SetParent (uint32_t iif);
//  /**
//   * \return Parent (input interface) for this route
//   */
//  uint32_t GetParent (void) const;
//
//  /**
//   * \param oif Outgoing interface index
//   * \param ttl time-to-live for this route
//   */
//  void SetOutputTtl (uint32_t oif, uint32_t ttl);
//
//  /**
//   * \return map of output interface Ids and TTLs for this route
//   */
//  std::map<uint32_t, uint32_t> GetOutputTtlMap () const;
//
//  static const uint32_t MAX_INTERFACES = 16;  //!< Maximum number of multicast interfaces on a router
//  static const uint32_t MAX_TTL = 255;  //!< Maximum time-to-live (TTL)
//
//private:
//  Ipv4Address m_group;      //!< Group
//  Ipv4Address m_origin;     //!< Source of packet
//  uint32_t m_parent;        //!< Source interface
//  std::map<uint32_t, uint32_t> m_ttls; //!< Time to Live container
//};

} // namespace ns3

#endif /* LRWPAN_ROUTE_H */
