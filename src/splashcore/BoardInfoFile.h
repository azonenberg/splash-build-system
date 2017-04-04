/***********************************************************************************************************************
*                                                                                                                      *
* SPLASH build system v0.2                                                                                             *
*                                                                                                                      *
* Copyright (c) 2016-2017 Andrew D. Zonenberg                                                                          *
* All rights reserved.                                                                                                 *
*                                                                                                                      *
* Redistribution and use in source and binary forms, with or without modification, are permitted provided that the     *
* following conditions are met:                                                                                        *
*                                                                                                                      *
*    * Redistributions of source code must retain the above copyright notice, this list of conditions, and the         *
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

#ifndef BoardInfoFile_h
#define BoardInfoFile_h

class BoardInfoPin
{
public:
	BoardInfoPin(std::string loc = "", std::string std = "")
	: m_loc(loc)
	, m_iostandard(std)
	, m_slew(SLEW_SLOW)
	, m_drive(12)			//default for 7 series if not specified in the UCF
	{}

	std::string m_loc;
	std::string m_iostandard;

	enum SlewRates
	{
		SLEW_SLOW,		//slew rate limited driver for EMI reduction
		SLEW_FAST		//unlimited driver for high speed
	} m_slew;

	//Drive strength, in mA
	int m_drive;
};

class BoardInfoClock
{
public:
	BoardInfoClock(double mhz = 0, double duty = 0)
	: m_speedMhz(mhz)
	, m_duty(duty)
	{}

	double m_speedMhz;
	double m_duty;
};

/**
	@brief Information about a PCB pinout for an FPGA
 */
class BoardInfoFile
{
public:
	BoardInfoFile(std::string data);
	virtual ~BoardInfoFile();

	std::string GetTriplet()
	{ return m_triplet; }

	BoardInfoPin GetPin(std::string name)
	{ return m_pins[name]; }

	bool HasPin(std::string name)
	{ return m_pins.find(name) != m_pins.end(); }

	BoardInfoClock GetClock(std::string name)
	{ return m_clocks[name]; }

	bool HasClock(std::string name)
	{ return m_clocks.find(name) != m_clocks.end(); }

	int GetSpeed()
	{ return m_speed; }

	std::string GetPackage()
	{ return m_package; }

protected:
	void ProcessDevice(const YAML::Node& node);
	void ProcessIOs(const YAML::Node& node);
	void ProcessClocks(const YAML::Node& node);

	void Print();

	///Architecture triplet
	std::string m_triplet;

	///Device speed grade
	int m_speed;

	///Device package designator
	std::string m_package;

	//Set of named pins
	std::map<std::string, BoardInfoPin> m_pins;

	//Set of named clock signals
	std::map<std::string, BoardInfoClock> m_clocks;
};

#endif

