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
 * Based on
 *      NS-2 AODV model developed by the CMU/MONARCH group and optimized and
 *      tuned by Samir Das and Mahesh Marina, University of Cincinnati;
 *
 *      AODV-UU implementation by Erik Nordström of Uppsala University
 *      http://core.it.uu.se/core/index.php/AODV-UU
 *
 * Authors: Elena Buchatskaia <borovkovaes@iitp.ru>
 *          Pavel Boyko <boyko@iitp.ru>
 */

#ifndef LRWPAN_AODVNEIGHBOR_H
#define LRWPAN_AODVNEIGHBOR_H

#include <vector>
#include "ns3/simulator.h"
#include "ns3/timer.h"
#include "ns3/mac16-address.h"
#include "ns3/callback.h"
#include "ns3/arp-cache.h"

namespace ns3 {

class LrWpanMacHeader;

namespace lrwpanaodv {

class AODVLrWpanRoutingProtocol;

/**
 * \ingroup aodv
 * \brief maintain list of active neighbors
 */
class LrWpanNeighbors
{
public:
  /**
   * constructor
   * \param delay the delay time for purging the list of neighbors
   */
  LrWpanNeighbors (Time delay);
  /// Neighbor description
  struct Neighbor
  {
    /// Neighbor Mac16 address
    Mac16Address m_neighborAddress;
    /// Neighbor MAC address
//    Mac48Address m_hardwareAddress;
    /// Neighbor expire time
    Time m_expireTime;
    /// Neighbor close indicator
    bool close;

    /**
     * \brief Neighbor structure constructor
     *
     * \param ip Mac16Address entry
     * \param mac Mac48Address entry
     * \param t Time expire time
     */
    Neighbor (Mac16Address mac16, Time t)
      : m_neighborAddress (mac16),
        m_expireTime (t),
        close (false)
    {
    }
  };
  /**
   * Return expire time for neighbor node with address addr, if exists, else return 0.
   * \param addr the IP address of the neighbor node
   * \returns the expire time for the neighbor node
   */
  Time GetExpireTime (Mac16Address addr);
  /**
   * Check that node with address addr is neighbor
   * \param addr the IP address to check
   * \returns true if the node with IP address is a neighbor
   */
  bool IsNeighbor (Mac16Address addr);
  /**
   * Update expire time for entry with address addr, if it exists, else add new entry
   * \param addr the IP address to check
   * \param expire the expire time for the address
   */
  void Update (Mac16Address addr, Time expire);
  /// Remove all expired entries
  void Purge ();
  /// Schedule m_ntimer.
  void ScheduleTimer ();
  /// Remove all entries
  void Clear ()
  {
    m_nb.clear ();
  }

  /**
   * Get callback to ProcessTxError
   * \returns the callback function
   */
  Callback<void, LrWpanMacHeader const &> GetTxErrorCallback () const
  {
    return m_txErrorCallback;
  }

  /**
   * Set link failure callback
   * \param cb the callback function
   */
  void SetCallback (Callback<void, Mac16Address> cb)
  {
    m_handleLinkFailure = cb;
  }
  /**
   * Get link failure callback
   * \returns the link failure callback
   */
  Callback<void, Mac16Address> GetCallback () const
  {
    return m_handleLinkFailure;
  }

private:
  /// link failure callback
  Callback<void, Mac16Address> m_handleLinkFailure;
  /// TX error callback
  Callback<void, LrWpanMacHeader const &> m_txErrorCallback;
  /// Timer for neighbor's list. Schedule Purge().
  Timer m_ntimer;
  /// vector of entries
  std::vector<Neighbor> m_nb;

  /// Process layer 2 TX error notification
  void ProcessTxError (LrWpanMacHeader const &);
};

}  // namespace lrwpanaodv
}  // namespace ns3

#endif /* LRWPAN_AODVNEIGHBOR_H */
