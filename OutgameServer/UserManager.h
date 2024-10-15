#pragma once

class User;

typedef unsigned int UserId;

class UserManager
{
public:
    UserManager();
    ~UserManager();


    static std::weak_ptr<User> FindActiveUser(UserId userId)
    {
        auto it = s_activeUserMap.find(userId);
        if (it != s_activeUserMap.end()) 
            return it->second;

        LOG_CONTENTS("FindActiveUser Failed: This UserId is Invalid");
    	return std::weak_ptr<User>();
    }
    static std::weak_ptr<User> FindActiveUser(const std::string_view& username)
    {
        auto it = s_activeUsername.find(std::string(username.begin(), username.end()));
        if (it != s_activeUsername.end())
            return s_activeUserMap.find(it->second)->second;

        LOG_CONTENTS("FindActiveUser Failed: This UserName is Invalid");
    	return std::weak_ptr<User>();
    }

    // �ֱ������� ���� ��� ���� ������
    void UpdateActiveUserMap(std::chrono::milliseconds period);

    // ���� ��Ŷ ó��
    void HandleLoginRequest(std::shared_ptr<ReceiveStruct> receiveStructure);
    void HandleLogoutRequest(std::shared_ptr<ReceiveStruct> receiveStructure);
    void HandleJoinRequest(std::shared_ptr<ReceiveStruct> receiveStructure);
    void HandleFindFriendRequest(std::shared_ptr<ReceiveStruct> receiveStructure);
    void HandleAddFriendRequest(std::shared_ptr<ReceiveStruct> receiveStructure);
    void HandleAcceptFriendRequest(std::shared_ptr<ReceiveStruct> receiveStructure);
    void HandleRefuseFriendRequest(std::shared_ptr<ReceiveStruct> receiveStructure);


private:
    // �� ���� ����(todo Ǯ���� �Ҵ� �޴� ���·� ����)
    void CreateActiveUser(Session* session, std::string_view name);
    // �α��� �Է� ���� ����
    bool AuthenticateUser(Session* session, const std::string_view& username, const std::string_view& password);
    // �α׾ƿ�
    bool LogoutUser(Session* session);
    // ���̵� �ߺ� Ȯ��
    bool IsAvailableID(const std::string_view& username, const std::string_view& password);
    // ģ�� �˻�
    bool FindUser(const std::string_view& username, const std::string_view& friendName, OUT int& friendState, OUT int& requestState);
    // ģ�� ��û
    bool AddFriend(const std::string_view& username, const std::string_view& friendName);
    // ģ�� ����
    bool AcceptFriend(const std::string_view& username, const std::string_view& friendName, OUT int& friendState);
    // ģ�� ����
    bool RefuseFriend(const std::string_view& username, const std::string_view& friendName);

private:
    static concurrency::concurrent_unordered_map<UserId, std::shared_ptr<User>> s_activeUserMap;
    static concurrency::concurrent_unordered_map<std::string, UserId> s_activeUsername;
    CRITICAL_SECTION m_userMapLock;
};

