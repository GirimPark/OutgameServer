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
        return s_activeUserMap.find(userId)->second;
    }


    // 주기적으로 유저 목록 정리 스레드
    void UpdateActiveUserMap(std::chrono::milliseconds period);

    // 수신 패킷 처리
    void HandleLoginRequest(std::shared_ptr<ReceiveStruct> receiveStructure);
    void HandleLogoutRequest(std::shared_ptr<ReceiveStruct> receiveStructure);
    void HandleJoinRequest(std::shared_ptr<ReceiveStruct> receiveStructure);

    //void HandleValidationResponse(std::shared_ptr<ReceiveStruct> receiveStructure);

private:
    // 새 유저 생성(todo 풀에서 할당 받는 형태로 변경)
    void CreateActiveUser(Session* session, std::string_view name);
    // 로그인 입력 정보 인증
    bool AuthenticateUser(Session* session, const std::string_view& username, const std::string_view& password);
    // 로그아웃
    bool LogoutUser(Session* session);
    // 아이디 중복 확인
    bool IsAvailableID(const std::string_view& username, const std::string_view& password);

private:
    static concurrency::concurrent_unordered_map<UserId, std::shared_ptr<User>> s_activeUserMap;
    CRITICAL_SECTION m_userMapLock;
};

