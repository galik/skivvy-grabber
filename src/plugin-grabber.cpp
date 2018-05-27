/*
 * plugin-grabber.cpp
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

#include <skivvy/plugin-grabber.h>

#include <ctime>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <iostream>

#include <hol/bug.h>
#include <hol/simple_logger.h>
#include <hol/string_utils.h>
#include <hol/small_types.h>
#include <hol/random_utils.h>

#include <skivvy/utils.h>
#include <skivvy/logrep.h>

namespace skivvy { namespace ircbot {

IRC_BOT_PLUGIN(GrabberIrcBotPlugin);
PLUGIN_INFO("grabber", "Comment Grabber", "0.6.0");

using namespace skivvy::utils;
using namespace hol::random_utils;
using namespace hol::simple_logger;
using namespace hol::small_types::basic;

const str DATA_FILE = "grabber.data_file";
const str DATA_FILE_DEFAULT = "grabber-data.txt";

struct entry
{
	str stamp;
	str chan;
	str nick;
	str text;
	entry() {}
	entry(const quote& q);
	entry(const str& stamp, const str& chan, const str& nick, const str& text);
};

entry::entry(const quote& q)
: stamp(std::to_string(q.stamp))
, chan(q.msg.get_chan())
, nick(q.msg.get_nickname())
, text(q.msg.get_trailing())
{
}

entry::entry(const str& stamp, const str& chan, const str& nick, const str& text)
: stamp(stamp)
, chan(chan)
, nick(nick)
, text(text)
{
}

GrabberIrcBotPlugin::GrabberIrcBotPlugin(IrcBot& bot)
: BasicIrcBotPlugin(bot)
, max_quotes(100)
{
}

void GrabberIrcBotPlugin::grab(const message& msg)
{
	BUG_COMMAND(msg);

	str nick;
	std::istringstream iss(msg.get_user_params());
	if(!(iss >> nick))
	{
		bot.fc_reply(msg, "I failed to grasp it... :(");
		return;
	}

	if(msg.get_nickname() == nick)
	{
		bot.fc_reply(msg, "Please don't grab yourself in public " + msg.get_nickname() + "!");
		return;
	}

	if(nick == bot.nick)
	{
		bug("grabber: " << msg.get_nickname());
		bot.fc_reply(msg, "Sorry " + msg.get_nickname() + ", look but don't touch!");
		return;
	}

	siz n = 1;
	str sub; // substring match text for grab

	if(!(iss >> n))
	{
		n = 1;
		iss.clear();
		std::getline(iss, sub);
		hol::trim_mute(sub);
	}

	bug("   n: " << n);
	bug(" sub: " << sub);
	bug("nick: " << nick);

	quote_que& chan_quotes = quotes[msg.get_chan()];

	if(n > chan_quotes.size())
	{
		std::ostringstream oss;
		oss << "My memory fails me. That was over ";
		oss << chan_quotes.size() << " comments ago. ";
		oss << "Who can remember all that stuff?";
		bot.fc_reply(msg, oss.str());
		return;
	}

	quote_citer q;

	lock_guard lock(mtx_quotes);
	bool skipped = false;
	for(q = chan_quotes.begin(); n && q != chan_quotes.end(); ++q)
	{
		if(sub.empty())
		{
			if(hol::lower_copy(q->msg.get_nickname()) == hol::lower_copy(nick))
				if(!(--n))
					break;
		}
		else
		{
			if(hol::lower_copy(q->msg.get_nickname()) == hol::lower_copy(nick))
				if(q->msg.get_trailing().find(sub) != str::npos && skipped)
					break;
			skipped = true;
		}
	}

	if(q != chan_quotes.end())
	{
		store(entry(*q));
		bot.fc_reply(msg, nick + " has been grabbed: " + q->msg.get_trailing().substr(0, 16) + "...");
	}
}

void GrabberIrcBotPlugin::store(const entry& e)
{
	bug_fun();
	bug("stamp: " << e.stamp);
	bug(" chan: " << e.chan);
	bug(" nick: " << e.nick);
	bug(" text: " << e.text);

	const str datafile = bot.getf(DATA_FILE, DATA_FILE_DEFAULT);

	lock_guard lock(mtx_grabfile);
	if(auto ofs = std::ofstream(datafile, std::ios::app))
		ofs << e.stamp << ' ' << e.chan << ' ' << e.nick << ' ' << e.text << '\n';
	else
		LOG::E << "Cannot open grabfile for output: " << datafile;
}

void GrabberIrcBotPlugin::rq(const message& msg)
{
	str nick = hol::lower_copy(msg.get_user_params());
	hol::trim_mute(nick);

	const str datafile = bot.getf(DATA_FILE, DATA_FILE_DEFAULT);

	std::vector<entry> full_match_list;
	std::vector<entry> part_match_list;

	{	std::lock_guard<std::mutex> lock(mtx_grabfile);

		if(auto ifs = std::ifstream(datafile))
		{
			str t, c, n, q;

			sgl(ifs, q); // skip version string
			while(sgl(ifs >> t >> c >> n >> std::ws, q))
			{
				if(c != "*" && c != msg.get_chan())
					continue;
				if(nick.empty() || hol::lower_copy(n) == nick)
					full_match_list.push_back(entry(t, c, n, q));
				if(nick.empty() || hol::lower_copy(n).find(nick) != str::npos)
					part_match_list.push_back(entry(t, c, n, q));
			}
		}
		else
			LOG::E << "Cannot open grabfile for input: " << datafile;
	}

	if(full_match_list.empty())
		full_match_list.assign(part_match_list.begin(), part_match_list.end());

	if(!full_match_list.empty())
	{
		const entry& e = rnd::random_element(full_match_list);
		bot.fc_reply(msg, "<" + e.nick + "> " + e.text);
	}
}

// INTERFACE: BasicIrcBotPlugin

bool GrabberIrcBotPlugin::initialize()
{
	if(!(db = db::BotDb::instance(bot, get_id())))
	{
		LOG::E << "could not open BotDb for: " << get_id();
		return false;
	}

	// TODO: UPGRADE FILE HERE
	LOG::I << "GRABBER: Checking file version:";
	std::ifstream ifs(bot.getf(DATA_FILE, DATA_FILE_DEFAULT));

	str line;
	sgl(ifs, line);
	if(line.find("GRABBER_FILE_VERSION:"))
	{
		LOG::I << "GRABBER: File is unversionned, upgrading to v0.1";
		// unversionned
		str_vec lines;
		str t, c, n, q;
		ifs.seekg(0);
		while(sgl(ifs >> t >> n >> std::ws, q))
			lines.push_back(t + " * " + n + " " + q);
		ifs.close();
		std::ofstream ofs(bot.getf(DATA_FILE, DATA_FILE_DEFAULT));
		ofs << "GRABBER_FILE_VERSION: 0.1" << '\n';
		for(const str& line: lines)
			ofs << line << '\n';
	}

	add
	({
		"!grab"
		, "!grab <nick> [<number>|<text>] Grab what <nick> said"
			" an optional <number> of comments back, OR that contains <text>."
		, [&](const message& msg){ grab(msg); }
	});
	add
	({
		"!rq"
		, "!rq [<nick>] request a previously grabbed quote, optionally from a specific nick."
		, [&](const message& msg){ rq(msg); }
	});
	bot.add_monitor(*this);
	return true;
}

// INTERFACE: IrcBotPlugin

str GrabberIrcBotPlugin::get_id() const { return ID; }
str GrabberIrcBotPlugin::get_name() const { return NAME; }
str GrabberIrcBotPlugin::get_version() const { return VERSION; }

void GrabberIrcBotPlugin::exit()
{
//	bug_fun();
}

// INTERFACE: IrcBotMonitor

void GrabberIrcBotPlugin::event(const message& msg)
{
	if(msg.command == "PRIVMSG" && msg.get_trailing().find("\001ACTION "))
	{
		quote_que& chan_quotes = quotes[msg.get_chan()];

		lock_guard lock(mtx_quotes);
		chan_quotes.push_front(msg);
		while(chan_quotes.size() > max_quotes)
			chan_quotes.pop_back();
	}
}

}} // sookee::ircbot
