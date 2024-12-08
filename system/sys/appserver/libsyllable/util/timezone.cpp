#include <util/timezone.h>

#include <iostream>
#include <unistd.h>

using namespace os;


//taken from php libtime
//converts an int
#define conv_int(l) ((l & 0x000000ff) << 24) + ((l & 0x0000ff00) << 8) + ((l & 0x00ff0000) >> 8) + ((l & 0xff000000) >> 24)


class Timezone::Private
{
public:
	Private()
	{
		tz_is_gmt_count = tz_is_std_count = tz_is_leap_count = tz_is_time_count = tz_is_type_count = tz_is_char_count = 0;
		name = abbreviation = x_coord = y_coord = "";
	}
	
public:
	std::string x_coord;
	std::string y_coord;

	//the timezone name
	std::string name;

	//the timezone abbrev
	std::string abbreviation;
	
	//the transition dates
	std::vector<Timezone::timezone_transition_date>transition_dates;

	//the types	
	std::vector< timezone_gmt_info > types;
	
	//the leap times
	std::vector< timezone_leap_times > leap_times;
	
	//the std count
	std::vector< uint32_t > std_count;
	
	//the gmt count
	std::vector< uint32_t > gmt_count;
	
	uint32_t tz_is_gmt_count;
	uint32_t tz_is_std_count;
	uint32_t tz_is_leap_count;
	uint32_t tz_is_time_count;
	uint32_t tz_is_type_count;
	uint32_t tz_is_char_count;

	unsigned char* translation_index;
};

Timezone::Timezone(const std::string& name)
{
	m = new Private();

	name != "" ? m->name = name : m->name =  GetLocalTimezoneName();
	_ParseTZData();	
}


std::string Timezone::GetZoneFilesPath()
{
	return "./zoneinfo/";
}

std::string Timezone::GetZoneTabPath()
{
	return "./data/zone.tab";
}

std::vector<std::string> Timezone::GetTimezoneNames()
{
	std::vector<std::string> names;

	//open up zone.tab
	//read it and make sure that we don't include comments	
	ifstream in(GetZoneTabPath().c_str());
	
	if (in.is_open())
	{
		do
		{
			std::string line;
			getline(in,line);
	
			if  (line.size() > 1 && line[0] != '#')
			{
				//create a vector of strings containing
				std::vector<std::string> vec = _Explode(line,"\t");

				//create a string with the zone file name and
				//then open that file
				names.push_back(vec[2]);
			}
		}while(!in.eof());
		
		in.close();		
	}
	return names;
}


std::vector<Timezone> Timezone::GetAllTimezones()
{
	std::vector<Timezone> ret;
	std::vector<string> names = GetTimezoneNames();  	
	
	for (uint32_t i = 0; i< names.size(); i++)
	{
		Timezone zone = Timezone(names[i]);
		ret.push_back(zone);	
	}
	return ret;
}

std::string Timezone::GetName()
{
	return m->name;
}

std::string Timezone::GetAbbreviation()
{
	return m->abbreviation;
}


std::vector < Timezone::timezone_gmt_info > Timezone::GetGMTInfo()
{	
	return m->types;
}

std::vector < Timezone::timezone_leap_times> Timezone::GetLeapTimeInfo()
{
	return m->leap_times;
}

std::vector<Timezone::timezone_transition_date> Timezone::GetTransitionDates()
{
	return m->transition_dates;
}

std::string Timezone::GetFileName()
{
	return m->name;
}

void Timezone::SetLocalTimeFromTimezone()
{
	std::string zonepath = GetZoneTabPath().c_str();
	zonepath += GetFileName().c_str();
	symlink(zonepath.c_str(),"/etc/localtime");	
}

/******************************************************************************
**********************************PRIVATE FUNCTONS*****************************
*******************************************************************************/
std::vector<std::string> Timezone::_Explode(std::string s, std::string e)
{
 	std::vector<std::string> ret;
 	
 	//the position of the first instance of e in s
 	int iPos = s.find(e, 0);
 	
 	//the length of e
 	int iPit = e.length();
 
 	//while the pos >-1
 	while(iPos>-1) 
 	{
   		if(iPos!=0) //if pos does not equal 0
   		{
     		ret.push_back(s.substr(0,iPos)); //push string into vector
		}
		else
			ret.push_back("");
			
   		s.erase(0,iPos+iPit); //erase from 0 to pos + length
   		iPos = s.find(e, 0);   //find new pos
 	} // end while
 
 	//if string != ""
	if(s!="")
   		ret.push_back(s); //push last string into vector
 	return ret;
}



void Timezone::_ParseTZData()
{
	//the header of a binary tzfile is 44 bytes long, but
	//since we don't need the first 20 bytes we will declare
	//a buffer, copy over the first 20 then copy over the next
	//24.  This is the only way I have found it to work 
	char buf[24];
	
	
	std::string cFile = GetZoneFilesPath() + m->name;
	ifstream binary_in(cFile.c_str(),ios::binary);
				
								
	//if we can actally open the file
	if (binary_in.is_open())
	{	
		//read in the first 20 bytes that are just garbage to us
		binary_in.read((char*)&buf,20);
		
		
		//if (buf[0] == 'T' && buf[1] == 'Z' && buf[2] == 'i' && buf[3] == 'f')
		//{
			binary_in.read((char*)&buf,24);
		
			//grab the header information
			_ParseBinaryHeaderInfo(buf,24);	
					
			//parse file after header
			_ParseBinaryInfo(binary_in);
		//}		
					
	}
	binary_in.close();
}


void Timezone::_ParseBinaryHeaderInfo(char* data,size_t size)
{
	if (data != "" && size > 0)
	{		
		//this will give you the number of incremtors
		//for our purposes it should be 6
		int incrementor = size/4;
		
		//if we have 6 different 4 byte sections
		if (incrementor == 6)
		{
			//increment through all 6
			for (uint32_t i=0; i<size; i+=4)
			{
				char new_data[4];
				new_data[0] = data[i];
				new_data[1] = data[i+1];
				new_data[2] = data[i+2];
				new_data[3] = data[i+3];
				
				//depending on the incrementation we have to determine where we need to
				//copy the data to.
				switch (i)
				{
					//0-3c
					//the gmt count
					case 0:
					{
						memcpy(&m->tz_is_gmt_count,new_data,sizeof(m->tz_is_gmt_count));
						m->tz_is_gmt_count = conv_int(m->tz_is_gmt_count);						
						//printf("gmt_count: %ld\n",m->tz_is_gmt_count);
						
						break;
					}
					
					//4-7
					//the std count
					case 4:
					{
						memcpy(&m->tz_is_std_count,new_data,sizeof(m->tz_is_std_count));
 						m->tz_is_std_count = conv_int(m->tz_is_std_count);
 						//printf("std count: %ld\n",m->tz_is_std_count);
						break;
					}
					
					//8-11
					//the leap count
					case 8:
					{
						memcpy(&m->tz_is_leap_count,new_data,sizeof(m->tz_is_leap_count));
						m->tz_is_leap_count = conv_int(m->tz_is_leap_count);
						//printf("leap count: %ld\n",m->tz_is_leap_count);
						break;						
					}
					
					//12-15
					//the time count
					case 12:
					{
						memcpy(&m->tz_is_time_count,new_data,sizeof(m->tz_is_time_count));
						m->tz_is_time_count = conv_int(m->tz_is_time_count);
						//printf("is_time_count: %ld\n",m->tz_is_time_count);
						break;
					}
					
					//16-19
					//the type count
					case 16:
					{
						memcpy(&m->tz_is_type_count,new_data,sizeof(m->tz_is_type_count));
						m->tz_is_type_count = conv_int(m->tz_is_type_count);
						//printf("is_type_count: %ld\n",m->tz_is_type_count);
						break;
					}
					
					//20-23
					//the char count
					case 20:
					{
						//m->tz_is_char_count = 0;
						memcpy(&m->tz_is_char_count,new_data,sizeof(m->tz_is_char_count));
						m->tz_is_char_count = conv_int(m->tz_is_char_count);
						///printf("is_char_cout: %ld\n",m->tz_is_char_count);
						break;
					}
				} 
			}
		}
	}	
}

std::string Timezone::_EpochToString(uint32_t time)
{
	char bfr[256];
	struct tm *pstm;
	time_t nTime = time;
	pstm = localtime( &nTime );
	strftime( bfr, sizeof(bfr), "%c", pstm );
	return (string)bfr;	
}
	
void Timezone::_ParseCoord(std::string coord)
{	
	//we know these will always be correct so we can just find the last one and take it from there
	int found=-1;
	
	for (uint32_t i=1; i<coord.size(); i++)
	{
		if (coord[i] == '-' || coord[i] == '+')
		{
			found = i;
			break;
		}
	}
	
	if (found > 0)
	{
		m->x_coord = coord.substr(0,found).c_str();
		m->y_coord = coord.substr(found,coord.size()).c_str();
	}
}

int Timezone::_ParseBinaryInfo(ifstream& in)
{
	if (in.is_open())
	{
		for (uint32_t i=0; i<m->tz_is_time_count; i++)
		{
			//printf("here: %d\n",i);
			timezone_transition_date d;
			
			//the time cnt starts as a char array then you have to convert it
			char c_timecnt[4] = {0,0,0,0};
			int32_t n_timecnt=0;
				
			//read in the timecnt and then copy its contents to the integer
			in.read(c_timecnt,4);
			memcpy(&n_timecnt,c_timecnt,sizeof(c_timecnt));
				
			//convert the time count to a number
			d.date = conv_int(n_timecnt);
			
			if (d.date > 0)
				d.dateAsString = _EpochToString(d.date);
			else
				d.dateAsString = "";
			m->transition_dates.push_back(d);
		}
			
		m->translation_index = (unsigned char*) malloc(sizeof(unsigned char) * m->tz_is_time_count);
		
		if (!m->translation_index)
		{
			return -1;
		}
			
		//read the translation index
		//this is done by taking the size of an unsigned char and multiplying it by the number of time count
		in.read((char*)m->translation_index,sizeof(unsigned char) * m->tz_is_time_count);
		//free(m->translation_index);
		
		for (uint32_t i=0; i<m->tz_is_type_count; i++)
		{
			unsigned char* buffer;
			buffer = (unsigned char*) malloc( sizeof(unsigned char) * 6);
				
				
			if (buffer)
			{
				timezone_gmt_info type_info;
				in.read( (char*)buffer, sizeof(unsigned char)*6);
					
				//when calculating the gmt offset
				//from binary data.  the idea is that you have
				//4 unsigned chars and each is a power of 256
				// 		16777216 	65536		256		1
				//		=================================
				//		256^3		256^2		256^1	1
				type_info.gmt_offset =  (buffer[0] * 16777216) + (buffer[1]*65536) + (buffer[2]*256) + buffer[3];
				type_info.is_dst = buffer[4];
				type_info.abbr_index = buffer[5];
				m->types.push_back(type_info);
			}
			//free(buffer);
		}
		//allocate the abbreviation
		char* abbrev;
		abbrev = (char*)malloc(sizeof(char) * m->tz_is_char_count);
		if (!abbrev)
			return -1;
				
		//read the abbreviation in and then push it into the abbreviation string
		in.read((char*)abbrev,m->tz_is_char_count*sizeof(char));
		m->abbreviation = abbrev;
		free(abbrev);

		
		for (uint32_t i=0; i<m->tz_is_leap_count; i++)
		{
			timezone_leap_times time;
			in.read((char*) &time,sizeof(time));
			time.trans_time = conv_int(time.trans_time);
			time.offset = conv_int(time.offset);
			m->leap_times.push_back(time);
		}
			
		// I am not sure that this is needed*/			
		for (uint32_t i=0; i<m->tz_is_std_count; i++)
		{
			uint32_t b=0;
			in.read((char*) &b,sizeof(b));
			b = conv_int(b);
			m->std_count.push_back(b);
		}
			
			
		for (uint32_t i=0; i<m->tz_is_gmt_count; i++)
		{
			uint32_t b=0;
			in.read((char*) &b,sizeof(b));
			b = conv_int(b);
			m->gmt_count.push_back(b);
		}
	}
	return 0;
}


std::string Timezone::GetLocalTimezoneName()
{
	return tzname[0];
}




























