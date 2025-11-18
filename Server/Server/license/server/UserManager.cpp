#include "UserManager.h"
#include <misc/base64.h>
#include <misc/digest.h>
#include <log/LogUtils.h>

using namespace license;
using namespace std;

UserManager::UserManager(sql::SqlManager *db) : database(db) {}
UserManager::~UserManager() {}

std::shared_ptr<User> UserManager::find_user(const std::string &username) {
	unique_lock lock(this->loaded_user_lock);
	for(const auto& user : this->loaded_user)
		if(user->username() == username)
			return user;

	lock.unlock();
	lock_guard load_lock(this->load_user_lock);
	lock.lock();
	for(const auto& user : this->loaded_user)
		if(user->username() == username)
			return user;
	lock.unlock();

	std::shared_ptr<User> user;
	sql::command(this->database, "SELECT `username`, `password_hash`, `status`, `owner` FROM `users` WHERE `username` = :username", variable{":username", username}).query([&](int length, string* values, string* key) {
		string username, password_hash, owner;
		int status;

		for(int index = 0; index < length; index++) {
			try {

				if(key[index] == "username")
					username = values[index];
				else if(key[index] == "password_hash")
					password_hash = values[index];
				else if(key[index] == "status")
					status = (User::Status::value) stoll(values[index]);
				else if(key[index] == "owner")
					owner = values[index];
			} catch(std::exception& ex) {
				logError(LOG_LICENSE_CONTROLL, "Failed to load user {}. Failed to parse field {} ({})", username, key[index], values[index]);
				return;
			}
		}

		user = make_shared<User>(this, username, password_hash, status);
		user->_owner = owner;

		lock.lock();
		this->loaded_user.push_back(user);
		lock.unlock();
	});

	return user;
}

User::User(license::UserManager *handle, const std::string &name, const std::string &password, license::User::Status::value status) : handle(handle), _username(name), _password_hash(password), _status(status) {}

bool User::verify_password(const std::string &password) {
	auto hashed_password = base64::encode(digest::sha1(password));
	return this->_password_hash == hashed_password;
}