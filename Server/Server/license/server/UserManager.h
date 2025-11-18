#pragma once

#include <sql/SqlQuery.h>

namespace license {
	class UserManager;
	class User;

	class UserPermissions {
			friend class User;
		public:
			struct Permission {
				static constexpr auto LICENSE_CREATE = "license_create";
				static constexpr auto LICENSE_CREATE_FROM_TEMPLATE = "license_create_from_template";
			};
		private:
			User* handle;
	};

	class User {
			friend class UserManager;
		public:
			struct Status {
				enum value {
					ACTIVE,
					DISABLED,
					BANNED
				};
			};

			User(UserManager* /* manager */, const std::string& /* username */,const std::string& /* password hash */, Status::value /* status */);

			double balance();
			inline std::string username() { return this->_username; }
			inline std::string password_hash() { return this->_password_hash; }
			inline Status::value status() { return this->_status; }
			bool verify_password(const std::string& /* password */);
		private:
			UserManager* handle;

			Status::value _status;
			std::string _username;
			std::string _password_hash;
			std::string _owner;
			double _balance = 0;
	};

	class UserOffer {
		public:

		private:

	};

	class UserManager {
		public:
			explicit UserManager(sql::SqlManager* /* database */);
			virtual ~UserManager();

			std::shared_ptr<User> find_user(const std::string& /* name */);
		private:
			std::mutex load_user_lock;
			std::mutex loaded_user_lock;
			std::deque<std::shared_ptr<User>> loaded_user;

			sql::SqlManager* database;
	};
}