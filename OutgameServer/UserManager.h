#pragma once

class User;

typedef unsigned int SessionId;

class UserManager
{
public:
    UserManager();
    ~UserManager();

    // Ÿ�Ӿƿ� ����
    void SetTimeout(std::chrono::milliseconds timeout) { m_userTimeout = timeout; }

    // �ֱ������� ���� ������ ��Ŷ ������ ������(S2C)
    void BroadcastValidationPacket(std::chrono::milliseconds period);
    // ���� ��Ŷ ó��
    void HandleLoginRequest(std::shared_ptr<ReceiveStruct> receiveStructure);
    void HandleLogoutRequest(std::shared_ptr<ReceiveStruct> receiveStructure);
    void HandleJoinRequest(std::shared_ptr<ReceiveStruct> receiveStructure);

    void HandleValidationResponse(std::shared_ptr<ReceiveStruct> receiveStructure);

private:
    // �� ���� ����(todo Ǯ���� �Ҵ� �޴� ���·� ����)
    void CreateActiveUser(Session* session, std::string_view name);

    // �α��� �Է� ���� ����
    bool AuthenticateUser(Session* session, const std::string_view& username, const std::string_view& password);
    // �α׾ƿ�
    bool LogoutUser(Session* session);
    // ���̵� �ߺ� Ȯ��
    bool IsAvailableID(const std::string_view& username, const std::string_view& password);

    // ���� ��ȿ�� �˻� �� ��� ����
    void UpdateActiveUserMap();

private:
    concurrency::concurrent_unordered_map<SessionId, User*> m_activeUserMap;
    CRITICAL_SECTION m_userMapLock;

    std::chrono::milliseconds m_userTimeout;
};

