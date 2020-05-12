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

#include "ns3/ptr.h"
#include "lrwpan-aodv-rtable.h"
#include "ns3/lr-wpan-net-device.h"
#include <algorithm>
#include <iomanip>
#include "ns3/simulator.h"
#include "ns3/log.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("LrWpanAodvRoutingTable");

namespace lrwpanaodv {

/*
 The Routing Table
 */

LrWpanRoutingTableEntry::LrWpanRoutingTableEntry (Ptr<NetDevice> dev, Mac16Address dst, bool vSeqNo, uint32_t seqNo,
                                      uint16_t hops, Mac16Address nextHop, Time lifetime)
  : m_ackTimer (Timer::CANCEL_ON_DESTROY),
    m_validSeqNo (vSeqNo),
    m_seqNo (seqNo),
    m_hops (hops),
    m_lifeTime (lifetime + Simulator::Now ()),
    m_flag (VALID),
    m_reqCount (0),
    m_blackListState (false),
    m_blackListTimeout (Simulator::Now ())
{
  m_lrWpanRoute = Create<LrWpanRoute> ();
  m_lrWpanRoute->SetDestination (dst);
  // TODO: revisar, no creo que el coordinado PAN deba ser el siguiente salto
  m_lrWpanRoute->SetPANCoordinator(nextHop);
  if (dev)
  {
	  Ptr<LrWpanNetDevice> lrWpanDev = DynamicCast<LrWpanNetDevice>(dev);
	  m_lrWpanRoute->SetSource (lrWpanDev->GetMac()->GetShortAddress());
  }
  else
  {
	  m_lrWpanRoute->SetSource (Mac16Address());
  }
  m_lrWpanRoute->SetOutputDevice (dev);
}

LrWpanRoutingTableEntry::~LrWpanRoutingTableEntry ()
{
}

bool
LrWpanRoutingTableEntry::InsertPrecursor (Mac16Address id)
{
  NS_LOG_FUNCTION (this << id);
  if (!LookupPrecursor (id))
    {
      m_precursorList.push_back (id);
      return true;
    }
  else
    {
      return false;
    }
}

bool
LrWpanRoutingTableEntry::LookupPrecursor (Mac16Address id)
{
  NS_LOG_FUNCTION (this << id);
  for (std::vector<Mac16Address>::const_iterator i = m_precursorList.begin (); i
       != m_precursorList.end (); ++i)
    {
      if (*i == id)
        {
          NS_LOG_LOGIC ("Precursor " << id << " found");
          return true;
        }
    }
  NS_LOG_LOGIC ("Precursor " << id << " not found");
  return false;
}

bool
LrWpanRoutingTableEntry::DeletePrecursor (Mac16Address id)
{
  NS_LOG_FUNCTION (this << id);
  std::vector<Mac16Address>::iterator i = std::remove (m_precursorList.begin (),
                                                      m_precursorList.end (), id);
  if (i == m_precursorList.end ())
    {
      NS_LOG_LOGIC ("Precursor " << id << " not found");
      return false;
    }
  else
    {
      NS_LOG_LOGIC ("Precursor " << id << " found");
      m_precursorList.erase (i, m_precursorList.end ());
    }
  return true;
}

void
LrWpanRoutingTableEntry::DeleteAllPrecursors ()
{
  NS_LOG_FUNCTION (this);
  m_precursorList.clear ();
}

bool
LrWpanRoutingTableEntry::IsPrecursorListEmpty () const
{
  return m_precursorList.empty ();
}

void
LrWpanRoutingTableEntry::GetPrecursors (std::vector<Mac16Address> & prec) const
{
  NS_LOG_FUNCTION (this);
  if (IsPrecursorListEmpty ())
    {
      return;
    }
  for (std::vector<Mac16Address>::const_iterator i = m_precursorList.begin (); i
       != m_precursorList.end (); ++i)
    {
      bool result = true;
      for (std::vector<Mac16Address>::const_iterator j = prec.begin (); j
           != prec.end (); ++j)
        {
          if (*j == *i)
            {
              result = false;
            }
        }
      if (result)
        {
          prec.push_back (*i);
        }
    }
}

void
LrWpanRoutingTableEntry::Invalidate (Time badLinkLifetime)
{
  NS_LOG_FUNCTION (this << badLinkLifetime.GetSeconds ());
  if (m_flag == INVALID)
    {
      return;
    }
  m_flag = INVALID;
  m_reqCount = 0;
  m_lifeTime = badLinkLifetime + Simulator::Now ();
}

void
LrWpanRoutingTableEntry::Print (Ptr<OutputStreamWrapper> stream) const
{
  std::ostream* os = stream->GetStream ();
  *os << m_lrWpanRoute->GetDestination () << "\t" << m_lrWpanRoute->GetPANCoordinator() << "\t";
  switch (m_flag)
    {
    case VALID:
      {
        *os << "UP";
        break;
      }
    case INVALID:
      {
        *os << "DOWN";
        break;
      }
    case IN_SEARCH:
      {
        *os << "IN_SEARCH";
        break;
      }
    }
  *os << "\t";
  *os << std::setiosflags (std::ios::fixed) <<
  std::setiosflags (std::ios::left) << std::setprecision (2) <<
  std::setw (14) << (m_lifeTime - Simulator::Now ()).GetSeconds ();
  *os << "\t" << m_hops << "\n";
}

/*
 The Routing Table
 */

RoutingTable::RoutingTable (Time t)
  : m_badLinkLifetime (t)
{
}

bool
RoutingTable::LookupRoute (Mac16Address id, LrWpanRoutingTableEntry & rt)
{
  NS_LOG_FUNCTION (this << id);
  Purge ();
  if (m_ipv4AddressEntry.empty ())
    {
      NS_LOG_LOGIC ("Route to " << id << " not found; m_ipv4AddressEntry is empty");
      return false;
    }
  std::map<Mac16Address, LrWpanRoutingTableEntry>::const_iterator i =
    m_ipv4AddressEntry.find (id);
  if (i == m_ipv4AddressEntry.end ())
    {
      NS_LOG_LOGIC ("Route to " << id << " not found");
      return false;
    }
  rt = i->second;
  NS_LOG_LOGIC ("Route to " << id << " found");
  return true;
}

bool
RoutingTable::LookupValidRoute (Mac16Address id, LrWpanRoutingTableEntry & rt)
{
  NS_LOG_FUNCTION (this << id);
  if (!LookupRoute (id, rt))
    {
      NS_LOG_LOGIC ("Route to " << id << " not found");
      return false;
    }
  NS_LOG_LOGIC ("Route to " << id << " flag is " << ((rt.GetFlag () == VALID) ? "valid" : "not valid"));
  return (rt.GetFlag () == VALID);
}

bool
RoutingTable::DeleteRoute (Mac16Address dst)
{
  NS_LOG_FUNCTION (this << dst);
  Purge ();
  if (m_ipv4AddressEntry.erase (dst) != 0)
    {
      NS_LOG_LOGIC ("Route deletion to " << dst << " successful");
      return true;
    }
  NS_LOG_LOGIC ("Route deletion to " << dst << " not successful");
  return false;
}

bool
RoutingTable::AddRoute (LrWpanRoutingTableEntry & rt)
{
  NS_LOG_FUNCTION (this);
  Purge ();
  if (rt.GetFlag () != IN_SEARCH)
    {
      rt.SetRreqCnt (0);
    }
  std::pair<std::map<Mac16Address, LrWpanRoutingTableEntry>::iterator, bool> result =
    m_ipv4AddressEntry.insert (std::make_pair (rt.GetDestination (), rt));
  return result.second;
}

bool
RoutingTable::Update (LrWpanRoutingTableEntry & rt)
{
  NS_LOG_FUNCTION (this);
  std::map<Mac16Address, LrWpanRoutingTableEntry>::iterator i =
    m_ipv4AddressEntry.find (rt.GetDestination ());
  if (i == m_ipv4AddressEntry.end ())
    {
      NS_LOG_LOGIC ("Route update to " << rt.GetDestination () << " fails; not found");
      return false;
    }
  i->second = rt;
  if (i->second.GetFlag () != IN_SEARCH)
    {
      NS_LOG_LOGIC ("Route update to " << rt.GetDestination () << " set RreqCnt to 0");
      i->second.SetRreqCnt (0);
    }
  return true;
}

bool
RoutingTable::SetEntryState (Mac16Address id, RouteFlags state)
{
  NS_LOG_FUNCTION (this);
  std::map<Mac16Address, LrWpanRoutingTableEntry>::iterator i =
    m_ipv4AddressEntry.find (id);
  if (i == m_ipv4AddressEntry.end ())
    {
      NS_LOG_LOGIC ("Route set entry state to " << id << " fails; not found");
      return false;
    }
  i->second.SetFlag (state);
  i->second.SetRreqCnt (0);
  NS_LOG_LOGIC ("Route set entry state to " << id << ": new state is " << state);
  return true;
}

void
RoutingTable::GetListOfDestinationWithNextHop (Mac16Address nextHop, std::map<Mac16Address, uint32_t> & unreachable )
{
  NS_LOG_FUNCTION (this);
  Purge ();
  unreachable.clear ();
  for (std::map<Mac16Address, LrWpanRoutingTableEntry>::const_iterator i =
         m_ipv4AddressEntry.begin (); i != m_ipv4AddressEntry.end (); ++i)
    {
      if (i->second.GetNextHop () == nextHop)
        {
          NS_LOG_LOGIC ("Unreachable insert " << i->first << " " << i->second.GetSeqNo ());
          unreachable.insert (std::make_pair (i->first, i->second.GetSeqNo ()));
        }
    }
}

void
RoutingTable::InvalidateRoutesWithDst (const std::map<Mac16Address, uint32_t> & unreachable)
{
  NS_LOG_FUNCTION (this);
  Purge ();
  for (std::map<Mac16Address, LrWpanRoutingTableEntry>::iterator i =
         m_ipv4AddressEntry.begin (); i != m_ipv4AddressEntry.end (); ++i)
    {
      for (std::map<Mac16Address, uint32_t>::const_iterator j =
             unreachable.begin (); j != unreachable.end (); ++j)
        {
          if ((i->first == j->first) && (i->second.GetFlag () == VALID))
            {
              NS_LOG_LOGIC ("Invalidate route with destination address " << i->first);
              i->second.Invalidate (m_badLinkLifetime);
            }
        }
    }
}

void
RoutingTable::Purge ()
{
  NS_LOG_FUNCTION (this);
  if (m_ipv4AddressEntry.empty ())
    {
      return;
    }
  for (std::map<Mac16Address, LrWpanRoutingTableEntry>::iterator i =
         m_ipv4AddressEntry.begin (); i != m_ipv4AddressEntry.end (); )
    {
      if (i->second.GetLifeTime () < Seconds (0))
        {
          if (i->second.GetFlag () == INVALID)
            {
              std::map<Mac16Address, LrWpanRoutingTableEntry>::iterator tmp = i;
              ++i;
              m_ipv4AddressEntry.erase (tmp);
            }
          else if (i->second.GetFlag () == VALID)
            {
              NS_LOG_LOGIC ("Invalidate route with destination address " << i->first);
              i->second.Invalidate (m_badLinkLifetime);
              ++i;
            }
          else
            {
              ++i;
            }
        }
      else
        {
          ++i;
        }
    }
}

void
RoutingTable::Purge (std::map<Mac16Address, LrWpanRoutingTableEntry> &table) const
{
  NS_LOG_FUNCTION (this);
  if (table.empty ())
    {
      return;
    }
  for (std::map<Mac16Address, LrWpanRoutingTableEntry>::iterator i =
         table.begin (); i != table.end (); )
    {
      if (i->second.GetLifeTime () < Seconds (0))
        {
          if (i->second.GetFlag () == INVALID)
            {
              std::map<Mac16Address, LrWpanRoutingTableEntry>::iterator tmp = i;
              ++i;
              table.erase (tmp);
            }
          else if (i->second.GetFlag () == VALID)
            {
              NS_LOG_LOGIC ("Invalidate route with destination address " << i->first);
              i->second.Invalidate (m_badLinkLifetime);
              ++i;
            }
          else
            {
              ++i;
            }
        }
      else
        {
          ++i;
        }
    }
}

bool
RoutingTable::MarkLinkAsUnidirectional (Mac16Address neighbor, Time blacklistTimeout)
{
  NS_LOG_FUNCTION (this << neighbor << blacklistTimeout.GetSeconds ());
  std::map<Mac16Address, LrWpanRoutingTableEntry>::iterator i =
    m_ipv4AddressEntry.find (neighbor);
  if (i == m_ipv4AddressEntry.end ())
    {
      NS_LOG_LOGIC ("Mark link unidirectional to  " << neighbor << " fails; not found");
      return false;
    }
  i->second.SetUnidirectional (true);
  i->second.SetBlacklistTimeout (blacklistTimeout);
  i->second.SetRreqCnt (0);
  NS_LOG_LOGIC ("Set link to " << neighbor << " to unidirectional");
  return true;
}

void
RoutingTable::Print (Ptr<OutputStreamWrapper> stream) const
{
  std::map<Mac16Address, LrWpanRoutingTableEntry> table = m_ipv4AddressEntry;
  Purge (table);
  *stream->GetStream () << "\nAODV Routing table\n"
                        << "Destination\tGateway\t\tInterface\tFlag\tExpire\t\tHops\n";
  for (std::map<Mac16Address, LrWpanRoutingTableEntry>::const_iterator i =
         table.begin (); i != table.end (); ++i)
    {
      i->second.Print (stream);
    }
  *stream->GetStream () << "\n";
}

}
}
