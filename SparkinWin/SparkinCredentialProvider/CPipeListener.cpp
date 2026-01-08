/*
 * Copyright (c) 2026 Tomosawa
 * https://github.com/Tomosawa/
 * All rights reserved
 */

#include <tchar.h>
#include <aclapi.h>
#include "CPipeListener.h"

#define BUFSIZE 512
const int ALLOCSIZE = 512 * sizeof(WCHAR);

CPipeListener::CPipeListener(void)
{
    _pProvider = NULL;
    _fUnlocked = FALSE;

    HANDLE hHeap = GetProcessHeap();
    _pwzUsername = (PWSTR)HeapAlloc(hHeap, 0, ALLOCSIZE);
    _pwzPassword = (PWSTR)HeapAlloc(hHeap, 0, ALLOCSIZE);
}

CPipeListener::~CPipeListener(void)
{
    if (_pProvider != NULL)
    {
        _pProvider->Release();
        _pProvider = NULL;
    }
}

HRESULT CPipeListener::Initialize(CSampleProvider *pProvider)
{
    HRESULT hr = S_OK;

    // Be sure to add a release any existing provider we might have, then add a reference
    // to the provider we're working with now.
    if (_pProvider != NULL)
    {
        _pProvider->Release();
    }
    _pProvider = pProvider;
    _pProvider->AddRef();

    // Create and launch the pipe thread.
    HANDLE hThread = ::CreateThread(NULL, 0, CPipeListener::_ThreadProc, (LPVOID) this, 0, NULL);
    if (hThread == NULL)
    {
        hr = HRESULT_FROM_WIN32(::GetLastError());
    }

    return hr;
}

BOOL CPipeListener::GetUnlockingStatus()
{
    return _fUnlocked;
}

void CPipeListener::GetCredential(PWSTR *pwzUsername, PWSTR *pwzPassword)
{
    *pwzUsername = _pwzUsername;
    *pwzPassword = _pwzPassword;
}

DWORD WINAPI CPipeListener::_ThreadProc(LPVOID lpParameter)
{
    CPipeListener *pPipeListener = static_cast<CPipeListener *>(lpParameter);

    LPTSTR lpszPipename = TEXT("\\\\.\\pipe\\SparkinCredentialProvider");

    HANDLE hPipe = INVALID_HANDLE_VALUE;
    BOOL fConnected = FALSE;

    BOOL fSuccess = FALSE;
    DWORD cbUsername = 0, cbPassword = 0;
    
    // Everyone writable ACL
    PSID pEveryoneSID = NULL;
    PACL pACL = NULL;
    PSECURITY_DESCRIPTOR pSD = NULL;
    EXPLICIT_ACCESS ea;
    SID_IDENTIFIER_AUTHORITY SIDAuthWorld = SECURITY_WORLD_SID_AUTHORITY;
    SECURITY_ATTRIBUTES sa;
    // Create a well-known SID for the Everyone group.
    AllocateAndInitializeSid(&SIDAuthWorld, 1,
        SECURITY_WORLD_RID,
        0, 0, 0, 0, 0, 0, 0,
        &pEveryoneSID);
    // Initialize an EXPLICIT_ACCESS structure for an ACE
    ZeroMemory(&ea, sizeof(EXPLICIT_ACCESS));
    ea.grfAccessPermissions = GENERIC_ALL;
    ea.grfAccessMode = SET_ACCESS;
    ea.grfInheritance = NO_INHERITANCE;
    ea.Trustee.TrusteeForm = TRUSTEE_IS_SID;
    ea.Trustee.TrusteeType = TRUSTEE_IS_GROUP;
    ea.Trustee.ptstrName = (LPTSTR)pEveryoneSID;
    // Create a new ACL that contains the new ACEs.
    SetEntriesInAcl(1, &ea, NULL, &pACL);
    // Initialize a security descriptor.  
    pSD = (PSECURITY_DESCRIPTOR)LocalAlloc(LPTR, SECURITY_DESCRIPTOR_MIN_LENGTH);
    InitializeSecurityDescriptor(pSD, SECURITY_DESCRIPTOR_REVISION);
    // Add the ACL to the security descriptor. 
    SetSecurityDescriptorDacl(pSD, TRUE, pACL, FALSE);
    // Initialize a security attributes structure.
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.lpSecurityDescriptor = pSD;
    sa.bInheritHandle = FALSE;    

    for (;;)
    {
        hPipe = CreateNamedPipe(
            lpszPipename,
            PIPE_ACCESS_INBOUND,
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
            PIPE_UNLIMITED_INSTANCES,
            BUFSIZE, BUFSIZE,
            0,
            &sa);
        fConnected = ConnectNamedPipe(hPipe, NULL);

        fSuccess = FALSE;
        cbUsername = 0;
        cbPassword = 0;
        fSuccess = ReadFile(hPipe, pPipeListener->_pwzUsername, ALLOCSIZE, &cbUsername, NULL);
        fSuccess &= ReadFile(hPipe, pPipeListener->_pwzPassword, ALLOCSIZE, &cbPassword, NULL);

        FlushFileBuffers(hPipe);
        DisconnectNamedPipe(hPipe);
        CloseHandle(hPipe);

        if (fSuccess && cbUsername && cbUsername)
        {
            pPipeListener->_fUnlocked = TRUE;
            pPipeListener->_pProvider->OnUnlockingStatusChanged();
            break;
        }
    }

    if (pEveryoneSID)
        FreeSid(pEveryoneSID);
    if (pACL)
        LocalFree(pACL);
    if (pSD)
        LocalFree(pSD);
    return 0;
}
