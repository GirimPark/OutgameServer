#pragma once

class User;

typedef unsigned int UserId;

class UserManager
{
public:
    UserManager();
    ~UserManager();

    // �ֱ������� ���� ��� ���� ������
    void UpdateActiveUserMap(std::chrono::milliseconds period);
    // ���� ��Ŷ ó��
    void HandleLoginRequest(std::shared_ptr<ReceiveStruct> receiveStructure);
    void HandleLogoutRequest(std::shared_ptr<ReceiveStruct> receiveStructure);
    void HandleJoinRequest(std::shared_ptr<ReceiveStruct> receiveStructure);

    void HandleValidationResponse(std::shared_ptr<ReceiveStruct> receiveStructure);

    static std::weak_ptr<User> FindActiveUser(UserId userId) { return s_activeUserMap.find(userId)->second; }

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
    //void UpdateActiveUserMap();

private:
    static concurrency::concurrent_unordered_map<UserId, std::shared_ptr<User>> s_activeUserMap;
    CRITICAL_SECTION m_userMapLock;
};

