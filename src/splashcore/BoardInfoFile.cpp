/***********************************************************************************************************************
*                                                                                                                      *
* SPLASH build system v0.2                                                                                             *
*                                                                                                                      *
* Copyright (c) 2016-2017 Andrew D. Zonenberg                                                                          *
* All rights reserved.                                                                                                 *
*                                                                                                                      *
* Redistribution and use in Object and binary forms, with or without modification, are permitted provided that the     *
* following conditions are met:                                                                                        *
*                                                                                                                      *
*    * Redistributions of Object code must retain the above copyright notice, this list of conditions, and the         *
*      following disclaimer.                                                                                           *
*                                                                                                                      *
*    * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the       *
*      following disclaimer in the documentation and/or other materials provided with the distribution.                *
*                                                                                                                      *
*    * Neither the name of the author nor the names of any contributors may be used to endorse or promote products     *
*      derived from this software without specific prior written permission.                                           *
*                                                                                                                      *
* THIS SOFTWARE IS PROVIDED BY THE AUTHORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED   *
* TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL *
* THE AUTHORS BE HELD LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES        *
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR       *
* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT *
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE       *
* POSSIBILITY OF SUCH DAMAGE.                                                                                          *
*                                                                                                                      *
***********************************************************************************************************************/

#include "splashcore.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

BoardInfoFile::BoardInfoFile(string data)
{
	vector<YAML::Node> docs = YAML::LoadAll(data);
	for(auto doc : docs)
	{
		if(doc["device"])
			ProcessDevice(doc["device"]);
		else
		{
			LogError("Board info file missing device node\n");
			return;
		}

		if(doc["ios"])
			ProcessIOs(doc["ios"]);
		else
		{
			LogError("Board info file missing ios node\n");
			return;
		}

		if(doc["clocks"])
			ProcessClocks(doc["clocks"]);
		else
		{
			LogError("Board info file missing clocks node\n");
			return;
		}
	}

	Print();
}

BoardInfoFile::~BoardInfoFile()
{
}

void BoardInfoFile::ProcessDevice(const YAML::Node& node)
{
	if(!node["triplet"])
	{
		LogError("Board info file missing triplet entry\n");
		return;
	}

	if(!node["speed"])
	{
		LogError("Board info file missing speed entry\n");
		return;
	}

	if(!node["package"])
	{
		LogError("Board info file missing package entry\n");
		return;
	}

	/*
	device:
		triplet: zynq7-xc7z010
		speed: 1
		package: clg400
	 */
	m_triplet = node["triplet"].as<string>();
	m_package = node["package"].as<string>();
	m_speed = node["speed"].as<int>();
}

void BoardInfoFile::ProcessIOs(const YAML::Node& node)
{
	//Array of I/O pin declarations
	for(auto it : node)
	{
		/*
		clk:
			loc:    L16
			std:    LVCMOS33
			slew:	fast
		*/
		string name = it.first.as<string>();
		YAML::Node pin = it.second;

		if(!pin["loc"])
		{
			LogError("Pin \"%s\" missing loc\n", name.c_str());
			return;
		}

		if(!pin["std"])
		{
			LogError("Pin \"%s\" missing std\n", name.c_str());
			return;
		}

		m_pins[name] = BoardInfoPin(
			pin["loc"].as<string>(),
			pin["std"].as<string>());

		//Slew rate attribute is optional
		if(pin["slew"])
		{
			string s = pin["slew"].as<string>();
			if(s == "fast")
				m_pins[name].m_slew = BoardInfoPin::SLEW_FAST;
			else if(s == "slow")
				m_pins[name].m_slew = BoardInfoPin::SLEW_SLOW;
			else
			{
				LogError("Pin \"%s\" has invalid slew rate\n", name.c_str());
				return;
			}
		}

		//Drive strength attribute is optional
		if(pin["drive"])
			m_pins[name].m_drive = pin["drive"].as<int>();
	}
}

void BoardInfoFile::ProcessClocks(const YAML::Node& node)
{
	//Array of clock pin declarations
	for(auto it : node)
	{
		/*
		clk:
			mhz:    125.000
			duty:   50
		 */

		string name = it.first.as<string>();
		YAML::Node clk = it.second;

		if(!clk["mhz"])
		{
			LogError("Clock \"%s\" missing mhz\n", name.c_str());
			return;
		}

		if(!clk["duty"])
		{
			LogError("duty \"%s\" missing duty\n", name.c_str());
			return;
		}

		m_clocks[name] = BoardInfoClock(
			clk["mhz"].as<double>(),
			clk["duty"].as<double>());
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Debug output

void BoardInfoFile::Print()
{

}
