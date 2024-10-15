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

    // 주기적으로 유저 목록 정리 스레드
    void UpdateActiveUserMap(std::chrono::milliseconds period);

    // 수신 패킷 처리
    void HandleLoginRequest(std::shared_ptr<ReceiveStruct> receiveStructure);
    void HandleLogoutRequest(std::shared_ptr<ReceiveStruct> receiveStructure);
    void HandleJoinRequest(std::shared_ptr<ReceiveStruct> receiveStructure);
    void HandleFindFriendRequest(std::shared_ptr<ReceiveStruct> receiveStructure);
    void HandleAddFriendRequest(std::shared_ptr<ReceiveStruct> receiveStructure);
    void HandleAcceptFriendRequest(std::shared_ptr<ReceiveStruct> receiveStructure);
    void HandleRefuseFriendRequest(std::shared_ptr<ReceiveStruct> receiveStructure);


private:
    // 새 유저 생성(todo 풀에서 할당 받는 형태로 변경)
    void CreateActiveUser(Session* session, std::string_view name);
    // 로그인 입력 정보 인증
    bool AuthenticateUser(Session* session, const std::string_view& username, const std::string_view& password);
    // 로그아웃
    bool LogoutUser(Session* session);
    // 아이디 중복 확인
    bool IsAvailableID(const std::string_view& username, const std::string_view& password);
    // 친구 검색
    bool FindUser(const std::string_view& username, const std::string_view& friendName, OUT int& friendState, OUT int& requestState);
    // 친구 신청
    bool AddFriend(const std::string_view& username, const std::string_view& friendName);
    // 친구 수락
    bool AcceptFriend(const std::string_view& username, const std::string_view& friendName, OUT int& friendState);
    // 친구 거절
    bool RefuseFriend(const std::string_view& username, const std::string_view& friendName);

private:
    static concurrency::concurrent_unordered_map<UserId, std::shared_ptr<User>> s_activeUserMap;
    static concurrency::concurrent_unordered_map<std::string, UserId> s_activeUsername;
    CRITICAL_SECTION m_userMapLock;
};

