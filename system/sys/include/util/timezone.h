#ifndef __UTIL_TIMEZONE_H__
#define __UTIL_TIMEZONE_H__

#include <string>
#include <vector>
#include <fstream>
#include <algorithm>

using namespace std;


namespace os
{
class Timezone
{
public:
	Timezone(const std::string& zone="");

	static std::vector<std::string> GetTimezoneNames();
	static std::string GetZoneTabPath();
	static std::string GetZoneFilesPath();
	static std::vector<Timezone> GetAllTimezones();
	static std::string GetLocalTimezoneName();	
public:
	//this structure is for types
	struct timezone_gmt_info
	{
		int gmt_offset;
		int is_dst;
		unsigned int abbr_index;
	};
	
	struct timezone_leap_times
	{
		int32_t trans_time;
		int32_t offset;
	};
	
	struct timezone_transition_date
	{
		uint32_t date;
		std::string dateAsString;
	};
	
public:
	std::string GetName();
	std::string GetAbbreviation();
	
	
	std::vector < Timezone::timezone_gmt_info > GetGMTInfo();
	std::vector < Timezone::timezone_leap_times > GetLeapTimeInfo();
	std::vector<Timezone::timezone_transition_date> GetTransitionDates();
	
	
	std::string GetFileName();
	void SetLocalTimeFromTimezone();
	
private:
	static std::vector<std::string> _Explode(std::string s, std::string e);

	std::string _EpochToString(uint32_t time);

	void _ParseTZData();
	void _ParseBinaryHeaderInfo(char* data,size_t size);
	void _ParseCoord(std::string coord);
	int _ParseBinaryInfo(ifstream& in);
	
	 

private:
	class Private;
	Private* m;
};
}


#endif









