#pragma once
class UserManager
{
public:
    static UserManager& Instance()
    {
        static UserManager instance;
        return instance;
	}

	bool AuthenticateUser(const std::string_view& username, const std::string_view& password);

private:
    UserManager() = default;
    ~UserManager() = default;

    UserManager(const UserManager&) = delete;
    UserManager& operator=(const UserManager&) = delete;
};

