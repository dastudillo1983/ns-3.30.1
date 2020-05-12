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
#include "lrwpan-aodv-rqueue.h"
#include <algorithm>
#include <functional>
#include "lrwpan-route.h"
#include "ns3/socket.h"
#include "ns3/log.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("LrWpanAodvRequestQueue");

namespace lrwpanaodv {
uint32_t
LrWpanRequestQueue::GetSize ()
{
  Purge ();
  return m_queue.size ();
}

bool
LrWpanRequestQueue::Enqueue (LrWpanQueueEntry & entry)
{
  Purge ();
  for (std::vector<LrWpanQueueEntry>::const_iterator i = m_queue.begin (); i
       != m_queue.end (); ++i)
    {
      if ((i->GetPacket ()->GetUid () == entry.GetPacket ()->GetUid ())
          && (i->GetLrWpanMacHeader ().GetShortDstAddr ()
              == entry.GetLrWpanMacHeader ().GetShortDstAddr ()))
        {
          return false;
        }
    }
  entry.SetExpireTime (m_queueTimeout);
  if (m_queue.size () == m_maxLen)
    {
      Drop (m_queue.front (), "Drop the most aged packet"); // Drop the most aged packet
      m_queue.erase (m_queue.begin ());
    }
  m_queue.push_back (entry);
  return true;
}

void
LrWpanRequestQueue::DropPacketWithDst (Mac16Address dst)
{
  NS_LOG_FUNCTION (this << dst);
  Purge ();
  for (std::vector<LrWpanQueueEntry>::iterator i = m_queue.begin (); i
       != m_queue.end (); ++i)
    {
      if (i->GetLrWpanMacHeader ().GetShortDstAddr () == dst)
        {
          Drop (*i, "DropPacketWithDst ");
        }
    }
  auto new_end = std::remove_if (m_queue.begin (), m_queue.end (),
                                 [&](const LrWpanQueueEntry& en) { return en.GetLrWpanMacHeader ().GetShortDstAddr () == dst; });
  m_queue.erase (new_end, m_queue.end ());
}

bool
LrWpanRequestQueue::Dequeue (Mac16Address dst, LrWpanQueueEntry & entry)
{
  Purge ();
  for (std::vector<LrWpanQueueEntry>::iterator i = m_queue.begin (); i != m_queue.end (); ++i)
    {
      if (i->GetLrWpanMacHeader ().GetShortDstAddr () == dst)
        {
          entry = *i;
          m_queue.erase (i);
          return true;
        }
    }
  return false;
}

bool
LrWpanRequestQueue::Find (Mac16Address dst)
{
  for (std::vector<LrWpanQueueEntry>::const_iterator i = m_queue.begin (); i
       != m_queue.end (); ++i)
    {
      if (i->GetLrWpanMacHeader ().GetShortDstAddr () == dst)
        {
          return true;
        }
    }
  return false;
}

/**
 * \brief IsExpired structure
 */
struct IsExpired
{
  bool
  /**
   * Check if the entry is expired
   *
   * \param e QueueEntry entry
   * \return true if expired, false otherwise
   */
  operator() (LrWpanQueueEntry const & e) const
  {
    return (e.GetExpireTime () < Seconds (0));
  }
};

void
LrWpanRequestQueue::Purge ()
{
  IsExpired pred;
  for (std::vector<LrWpanQueueEntry>::iterator i = m_queue.begin (); i
       != m_queue.end (); ++i)
    {
      if (pred (*i))
        {
          Drop (*i, "Drop outdated packet ");
        }
    }
  m_queue.erase (std::remove_if (m_queue.begin (), m_queue.end (), pred),
                 m_queue.end ());
}

void
LrWpanRequestQueue::Drop (LrWpanQueueEntry en, std::string reason)
{
  NS_LOG_LOGIC (reason << en.GetPacket ()->GetUid () << " " << en.GetLrWpanMacHeader ().GetShortDstAddr ());
  en.GetErrorCallback () (en.GetPacket (), en.GetLrWpanMacHeader (),
                          Socket::ERROR_NOROUTETOHOST);
  return;
}

}  // namespace aodv
}  // namespace ns3
