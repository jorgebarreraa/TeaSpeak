#include <fstream>
#include <query/Command.h>
#include <cstring>
#include <utility>

#include <functional> /* required from permission manager */
#include "log/LogUtils.h"
#include "Definitions.h"
#include "PermissionManager.h"

using namespace std;
using namespace ts;

/* Took from the permission mapper within the TeaSpeakServer */
enum PermissionMapGroup {
	MIN,
	TS3 = MIN,
	TEAWEB,
	TEACLIENT,
	QUERY,
	MAX
};

std::map<PermissionMapGroup, string> group_names = {
		{PermissionMapGroup::TS3, "TeamSpeak 3"},
		{PermissionMapGroup::TEAWEB, "TeaSpeak-Web"},
		{PermissionMapGroup::TEACLIENT, "TeaSpeak-Client"},
		{PermissionMapGroup::QUERY, "Query"}
};

//TODO: Does it work with a space at the end?
#define I "\x5f\xcc\xb2" /* an underscore with an non-spacing underscore */
std::map<string, string> replacements = {
		{"_music", I "music"},
		{"_hwid", I "hwid" },
		{"_playlist", I "playlist"}
};

std::string replace_all(std::string str, const std::string& from, const std::string& to) {
	size_t start_pos = 0;
	while((start_pos = str.find(from, start_pos)) != std::string::npos) {
		str.replace(start_pos, from.length(), to);
		start_pos += to.length(); // Handles case where 'to' is a substring of 'from'
	}
	return str;
}


int main(int argc, char** argv) {
	ofstream of("permission_mapping.txt");

	of << "# This is a auto generated template file!" << endl;
	of << "# DO NOT EDIT IF YOU'RE NOT SURE WHAT YOU'RE DOING!" << endl;
	of << "# Syntax:" << endl;
	of << "# group:<group id> -> group id values: 0 := TS3 | 1 := TeaWeb | 2 := TeaClient | 3 := Query " << endl;
	of << "# mapping:<original name>:<mapped value>" << endl;
	of << "# Note: Be aware of spaces and line endings. The TeaSpeakServer does not trim the values!" << endl;
	of << "#" << endl;


	for(PermissionMapGroup type = PermissionMapGroup::MIN; type < PermissionMapGroup::MAX; (*(int*) &type)++) {
		of << "# Begin mapping for group " << (int) type << " (" << group_names[type] << ")" << endl;
		of << "group:" << (int) type << endl;

		if(type == PermissionMapGroup::TS3) {
			for(const auto& permission : permission::availablePermissions) {
				if(!permission->clientSupported)
					continue;

				auto value = permission->name;
				for(auto& replacement : replacements)
					value = replace_all(value, replacement.first, replacement.second);
				of << "mapping:" << permission->name << ":" << value << endl;
			}
		} else {
			of << "#  No mapping required here. You're of course free to add stuff here." << endl;
		}
		of << "# End mapping of group" << endl;
	}

	of.close();
}