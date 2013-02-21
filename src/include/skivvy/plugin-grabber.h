#pragma once
#ifndef _SOOKEE_IRCBOT_GRABBER_H_
#define _SOOKEE_IRCBOT_GRABBER_H_
/*
 * ircbot-grabber.h
 *
 *  Created on: 29 Jul 2011
 *      Author: oaskivvy@gmail.com
 */

/*-----------------------------------------------------------------.
| Copyright (C) 2011 SooKee oaskivvy@gmail.com               |
'------------------------------------------------------------------'

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
02110-1301, USA.

http://www.gnu.org/licenses/gpl-2.0.html

'-----------------------------------------------------------------*/

#include <skivvy/ircbot.h>

#include <deque>
#include <mutex>

namespace skivvy { namespace ircbot {

struct entry;
struct quote
{
	time_t stamp;
	message msg;
	quote(const message& msg): stamp(time(0)), msg(msg) {}
};

/**
 * PROPERTIES: (Accesed by: bot.props["property"])
 *
 * grabfile - location of grabbed text file "grabfile.txt"
 *
 */
class GrabberIrcBotPlugin
: public BasicIrcBotPlugin
 , public IrcBotMonitor
{
public:
	typedef std::deque<quote> quote_que;
	typedef quote_que::iterator quote_iter;
	typedef quote_que::const_iterator quote_citer;

	typedef std::map<str, quote_que> quote_map;
	typedef quote_map::value_type quote_map_val;

private:

	std::mutex mtx_grabfile; // database
	std::mutex mtx_quotes; // message queue
	quote_map quotes;
	size_t max_quotes; // message queue

	void grab(const message& msg);
	void rq(const message& msg);

	void store(const entry& e);

public:
	GrabberIrcBotPlugin(IrcBot& bot);
	virtual ~GrabberIrcBotPlugin();

	// INTERFACE: BasicIrcBotPlugin

	virtual bool initialize();

	// INTERFACE: IrcBotPlugin

	virtual std::string get_id() const;
	virtual std::string get_name() const;
	virtual std::string get_version() const;
	virtual void exit();

	// INTERFACE: IrcBotMonitor

	virtual void event(const message& msg);
};

}} // sookee::ircbot

#endif // _SOOKEE_IRCBOT_GRABBER_H_
