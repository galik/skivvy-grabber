#ifndef SOOKEE_IRCBOT_GRABBER_H_
#define SOOKEE_IRCBOT_GRABBER_H_
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
#include <skivvy/botdb.h>

#include <deque>
#include <mutex>

#include <hol/small_types.h>

namespace skivvy { namespace ircbot {

using namespace hol::small_types::basic;

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
	using quote_que = std::deque<quote>;
	using quote_iter = quote_que::iterator;
	using quote_citer = quote_que::const_iterator;

	using quote_map = std::map<str, quote_que>;
	using quote_map_val = quote_map::value_type;

	GrabberIrcBotPlugin(IrcBot& bot);

	// INTERFACE: BasicIrcBotPlugin

	virtual bool initialize();

	// INTERFACE: IrcBotPlugin

	str get_id() const override;
	str get_name() const override;
	str get_version() const override;
	void exit() override;

	// INTERFACE: IrcBotMonitor

	void event(const message& msg) override;

private:
	db::BotDb::SPtr db;
	std::mutex mtx_grabfile; // database
	std::mutex mtx_quotes; // message queue
	quote_map quotes;
	siz max_quotes; // message queue

	void grab(const message& msg);
	void rq(const message& msg);

	void store(const entry& e);
};

}} // skivvy::ircbot

#endif // SOOKEE_IRCBOT_GRABBER_H_
