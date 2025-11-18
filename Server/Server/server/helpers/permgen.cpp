#include <fstream>
#include <query/Command.h>

#include <functional> /* required from permission manager */
#include "Definitions.h"
#include "PermissionManager.h"

using namespace std;
using namespace ts;

enum GroupType {
	GENERAL,
	SERVER,
	CHANNEL
};
enum GroupUpdateType {
	NONE = 0,

	CHANNEL_GUEST = 10,
	CHANNEL_VOICE = 25,
	CHANNEL_OPERATOR = 35,
	CHANNEL_ADMIN = 40,

	SERVER_GUEST = 15,
	SERVER_NORMAL = 30,
	SERVER_ADMIN = 45,

	QUERY_GUEST = 20,
	QUERY_ADMIN = 50
};

/*
Value 10: The group will be handled like 'Channel Guest'
Value 15: The group will be handled like 'Server Guest'
Value 20: The group will be handled like 'Query Guest'
Value 25: The group will be handled like 'Channel Voice'
Value 30: The group will be handled like 'Server Normal'
Value 35: The group will be handled like 'Channel Operator'
Value 40: The group will be handled like 'Channel Admin'
Value 45: The group will be handled like 'Server Admin'
Value 50: The group will be handled like 'Query Admin'
 */


enum Target {
	TARGET_QUERY = 0,
	TARGET_SERVER = 1,
	TARGET_CHANNEL = 2
};

struct Group {
	Target target;
	string name;
	deque<permission::update::UpdatePermission> permissions;
};

map<Target, map<string, vector<string>>> property_mapping = {
	   {TARGET_QUERY, {
			                  {"Guest Server Query", {"serverinstance_guest_serverquery_group"}},
			                  {"Admin Server Query", {"serverinstance_admin_serverquery_group"}}
       }},
	   {TARGET_SERVER, {
			                  {"Server Admin", {"serverinstance_template_serveradmin_group"}},
			                  {"Guest", {"serverinstance_template_serverdefault_group", "serverinstance_template_musicdefault_group"}}
       }},
	   {TARGET_CHANNEL, {
		                      {"Channel Admin", {"serverinstance_template_channeladmin_group"}},
		                      {"Guest", {"serverinstance_template_channeldefault_group"}}
       }},
};

inline bool read_line(ifstream& in, string& line) {
	if(!getline(in, line)) return false;
	while(!line.empty()) {
		if(line.back() == '\r') line = line.substr(0, line.length() - 1);
		else if(line.front() == ' ' || line.front() == '\r' || (unsigned char) line.front() > 0x80) line = line.substr(1);
		else break;
	}
	return true;
}

#define PRINT_UNMAP(type)                                                                       \
do {                                                                                            \
	cout << type << " => {";                                                                    \
	auto e = permission::teamspeak::unmap_key(type, permission::teamspeak::GroupType::SERVER);  \
	for(auto it = e.begin(); it != e.end(); it++) {                                             \
		cout << *it;                                                                            \
		if(it + 1 != e.end())                                                                   \
			cout << ", ";                                                                       \
	}                                                                                           \
	cout << "}" << endl;                                                                        \
} while(0)

#define PRINT_MAP(type)                                                                         \
do {                                                                                            \
	cout << type << " => {";                                                                    \
	auto e = permission::teamspeak::map_key(type, permission::teamspeak::GroupType::SERVER);    \
	for(auto it = e.begin(); it != e.end(); it++) {                                             \
		cout << *it;                                                                            \
		if(it + 1 != e.end())                                                                   \
			cout << ", ";                                                                       \
	} \
	cout << "}" << endl; \
} while(0)

static constexpr bool USE_MAPPING = false;
int main(int argc, char** argv) {
	PRINT_UNMAP("i_client_music_needed_rename_power");
	PRINT_UNMAP("b_client_music_channel_list");
	PRINT_UNMAP("i_client_music_info");

	PRINT_UNMAP("i_client_max_clones_ip"); //=> i_client_max_clones_uid
	PRINT_UNMAP("i_client_max_clones_hwid"); //=> i_client_max_clones_uid
	PRINT_UNMAP("i_client_max_clones_uid"); //=> i_client_max_clones_uid

	PRINT_UNMAP("i_server_group_needed_modify_power"); //=> i_client_max_clones_uid
	PRINT_UNMAP("i_displayed_group_needed_modify_power"); //=> i_client_max_clones_uid

	cout << "--------- map ----------" << endl;
	PRINT_MAP("i_client_max_clones_uid");
	PRINT_MAP("i_group_needed_modify_power");

	deque<Group> groups;
	{
		ifstream file("../helpers/server_groups"); /* the new file is already mapped! */
		string line;
		while (read_line(file, line))
		{
			Group group{};
			group.name = line;
			read_line(file, line);
			group.target = line == "2" ? TARGET_QUERY : TARGET_SERVER;
			read_line(file, line);
			auto data = "perms " + line;
			ts::Command group_parms = ts::Command::parse(data);

			map<permission::PermissionType, permission::update::UpdatePermission> grantMapping;
			for (int index = 0; index < group_parms.bulkCount(); index++) {
				auto permission_name = group_parms[index]["permsid"].string();
				auto permissions = USE_MAPPING ? permission::teamspeak::map_key(permission_name, permission::teamspeak::SERVER) : std::deque<std::string>({permission_name});

				for(const auto& permission : permissions) {
					auto type = permission::resolvePermissionData(permission);
					if(type->type == permission::unknown) {
						cerr << "Failed to parse type " << permission << " (" << permission_name << ")!" << endl;
						continue;
					}
					if(type->grantName() == permission) {
						permission::update::UpdatePermission entry;
						for(auto& perm : group.permissions)
							if(perm.name == type->name) {
								perm.granted = group_parms[index]["permvalue"];
								goto jmp_out_a;
							}
						entry.name = type->name;
						entry.granted = group_parms[index]["permvalue"];
						group.permissions.push_back(entry);
						jmp_out_a:;
					} else {
						permission::update::UpdatePermission entry;
						entry.name = permission;
						entry.value = group_parms[index]["permvalue"];
						entry.negated = group_parms[index]["permnegated"];
						entry.skipped = group_parms[index]["permskip"];
						group.permissions.push_back(entry);
					}
				}
			}
			groups.push_back(group);
		}
		file.close();
	}
	{
		ifstream file("../helpers/channel_groups");
		string line;
		while (read_line(file, line))
		{
			Group group{};
			group.name = line;
			read_line(file, line);
			group.target = TARGET_CHANNEL;
			read_line(file, line);
			auto data = "perms " + line;
			ts::Command group_parms = ts::Command::parse(data);

			map<permission::PermissionType, permission::update::UpdatePermission> grantMapping;
			for (int index = 0; index < group_parms.bulkCount(); index++) {
				auto permission_name = group_parms[index]["permsid"].string();
				auto permissions = permission::teamspeak::map_key(permission_name, permission::teamspeak::CHANNEL);

				for(const auto& permission : permissions) {
					auto type = permission::resolvePermissionData(permission);
					if(type->type == permission::unknown) {
						cerr << "Failed to parse type " << permission << " (" << permission_name << ")!" << endl;
						continue;
					}

					if(type->grantName() == permission) {
						permission::update::UpdatePermission entry;
						for(auto& perm : group.permissions)
							if(perm.name == type->name) {
								perm.granted = group_parms[index]["permvalue"];
								goto jmp_out_b;
							}
						entry.name = type->name;
						entry.granted = group_parms[index]["permvalue"];
						group.permissions.push_back(entry);
						jmp_out_b:;
					} else {
						permission::update::UpdatePermission entry;
						entry.name = permission;
						entry.value = group_parms[index]["permvalue"];
						entry.negated = group_parms[index]["permnegated"];
						entry.skipped = group_parms[index]["permskip"];
						group.permissions.push_back(entry);
					}
				}
			}
			groups.push_back(group);
		}
		file.close();
	}

	cout << "Got " << groups.size() << " groups" << endl;
	ofstream of("permissions.template");

	of << "# This is a auto generated template file!" << endl;
	of << "# DO NOT EDIT IF YOU'RE NOT SURE WHAT YOU'RE DOING!" << endl;
	of << "# Syntax:" << endl;
	of << "# Every entry starts with a '--start' and ends with a '--end'" << endl;
	of << "# Every entry has the following properties:" << endl;
	of << "# name [string] -> The name of the entry" << endl;
	of << "# target [numeric] -> The type of the entry {QUERY | SERVER | CHANNEL}" << endl;
	of << "# property [string] -> The applied property of the entry" << endl;
	of << "# permission [[string],[numeric],[numeric],[flag],[flag]] -> A permission applied on the entry ([name],[value],[granted],[skipped],[negated])" << endl;
	of << "#" << endl;

	for(const auto& group : groups) {
		of << "--start" << endl;
		of << "name:" << group.name << endl;
		of << "target:" << group.target << endl;
		for(const auto& property : property_mapping[group.target][group.name])
			of << "property:" << property << endl;
		for(const auto& perm : group.permissions) {
			of << "permission:" << perm.name << "=" << perm.value << "," << perm.granted << "," << perm.skipped << "," << perm.negated << endl;
		}

		if(USE_MAPPING) {
			for(const auto& perm : group.permissions) {
				if(perm.name == "i_group_auto_update_type") {
					for(const auto& insert : permission::update::migrate) {
						if(insert.type == perm.value) {
							of << "permission:" << insert.permission.name << "=" << insert.permission.value << "," << insert.permission.granted << "," << insert.permission.skipped << "," << insert.permission.negated << endl;
							cout << "Auto insert permission " << insert.permission.name << " (" << insert.type << ")" << endl;
						}
					}
					break;
				}
			}
		}
		of << "--end" << endl;
	}

    of.close();
}