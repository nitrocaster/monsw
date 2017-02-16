#include <stdio.h>
#include "ServiceInstaller.hpp"

void InstallService(PWSTR pszServiceName,
    PWSTR pszDisplayName,
    DWORD dwServiceType,
    DWORD dwStartType,
    PWSTR pszDependencies,
    PWSTR pszAccount,
    PWSTR pszPassword)
{
    WCHAR szPath[MAX_PATH];
    SC_HANDLE schSCManager = NULL;
    SC_HANDLE schService = NULL;
    if (!GetModuleFileNameW(NULL, szPath, ARRAYSIZE(szPath)))
    {
        wprintf(L"GetModuleFileName failed w/err 0x%08lx\n", GetLastError());
        goto Cleanup;
    }
    schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT|SC_MANAGER_CREATE_SERVICE);
    if (!schSCManager)
    {
        wprintf(L"OpenSCManager failed w/err 0x%08lx\n", GetLastError());
        goto Cleanup;
    }
    schService = CreateServiceW(
        schSCManager,
        pszServiceName,
        pszDisplayName,
        SERVICE_QUERY_STATUS, // desired access
        dwServiceType,
        dwStartType,
        SERVICE_ERROR_NORMAL, // error control type
        szPath,
        NULL, // no load ordering group
        NULL, // no tag identifier
        pszDependencies,
        pszAccount,
        pszPassword
    );
    if (!schService)
    {
        wprintf(L"CreateService failed w/err 0x%08lx\n", GetLastError());
        goto Cleanup;
    }
    wprintf(L"%s is installed.\n", pszServiceName);
Cleanup:
    if (schSCManager)
    {
        CloseServiceHandle(schSCManager);
        schSCManager = NULL;
    }
    if (schService)
    {
        CloseServiceHandle(schService);
        schService = NULL;
    }
}

void UninstallService(PWSTR pszServiceName)
{
    SC_HANDLE schSCManager = NULL;
    SC_HANDLE schService = NULL;
    SERVICE_STATUS ssSvcStatus;
    ZeroMemory(&ssSvcStatus, sizeof(ssSvcStatus));
    // Open the local default service control manager database 
    schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);
    if (!schSCManager)
    {
        wprintf(L"OpenSCManager failed w/err 0x%08lx\n", GetLastError());
        goto Cleanup;
    }
    // Open the service with delete, stop, and query status permissions 
    schService = OpenServiceW(schSCManager, pszServiceName, SERVICE_STOP|SERVICE_QUERY_STATUS|DELETE);
    if (!schService)
    {
        wprintf(L"OpenService failed w/err 0x%08lx\n", GetLastError());
        goto Cleanup;
    }
    // Try to stop the service 
    if (ControlService(schService, SERVICE_CONTROL_STOP, &ssSvcStatus))
    {
        wprintf(L"Stopping %s.", pszServiceName);
        Sleep(1000);
        while (QueryServiceStatus(schService, &ssSvcStatus))
        {
            if (ssSvcStatus.dwCurrentState==SERVICE_STOP_PENDING)
            {
                wprintf(L".");
                Sleep(1000);
            }
            else
                break;
        }
        if (ssSvcStatus.dwCurrentState==SERVICE_STOPPED)
            wprintf(L"\n%s is stopped.\n", pszServiceName);
        else
            wprintf(L"\n%s failed to stop.\n", pszServiceName);
    }
    // Now remove the service by calling DeleteService. 
    if (!DeleteService(schService))
    {
        wprintf(L"DeleteService failed w/err 0x%08lx\n", GetLastError());
        goto Cleanup;
    }
    wprintf(L"%s is removed.\n", pszServiceName);
Cleanup:
    // Centralized cleanup for all allocated resources. 
    if (schSCManager)
    {
        CloseServiceHandle(schSCManager);
        schSCManager = NULL;
    }
    if (schService)
    {
        CloseServiceHandle(schService);
        schService = NULL;
    }
}
