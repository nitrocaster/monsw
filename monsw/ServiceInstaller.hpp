#pragma once
#include <windows.h>

void InstallService(PWSTR pszServiceName, PWSTR pszDisplayName, DWORD dwServiceType,
    DWORD dwStartType, PWSTR pszDependencies, PWSTR pszAccount, PWSTR pszPassword);

void UninstallService(PWSTR pszServiceName);
