#pragma once

class User;

typedef unsigned int SessionId;

class UserManager
{
public:
    UserManager() = default;
    ~UserManager() = default;

    // Ÿ�Ӿƿ� ����
    void SetTimeout(std::chrono::milliseconds timeout) { m_userTimeout = timeout; }

    // �۾� ť ����
    void InsertLoginRequest(std::shared_ptr<ReceiveStruct> task);
    void InsertValidationResponse(std::shared_ptr<ReceiveStruct> task);

    // �ֱ������� ���� ������ ��Ŷ ������ ������(S2C)
    void BroadcastValidationPacket(std::chrono::milliseconds period);
    // �۾� ť ó�� ������(C2S)
    void HandleLoginRequest();
    void HandleValidationResponse();

private:
    // �α��� �Է� ���� ����
    bool AuthenticateUser(const std::string_view& username, const std::string_view& password);
    // ���� ��ȿ�� �˻� �� ��� ����
    void UpdateActiveUserMap();

private:
    concurrency::concurrent_unordered_map<SessionId, User*> m_activeUserMap;

    concurrency::concurrent_queue<std::shared_ptr<ReceiveStruct>> m_loginRequests;
    concurrency::concurrent_queue<std::shared_ptr<ReceiveStruct>> m_validationResponses;

    std::chrono::milliseconds m_userTimeout;

    std::mutex m_mutex;
};

