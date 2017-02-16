#include <stdio.h>
#include <windows.h>
#include <VersionHelpers.h>
#include <Dbt.h>
#include "ServiceInstaller.hpp"
#include <strsafe.h>
#include <powersetting.h>
#pragma comment(lib, "PowrProf.lib")

// Internal name of the service
#define SERVICE_NAME L"monsw"
// Displayed name of the service
#define SERVICE_DISPLAY_NAME L"Lid driven monitor switch"
#define SERVICE_TYPE (SERVICE_WIN32_OWN_PROCESS|SERVICE_INTERACTIVE_PROCESS)
// Service start options.
#define SERVICE_START_TYPE SERVICE_DEMAND_START
// List of service dependencies - "dep1\0dep2\0\0"
#define SERVICE_DEPENDENCIES L""
// The name of the account under which the service should run 
#define SERVICE_ACCOUNT L".\\LocalSystem"
// The password to the service account name
#define SERVICE_PASSWORD NULL

SERVICE_STATUS_HANDLE gStatusHandle = NULL;
HPOWERNOTIFY gPowerNotify = NULL;
HANDLE ghSvcStopEvent;

#define SVC_ERROR ((DWORD)0xC0020001L)
#define SVC_MESSAGE ((DWORD)0x20000001L)
#define SLEEP_DELAY 1800

VOID SvcReportEvent(LPWSTR szMessage)
{
    HANDLE hEventSource;
    LPCWSTR lpszStrings[2];
    hEventSource = RegisterEventSourceW(NULL, SERVICE_NAME);
    if (hEventSource)
    {
        lpszStrings[0] = SERVICE_NAME;
        lpszStrings[1] = szMessage;
        ReportEventW(
            hEventSource, // event log handle
            EVENTLOG_INFORMATION_TYPE, // event type
            0, // event category
            SVC_MESSAGE, // event identifier
            NULL, // no security identifier
            2, // size of lpszStrings array
            0, // no binary data
            lpszStrings, // array of strings
            NULL); // no binary data
        DeregisterEventSource(hEventSource);
    }
}

void SetStatus(DWORD dwCurrentState, DWORD dwWin32ExitCode = NO_ERROR, DWORD dwWaitHint = 0)
{
    static DWORD dwCheckPoint = 1;
    SERVICE_STATUS status = {};
    status.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
    status.dwServiceType = SERVICE_TYPE;
    status.dwServiceSpecificExitCode = 0;
    status.dwCurrentState = dwCurrentState;
    status.dwWin32ExitCode = dwWin32ExitCode;
    status.dwWaitHint = dwWaitHint;
    if (dwCurrentState!=SERVICE_RUNNING && dwCurrentState!=SERVICE_STOPPED)
        status.dwCheckPoint = dwCheckPoint++;
    SetServiceStatus(gStatusHandle, &status);
}

void RegisterPowerNotify()
{
    if (gPowerNotify)
        return;
    if (!gStatusHandle)
        return;
    gPowerNotify = RegisterPowerSettingNotification(gStatusHandle, &GUID_LIDSWITCH_STATE_CHANGE, DEVICE_NOTIFY_SERVICE_HANDLE);
}

void UnregisterPowerNotify()
{
    if (!gPowerNotify)
        return;
    UnregisterPowerSettingNotification(gPowerNotify);
    gPowerNotify = NULL;
}

typedef DWORD (WINAPI *PowerReadValueFunc)(
    HKEY RootPowerKey, CONST GUID *SchemeGuid, CONST GUID *SubGroupOfPowerSettingsGuid,
    CONST GUID *PowerSettingGuid, PULONG Type, _Out_writes_bytes_opt_(*BufferSize) PUCHAR Buffer,
    LPDWORD BufferSize);
typedef DWORD (WINAPI* PowerWriteValueIndexFunc)(
    HKEY RootPowerKey, CONST GUID *SchemeGuid, CONST GUID *SubGroupOfPowerSettingsGuid,
    CONST GUID *PowerSettingGuid, DWORD ValueIndex);

DWORD WINAPI ServiceCtrlHandlerEx(DWORD dwControl, DWORD dwEventType, LPVOID lpEventData, LPVOID lpContext)
{
    switch (dwControl)
    {
    case SERVICE_CONTROL_STOP:
    case SERVICE_CONTROL_SHUTDOWN:
        UnregisterPowerNotify();
        SetEvent(ghSvcStopEvent);
        SetStatus(SERVICE_STOPPED);
        return NO_ERROR;
    case SERVICE_CONTROL_INTERROGATE:
        return NO_ERROR;
    case SERVICE_CONTROL_POWEREVENT:
        if (dwEventType==PBT_POWERSETTINGCHANGE)
        {
            POWERBROADCAST_SETTING *setting = (POWERBROADCAST_SETTING *)lpEventData;
            DWORD state = *(DWORD *)setting->Data;
            switch (state)
            {
            case 0: // closed: disable display
            {
                GUID *pwrGuid;
                if (PowerGetActiveScheme(NULL, &pwrGuid)!=ERROR_SUCCESS)
                {
                    SvcReportEvent(L"PowerGetActiveScheme failed");
                    break;
                }
                SYSTEM_POWER_STATUS pwrStatus;
                if (!GetSystemPowerStatus(&pwrStatus))
                {
                    SvcReportEvent(L"GetSystemPowerStatus failed");
                    break;
                }
                int dc = !pwrStatus.ACLineStatus;
                UINT32 val = 300;
                DWORD size = sizeof(val);
                PowerReadValueFunc PowerReadValue = dc ? PowerReadDCValue : PowerReadACValue;
                PowerWriteValueIndexFunc PowerWriteValueIndex = dc ? PowerWriteDCValueIndex : PowerWriteACValueIndex;
                if (PowerReadValue(NULL, pwrGuid, &GUID_VIDEO_SUBGROUP, &GUID_VIDEO_POWERDOWN_TIMEOUT,
                    NULL, (UCHAR *)&val, &size)==ERROR_SUCCESS)
                {
                    PowerWriteValueIndex(NULL, pwrGuid, &GUID_VIDEO_SUBGROUP, &GUID_VIDEO_POWERDOWN_TIMEOUT, 1);
                    if (PowerSetActiveScheme(NULL, pwrGuid)!=ERROR_SUCCESS)
                    {
                        SvcReportEvent(L"PowerSetActiveScheme failed");
                        break;
                    }
                    Sleep(SLEEP_DELAY);
                    PowerWriteValueIndex(NULL, pwrGuid, &GUID_VIDEO_SUBGROUP, &GUID_VIDEO_POWERDOWN_TIMEOUT, val);
                    PowerSetActiveScheme(NULL, pwrGuid);
                }
                else
                {
                    SvcReportEvent(L"PowerReadACValue/PowerReadDCValue failed");
                    break;
                }
                break;
            }
            case 1: // opened: enable display
                // OS does that automatically
                break;
            }
        }
        return NO_ERROR;
    }
    return ERROR_CALL_NOT_IMPLEMENTED;
}

void WINAPI ServiceMain(DWORD dwArgc, PWSTR *pszArgv)
{
    // Register the handler function for the service
    gStatusHandle = RegisterServiceCtrlHandlerExW(SERVICE_NAME, ServiceCtrlHandlerEx, NULL);
    if (!gStatusHandle)
    {
        return; // fail with GetLastError
    }
    ghSvcStopEvent = CreateEventW(
        NULL, // default security attributes
        TRUE, // manual reset event
        FALSE, // not signaled
        NULL); // no name
    if (!ghSvcStopEvent)
    {
        SetStatus(SERVICE_STOPPED);
        return;
    }
    SetStatus(SERVICE_RUNNING);
    RegisterPowerNotify();
    while (1)
    {
        WaitForSingleObject(ghSvcStopEvent, INFINITE);
        SetStatus(SERVICE_STOPPED);
        return;
    }
}

BOOL RunService()
{
    SERVICE_TABLE_ENTRYW serviceTable[] =
    {
        {SERVICE_NAME, ServiceMain},
        {NULL, NULL}
    };
    return StartServiceCtrlDispatcherW(serviceTable);
}

int wmain(int argc, wchar_t *argv[])
{
    if (argc>1 && ((*argv[1]==L'-' || *argv[1]==L'/')))
    {
        if (!_wcsicmp(L"install", argv[1]+1))
        {
            // install the service when the command is "-install" or "/install"
            InstallService(SERVICE_NAME, SERVICE_DISPLAY_NAME, SERVICE_TYPE,
                SERVICE_START_TYPE, SERVICE_DEPENDENCIES, SERVICE_ACCOUNT, SERVICE_PASSWORD);
        }
        else if (!_wcsicmp(L"remove", argv[1]+1))
        {
            // uninstall the service when the command is "-remove" or "/remove"
            UninstallService(SERVICE_NAME);
        }
    }
    else
    {
        wprintf(L"Parameters:\n");
        wprintf(L" -install  to install the service.\n");
        wprintf(L" -remove   to remove the service.\n");
        if (!RunService())
            wprintf(L"Service failed to run w/err 0x%08lx\n", GetLastError());
    }
    return 0;
}
