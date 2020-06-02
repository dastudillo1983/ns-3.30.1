/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2010 Hemanth Narra
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
 * Author: Hemanth Narra <hemanth@ittc.ku.com>
 *
 * James P.G. Sterbenz <jpgs@ittc.ku.edu>, director
 * ResiliNets Research Group  http://wiki.ittc.ku.edu/resilinets
 * Information and Telecommunication Technology Center (ITTC)
 * and Department of Electrical Engineering and Computer Science
 * The University of Kansas Lawrence, KS USA.
 *
 * Work supported in part by NSF FIND (Future Internet Design) Program
 * under grant CNS-0626918 (Postmodern Internet Architecture),
 * NSF grant CNS-1050226 (Multilayer Network Resilience Analysis and Experimentation on GENI),
 * US Department of Defense (DoD), and ITTC at The University of Kansas.
 */
#include "dsdv6-rtable.h"
#include "ns3/simulator.h"
#include <iomanip>
#include "ns3/log.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("Dsdv6RoutingTable");

namespace dsdv6 {
RoutingTableEntry::RoutingTableEntry (Ptr<NetDevice> dev,
                                      Ipv6Address dst,
                                      uint32_t seqNo,
                                      Ipv6InterfaceAddress iface,
                                      uint32_t hops,
                                      Ipv6Address nextHop,
                                      Time lifetime,
                                      Time SettlingTime,
                                      bool areChanged)
  : m_seqNo (seqNo),
    m_hops (hops),
    m_lifeTime (lifetime),
    m_iface (iface),
    m_flag (VALID),
    m_settlingTime (SettlingTime),
    m_entriesChanged (areChanged)
{
  m_ipv6Route = Create<Ipv6Route> ();
  m_ipv6Route->SetDestination (dst);
  m_ipv6Route->SetGateway (nextHop);
  m_ipv6Route->SetSource (m_iface.GetAddress ());
  m_ipv6Route->SetOutputDevice (dev);
  NS_LOG_DEBUG(this << " dst " << dst << " nextHop/Gateway " << nextHop << " Source " << m_iface.GetAddress () << " Dev " << (dev!=0?dev->GetIfIndex():0xFFFFFFFF));
}
RoutingTableEntry::~RoutingTableEntry ()
{
}
RoutingTable::RoutingTable ()
{
}

bool
RoutingTable::LookupRoute (Ipv6Address id,
                           RoutingTableEntry & rt)
{
  if (m_ipv6AddressEntry.empty ())
    {
      return false;
    }
  std::map<Ipv6Address, RoutingTableEntry>::const_iterator i = m_ipv6AddressEntry.find (id);
  if (i == m_ipv6AddressEntry.end ())
    {
      return false;
    }
  rt = i->second;
  return true;
}

bool
RoutingTable::LookupRoute (Ipv6Address id,
                           RoutingTableEntry & rt,
                           bool forRouteInput)
{
  if (m_ipv6AddressEntry.empty ())
    {
      return false;
    }
  std::map<Ipv6Address, RoutingTableEntry>::const_iterator i = m_ipv6AddressEntry.find (id);
  if (i == m_ipv6AddressEntry.end ())
    {
      return false;
    }
  if (forRouteInput == true && id == Ipv6Address ("FF02::1"))
    {
      return false;
    }
  rt = i->second;
  return true;
}

bool
RoutingTable::DeleteRoute (Ipv6Address dst)
{
  if (m_ipv6AddressEntry.erase (dst) != 0)
    {
      // NS_LOG_DEBUG("Route erased");
      return true;
    }
  return false;
}

uint32_t
RoutingTable::RoutingTableSize ()
{
  return m_ipv6AddressEntry.size ();
}

bool
RoutingTable::AddRoute (RoutingTableEntry & rt)
{
  std::pair<std::map<Ipv6Address, RoutingTableEntry>::iterator, bool> result = m_ipv6AddressEntry.insert (std::make_pair (
                                                                                                            rt.GetDestination (),rt));
  return result.second;
}

bool
RoutingTable::Update (RoutingTableEntry & rt)
{
  std::map<Ipv6Address, RoutingTableEntry>::iterator i = m_ipv6AddressEntry.find (rt.GetDestination ());
  if (i == m_ipv6AddressEntry.end ())
    {
      return false;
    }
  i->second = rt;
  return true;
}

void
RoutingTable::DeleteAllRoutesFromInterface (Ipv6InterfaceAddress iface)
{
  if (m_ipv6AddressEntry.empty ())
    {
      return;
    }
  for (std::map<Ipv6Address, RoutingTableEntry>::iterator i = m_ipv6AddressEntry.begin (); i != m_ipv6AddressEntry.end (); )
    {
      if (i->second.GetInterface () == iface)
        {
          std::map<Ipv6Address, RoutingTableEntry>::iterator tmp = i;
          ++i;
          m_ipv6AddressEntry.erase (tmp);
        }
      else
        {
          ++i;
        }
    }
}

void
RoutingTable::GetListOfAllRoutes (std::map<Ipv6Address, RoutingTableEntry> & allRoutes)
{
  for (std::map<Ipv6Address, RoutingTableEntry>::iterator i = m_ipv6AddressEntry.begin (); i != m_ipv6AddressEntry.end (); ++i)
    {
      if (i->second.GetDestination () != Ipv6Address ("::1") && i->second.GetFlag () == VALID)
        {
          allRoutes.insert (
            std::make_pair (i->first,i->second));
        }
    }
}

void
RoutingTable::GetListOfDestinationWithNextHop (Ipv6Address nextHop,
                                               std::map<Ipv6Address, RoutingTableEntry> & unreachable)
{
  unreachable.clear ();
  for (std::map<Ipv6Address, RoutingTableEntry>::const_iterator i = m_ipv6AddressEntry.begin (); i
       != m_ipv6AddressEntry.end (); ++i)
    {
      if (i->second.GetNextHop () == nextHop)
        {
          unreachable.insert (std::make_pair (i->first,i->second));
        }
    }
}

void
RoutingTableEntry::Print (Ptr<OutputStreamWrapper> stream) const
{
  *stream->GetStream () << std::setiosflags (std::ios::fixed) << m_ipv6Route->GetDestination () << "\t\t" << m_ipv6Route->GetGateway () << "\t\t"
                        << m_iface.GetAddress () << "\t\t" << std::setiosflags (std::ios::left)
                        << std::setw (10) << m_hops << "\t" << std::setw (10) << m_seqNo << "\t"
                        << std::setprecision (3) << (Simulator::Now () - m_lifeTime).GetSeconds ()
                        << "s\t\t" << m_settlingTime.GetSeconds () << "s\n";
}

void
RoutingTable::Purge (std::map<Ipv6Address, RoutingTableEntry> & removedAddresses)
{
  if (m_ipv6AddressEntry.empty ())
    {
      return;
    }
  for (std::map<Ipv6Address, RoutingTableEntry>::iterator i = m_ipv6AddressEntry.begin (); i != m_ipv6AddressEntry.end (); )
    {
      std::map<Ipv6Address, RoutingTableEntry>::iterator itmp = i;
      if (i->second.GetLifeTime () > m_holddownTime && (i->second.GetHop () > 0))
        {
          for (std::map<Ipv6Address, RoutingTableEntry>::iterator j = m_ipv6AddressEntry.begin (); j != m_ipv6AddressEntry.end (); )
            {
              if ((j->second.GetNextHop () == i->second.GetDestination ()) && (i->second.GetHop () != j->second.GetHop ()))
                {
                  std::map<Ipv6Address, RoutingTableEntry>::iterator jtmp = j;
                  removedAddresses.insert (std::make_pair (j->first,j->second));
                  ++j;
                  m_ipv6AddressEntry.erase (jtmp);
                }
              else
                {
                  ++j;
                }
            }
          removedAddresses.insert (std::make_pair (i->first,i->second));
          ++i;
          m_ipv6AddressEntry.erase (itmp);
        }
      /** \todo Need to decide when to invalidate a route */
      /*          else if (i->second.GetLifeTime() > m_holddownTime)
       {
       ++i;
       itmp->second.SetFlag(INVALID);
       }*/
      else
        {
          ++i;
        }
    }
  return;
}

void
RoutingTable::Print (Ptr<OutputStreamWrapper> stream) const
{
  *stream->GetStream () << "\nDSDV6 Routing table\n" << "Destination\t\tGateway\t\tInterface\t\tHopCount\t\tSeqNum\t\tLifeTime\t\tSettlingTime\n";
  for (std::map<Ipv6Address, RoutingTableEntry>::const_iterator i = m_ipv6AddressEntry.begin (); i
       != m_ipv6AddressEntry.end (); ++i)
    {
      i->second.Print (stream);
    }
  *stream->GetStream () << "\n";
}

bool
RoutingTable::AddIpv6Event (Ipv6Address address,
                            EventId id)
{
  std::pair<std::map<Ipv6Address, EventId>::iterator, bool> result = m_ipv6Events.insert (std::make_pair (address,id));
  return result.second;
}

bool
RoutingTable::AnyRunningEvent (Ipv6Address address)
{
  EventId event;
  std::map<Ipv6Address, EventId>::const_iterator i = m_ipv6Events.find (address);
  if (m_ipv6Events.empty ())
    {
      return false;
    }
  if (i == m_ipv6Events.end ())
    {
      return false;
    }
  event = i->second;
  if (event.IsRunning ())
    {
      return true;
    }
  else
    {
      return false;
    }
}

bool
RoutingTable::ForceDeleteIpv6Event (Ipv6Address address)
{
  EventId event;
  std::map<Ipv6Address, EventId>::const_iterator i = m_ipv6Events.find (address);
  if (m_ipv6Events.empty () || i == m_ipv6Events.end ())
    {
      return false;
    }
  event = i->second;
  Simulator::Cancel (event);
  m_ipv6Events.erase (address);
  return true;
}

bool
RoutingTable::DeleteIpv6Event (Ipv6Address address)
{
  EventId event;
  std::map<Ipv6Address, EventId>::const_iterator i = m_ipv6Events.find (address);
  if (m_ipv6Events.empty () || i == m_ipv6Events.end ())
    {
      return false;
    }
  event = i->second;
  if (event.IsRunning ())
    {
      return false;
    }
  if (event.IsExpired ())
    {
      event.Cancel ();
      m_ipv6Events.erase (address);
      return true;
    }
  else
    {
      m_ipv6Events.erase (address);
      return true;
    }
}

EventId
RoutingTable::GetEventId (Ipv6Address address)
{
  std::map <Ipv6Address, EventId>::const_iterator i = m_ipv6Events.find (address);
  if (m_ipv6Events.empty () || i == m_ipv6Events.end ())
    {
      return EventId ();
    }
  else
    {
      return i->second;
    }
}
}
}
