#include "pch.h"
#include "User.h"

void User::UpdateStatus(EUserStateType status)
{
	m_status = status;

	//todo DBUpdate
}
