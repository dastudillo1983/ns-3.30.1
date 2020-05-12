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

#include <algorithm>
#include "ns3/log.h"
#include "ns3/lr-wpan-mac-header.h"
#include "lrwpan-aodv-neighbor.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("LrWpanAodvNeighbors");

namespace lrwpanaodv {
LrWpanNeighbors::LrWpanNeighbors (Time delay)
  : m_ntimer (Timer::CANCEL_ON_DESTROY)
{
  m_ntimer.SetDelay (delay);
  m_ntimer.SetFunction (&LrWpanNeighbors::Purge, this);
  m_txErrorCallback = MakeCallback (&LrWpanNeighbors::ProcessTxError, this);
}

bool
LrWpanNeighbors::IsNeighbor (Mac16Address addr)
{
  Purge ();
  for (std::vector<Neighbor>::const_iterator i = m_nb.begin ();
       i != m_nb.end (); ++i)
    {
      if (i->m_neighborAddress == addr)
        {
          return true;
        }
    }
  return false;
}

Time
LrWpanNeighbors::GetExpireTime (Mac16Address addr)
{
  Purge ();
  for (std::vector<Neighbor>::const_iterator i = m_nb.begin (); i
       != m_nb.end (); ++i)
    {
      if (i->m_neighborAddress == addr)
        {
          return (i->m_expireTime - Simulator::Now ());
        }
    }
  return Seconds (0);
}

void
LrWpanNeighbors::Update (Mac16Address addr, Time expire)
{
  for (std::vector<Neighbor>::iterator i = m_nb.begin (); i != m_nb.end (); ++i)
    {
      if (i->m_neighborAddress == addr)
        {
          i->m_expireTime
            = std::max (expire + Simulator::Now (), i->m_expireTime);
          return;
        }
    }

  NS_LOG_LOGIC ("Open link to " << addr);
  Neighbor neighbor (addr, expire + Simulator::Now ());
  m_nb.push_back (neighbor);
  Purge ();
}

/**
 * \brief CloseNeighbor structure
 */
struct CloseNeighbor
{
  /**
   * Check if the entry is expired
   *
   * \param nb Neighbors::Neighbor entry
   * \return true if expired, false otherwise
   */
  bool operator() (const LrWpanNeighbors::Neighbor & nb) const
  {
    return ((nb.m_expireTime < Simulator::Now ()) || nb.close);
  }
};

void
LrWpanNeighbors::Purge ()
{
  if (m_nb.empty ())
    {
      return;
    }

  CloseNeighbor pred;
  if (!m_handleLinkFailure.IsNull ())
    {
      for (std::vector<Neighbor>::iterator j = m_nb.begin (); j != m_nb.end (); ++j)
        {
          if (pred (*j))
            {
              NS_LOG_LOGIC ("Close link to " << j->m_neighborAddress);
              m_handleLinkFailure (j->m_neighborAddress);
            }
        }
    }
  m_nb.erase (std::remove_if (m_nb.begin (), m_nb.end (), pred), m_nb.end ());
  m_ntimer.Cancel ();
  m_ntimer.Schedule ();
}

void
LrWpanNeighbors::ScheduleTimer ()
{
  m_ntimer.Cancel ();
  m_ntimer.Schedule ();
}

void
LrWpanNeighbors::ProcessTxError (LrWpanMacHeader const & hdr)
{
  Mac16Address addr = hdr.GetShortDstAddr();

  for (std::vector<Neighbor>::iterator i = m_nb.begin (); i != m_nb.end (); ++i)
    {
      if (i->m_neighborAddress == addr)
        {
          i->close = true;
        }
    }
  Purge ();
}

}  // namespace lrwpanaodv
}  // namespace ns3

