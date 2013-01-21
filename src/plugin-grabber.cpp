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

#include <skivvy-grabber/plugin-grabber.h>

#include <ctime>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <iostream>

#include <skivvy/logrep.h>
#include <skivvy/str.h>

namespace skivvy { namespace ircbot {

IRC_BOT_PLUGIN(GrabberIrcBotPlugin);
PLUGIN_INFO("grabber", "Comment Grabber", "0.2");

using namespace skivvy::types;
using namespace skivvy::utils;
using namespace skivvy::string;

const str DATA_FILE = "grabber.data_file";
const str DATA_FILE_DEFAULT = "grabber-data.txt";

struct entry
{
	str stamp;
	str nick;
	str text;
	entry(const quote& q);
	entry(const str& stamp, const str& nick, const str& text);
};

entry::entry(const quote& q)
: nick(q.msg.get_nick())
, text(q.msg.text)
{
	std::ostringstream oss;
	oss << q.stamp;
	stamp = oss.str();
}

entry::entry(const str& stamp, const str& nick, const str& text)
: stamp(stamp)
, nick(nick)
, text(text)
{
}

GrabberIrcBotPlugin::GrabberIrcBotPlugin(IrcBot& bot)
: BasicIrcBotPlugin(bot), max_quotes(100)
{
}

GrabberIrcBotPlugin::~GrabberIrcBotPlugin() {}

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

	// TODO: Remove  && nick != "SooKee" (debugging)
	if(msg.get_nick() == nick && nick != "SooKee")
	{
		bot.fc_reply(msg, "Please don't grab yourself in public " + msg.get_nick() + "!");
		return;
	}

	if(nick == bot.nick)
	{
		bug("grabber: " << msg.get_nick());
		bot.fc_reply(msg, "Sorry " + msg.get_nick() + ", look but don't touch!");
		return;
	}

	size_t n = 1;
	str sub; // substring match text for grab

	if(!(iss >> n))
	{
		iss.clear();
		std::getline(iss, sub);
		trim(sub);
	}

	bug("  n: " << n);
	bug("sub: " << sub);

	if(n > quotes.size())
	{
		std::ostringstream oss;
		oss << "My memory fails me. That was over ";
		oss << quotes.size() << " comments ago. ";
		oss << "Who can remember all that stuff?";
		bot.fc_reply(msg, oss.str());
		return;
	}

	quote_citer q;

	mtx_quotes.lock();
	bool skipped = false;
	for(q = quotes.begin(); n && q != quotes.end(); ++q)
		if(sub.empty())
		{
			if(lowercase(q->msg.get_nick()) == lowercase(nick))
				if(!(--n))
					break;
		}
		else
		{
			if(lowercase(q->msg.get_nick()) == lowercase(nick))
				if(q->msg.text.find(sub) != str::npos && skipped)
					break;
			skipped = true;
		}


	if(q != quotes.end())
	{
		store(entry(*q));
		bot.fc_reply(msg, nick + " has been grabbed: " + q->msg.text.substr(0, 16) + "...");
	}
	mtx_quotes.unlock();
}

void GrabberIrcBotPlugin::store(const entry& e)
{
	bug_func();
	bug("stamp: " << e.stamp);
	bug(" nick: " << e.nick);
	bug(" text: " << e.text);

	const str datafile = bot.getf(DATA_FILE, DATA_FILE_DEFAULT);

	std::ofstream ofs(datafile, std::ios::app);
	if(!ofs)
		log("ERROR: Cannot open grabfile for output: " << datafile);
	mtx_grabfile.lock();
	ofs << e.stamp << ' ' << e.nick << ' ' << e.text << '\n';
	mtx_grabfile.unlock();
}

void GrabberIrcBotPlugin::rq(const message& msg)
{
	str nick = lowercase(msg.get_user_params());
	trim(nick);

	const str datafile = bot.getf(DATA_FILE, DATA_FILE_DEFAULT);

	std::ifstream ifs(datafile);
	if(!ifs) log("ERROR: Cannot open grabfile for input: " << datafile);
	str t, n, q;
	std::vector<entry> full_match_list;
	std::vector<entry> part_match_list;

	mtx_grabfile.lock();
	while(std::getline((ifs >> t >> n), q))
	{
		if(nick.empty() || lowercase(n) == nick)
			full_match_list.push_back(entry(t, n, q));
		if(nick.empty() || lowercase(n).find(nick) != str::npos)
			part_match_list.push_back(entry(t, n, q));
	}
	mtx_grabfile.unlock();

	if(full_match_list.empty())
		full_match_list.assign(part_match_list.begin(), part_match_list.end());

	if(!full_match_list.empty())
	{
		const entry& e = full_match_list[rand_int(0, full_match_list.size() - 1)];
		bot.fc_reply(msg, "<" + e.nick + "> " + e.text);
	}
}

// INTERFACE: BasicIrcBotPlugin

bool GrabberIrcBotPlugin::initialize()
{
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
//	bug_func();
}

// INTERFACE: IrcBotMonitor

void GrabberIrcBotPlugin::event(const message& msg)
{
	mtx_quotes.lock();
	if(msg.cmd == "PRIVMSG") quotes.push_front(msg);
	while(quotes.size() > max_quotes) quotes.pop_back();
	mtx_quotes.unlock();
}

}} // sookee::ircbot
