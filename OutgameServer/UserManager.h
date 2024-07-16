#pragma once

class User;

typedef unsigned int SessionId;

class UserManager
{
public:
    UserManager() = default;
    ~UserManager() = default;

    // 타임아웃 설정
    void SetTimeout(std::chrono::milliseconds timeout) { m_userTimeout = timeout; }

    // 작업 큐 접근
    void InsertLoginRequest(std::shared_ptr<ReceiveStruct> task);
    void InsertValidationResponse(std::shared_ptr<ReceiveStruct> task);

    // 주기적으로 유저 검증용 패킷 보내는 스레드(S2C)
    void BroadcastValidationPacket(std::chrono::milliseconds period);
    // 작업 큐 처리 스레드(C2S)
    void HandleLoginRequest();
    void HandleValidationResponse();

private:
    // 로그인 입력 정보 인증
    bool AuthenticateUser(const std::string_view& username, const std::string_view& password);
    // 유저 유효성 검사 및 목록 정리
    void UpdateActiveUserMap();

private:
    concurrency::concurrent_unordered_map<SessionId, User*> m_activeUserMap;

    concurrency::concurrent_queue<std::shared_ptr<ReceiveStruct>> m_loginRequests;
    concurrency::concurrent_queue<std::shared_ptr<ReceiveStruct>> m_validationResponses;

    std::chrono::milliseconds m_userTimeout;

    std::mutex m_mutex;
};

