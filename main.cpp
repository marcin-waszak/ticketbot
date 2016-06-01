#include <cstdlib>
#include <cstring>
#include <ctime>
#include <clocale>
#include <vector>
#include <string>
#include <sstream>
#include <fstream>
#include <iostream>
#include <Windows.h>
#include <curl\curl.h>

const int version_major = 0;
const int version_minor = 7;
const char *n_alarmSource = "alarm_info.html";
const char *d_phantomjsPath = "phantomjs";
const char *d_inputFile = "input.txt";
const char *d_scriptFile = "script.js";
const char *d_resultFile = "result.dat";
CURL *curl;

int getURL(const char* filename, std::vector<std::string> &input);
int checkLoop(std::vector<std::string> &input);
static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp);
int parseResult_ticketportal(const char *input);
int parseResult_ticketpro(std::string &input);
char* generate_alarm_name(char *destination);
int save_html(std::string t_source);
void atime(char *destination);
void alert();
std::string valueBetween(std::string &source, std::string a, std::string b, size_t start);

int main(int argc, char **argv)
{
	char inputFile[128];
	std::vector<std::string> urlTable;
	curl = curl_easy_init();

	std::cout << "TicketBot " << version_major << '.' << version_minor
		<< "\n(C) 2015 Marcin Waszak" << std::endl;
	std::cout << "Compilation date: " << __DATE__ << '\t' << __TIME__ << std::endl << std::endl;

	if (argc == 2)
		strcpy(inputFile, argv[1]);
	else
		strcpy(inputFile, d_inputFile);

	std::cout << "Reading URLs from " << inputFile << " ..." << std::endl;
	getURL(inputFile, urlTable);
	checkLoop(urlTable);

	return  0;
}

char* generate_alarm_name(char *destination)
{
	time_t rawtime;
	struct tm *timeinfo;
	time(&rawtime);
	timeinfo = localtime(&rawtime);
	strftime(destination, 64, "alarmsrc_%Y-%m-%d_%H-%M-%S.html", timeinfo);

	return destination;
}

int save_html(std::string t_source)
{
	char s_temp[64];
	std::fstream t_file(generate_alarm_name(s_temp), std::ios::out);
	t_file << t_source;
	t_file.flush();
	t_file.close();

	return 0;
}

int getURL(const char* filename, std::vector<std::string> &input)
{
	int url_counter = 0;
	std::fstream file(filename, std::ios::in);

	if(!file.good())
		return 0;

	while (!file.eof())
	{
		std::string currentLine;
		std::getline(file, currentLine);

		if(currentLine.size())
		{
			input.push_back(currentLine);
			url_counter++;
		}
	}

	file.close();
	return url_counter;
}

int checkLoop(std::vector<std::string> &input)
{
	CURLcode res;
	std::string t_buffer;

	while (1)
	{
		for (auto urlString : input)
		{
			if(urlString.find("ticketportal", 0) != std::string::npos)
			{
				std::string command;
				command += d_phantomjsPath;
				command += ' ';
				command += d_scriptFile;
				command += ' ';
				command += urlString;
				command += " > ";
				command += d_resultFile;

				system(command.c_str());
				parseResult_ticketportal(d_resultFile);
			}
			else if (urlString.find("ticketpro", 0) != std::string::npos)
			{
				t_buffer.clear();
				curl_easy_setopt(curl, CURLOPT_URL, urlString.c_str());
				curl_easy_setopt(curl, CURLOPT_HEADER, 0);
				curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
				curl_easy_setopt(curl, CURLOPT_WRITEDATA, &t_buffer);
				curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0); //bugfix if SSL is enabled 
				res = curl_easy_perform(curl);
				if (res != CURLE_OK)
					fprintf(stderr, "Curl perform failed: %s\n", curl_easy_strerror(res));
				else	
					parseResult_ticketpro(t_buffer);
			}
		}
		Sleep(5000);
	}

	return 0;
}

static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
	(static_cast<std::string*>(userp))->append(static_cast<char*>(contents), size * nmemb);
	return size * nmemb;
}

int parseResult_ticketportal(const char *input)
{
	std::fstream file(input, std::ios::in);
	std::stringstream buffer;
	std::string source;
	char timeBuffer[64];
	size_t found = 0;
	int tix = 0;
	bool upodia =  false;

	atime(timeBuffer);
	std::cout << timeBuffer << " ticketportal.cz\t";

	if(!file.good())
	{
		std::cout << "Error loading " << input << std::endl;
		return 0;
	}

	buffer << file.rdbuf();
	source = buffer.str();

	if (source.size() < 1024)
	{
		std::cout << "Error loading webpage! Check Internet connection!" << std::endl;
		return 0;
	}

	if (source.find("sector_line_headline", 0) == std::string::npos)
	{
		std::cout << "Cannot load event page!" << std::endl;
		return 0;
	}

	while((found = source.find("\"sector_line\"", found + 1)) != std::string::npos)
	{
		if(valueBetween(source, "sektor_span1_", "\"", found) == "405001076")
			upodia = true;
		tix++;
	}

	if (tix == 2 || upodia)
	{
		save_html(source);
		std::cout << "Found STANI U PODIA !!!!!!" << std::endl;
		alert();
	}

	std::cout << "Found " << tix << " tickets!" << std::endl;
	return tix;
}

int parseResult_ticketpro(std::string &input)
{
	char timeBuffer[64];
	atime(timeBuffer);

	for (auto &c : input) // string to lowercase
		c = tolower(c);

	std::cout << timeBuffer << " ticketpro.cz\t\t";

	if (input.find("ticketpro - detaily akce - acdc (st") == std::string::npos ||
		input.find("prlev_header") == std::string::npos)
	{
		std::cout << "Cannot load event page!" << std::endl;
		return 0;
	}

	if (input.find("prlev_hc5") != std::string::npos ||
		input.find("prlev_sc5") != std::string::npos)
	{
		save_html(input);
		std::cout << "Found STANI U PODIA !!!!!!" << std::endl;
		alert();
	}
	else
	{
		std::cout << "Found no tickets!" << std::endl;
	}

	return 0;
}

void atime(char *destination)
{
	time_t rawtime;
	struct tm *timeinfo;
	time(&rawtime);
	timeinfo = localtime(&rawtime);
	strftime(destination, 64, "%X", timeinfo);
}

void alert()
{
	while (1)
	{
		std::cout << '\a';
		Sleep(500);
	}
}

std::string valueBetween(std::string &source, std::string a, std::string b, size_t start)
{
	size_t _found1 = source.find(a, start);
	size_t _found2 = source.find(b, _found1 + a.length());

	if (_found1 == std::string::npos || _found2 == std::string::npos) // if cannot find reference points
		return std::string("");

	std::string value(source.begin() + _found1 + a.length(), source.begin() + _found2);
	return value;
}