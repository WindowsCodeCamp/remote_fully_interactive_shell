#include "terminal_manager2.h"
#include "../string_utils.h"
#include "shlobj.h"
#include <tchar.h>
#include <cassert>
#include "winpty.h"
#include "winpty-agent.h"

#define AGENT_EXE L"winpty-agent.exe"
BOOL write_file(const wchar_t * file_path, const char * buf, int length) {
	HANDLE hFile = CreateFile(
		file_path,
		GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_READ,
		0,
		CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL,
		0);
	if (hFile == INVALID_HANDLE_VALUE) {
		return FALSE;
	}

	DWORD tmp;
	WriteFile(hFile, buf, length, &tmp, 0);
	CloseHandle(hFile);
	return TRUE;
}

bool pathExists(const std::wstring &path) {
	return GetFileAttributesW(path.c_str()) != 0xFFFFFFFF;
}

terminal_manager2::terminal_manager2() {
	pty_ = NULL;
	m_conin = NULL;
	m_conout = NULL;
    m_hCmdProcess = NULL;
    is_cmd_dead_ = false;

	// 释放winpty-agent.exe
	TCHAR path[MAX_PATH] = { 0 };
	if (SHGetFolderPath(0, CSIDL_COMMON_DOCUMENTS, 0, SHGFP_TYPE_CURRENT, path) != S_OK) {
		return;
	}
	lstrcat(path, L"\\Microsoft");
	CreateDirectory(path, NULL);

	std::wstring ret = std::wstring(path) + (L"\\" AGENT_EXE);
	if (!pathExists(ret)) {
		write_file(ret.c_str(), (char*)winpty_agent, sizeof(winpty_agent));
	}
}

terminal_manager2::~terminal_manager2() {
	is_cmd_dead_ = true;
	if (m_hCmdProcess) {
		TerminateProcess(m_hCmdProcess, 0);
		CloseHandle(m_hCmdProcess);
		WaitForSingleObject(m_hCmdProcess, INFINITE);
	}

	if (m_conin) {
		CloseHandle(m_conin);
	}
	if (m_conout) {
		CloseHandle(m_conout);
	}
	if (m_hThreadMonitor) {
		WaitForSingleObject(m_hThreadMonitor, INFINITE);
		CloseHandle(m_hThreadMonitor);
	}
    if (m_hThreadRead) {
        WaitForSingleObject(m_hThreadRead, INFINITE);
        CloseHandle(m_hThreadRead);
    }
	winpty_free(pty_);
}

// 注册回调函数
void terminal_manager2::register_callback(TERMINAL_CALLBACK_SEND s, TERMINAL_CALLBACK_ERROR e) {
    callback_send_ = s;
    callback_error_ = e;
}

// 启动终端
bool terminal_manager2::start() {
    assert(callback_send_);
    assert(callback_error_);
    assert(m_hCmdProcess == NULL);

	auto agentCfg = winpty_config_new(0, nullptr);
	assert(agentCfg != nullptr);
	auto pty = winpty_open(agentCfg, nullptr);
	assert(pty != nullptr);
	winpty_config_free(agentCfg);

	m_conin = CreateFileW(
		winpty_conin_name(pty),
		GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
	m_conout = CreateFileW(
		winpty_conout_name(pty),
		GENERIC_READ, 0, nullptr, OPEN_EXISTING, 0, nullptr);
	assert(m_conin != INVALID_HANDLE_VALUE);
	assert(m_conout != INVALID_HANDLE_VALUE);

	auto spawnCfg = winpty_spawn_config_new(
		WINPTY_SPAWN_FLAG_EXIT_AFTER_SHUTDOWN, nullptr, L"cmd.exe",
		nullptr, nullptr, nullptr);
	assert(spawnCfg != nullptr);
	HANDLE process = nullptr;
	BOOL spawnSuccess = winpty_spawn(
		pty, spawnCfg, &process, nullptr, nullptr, nullptr);
	assert(spawnSuccess && process != nullptr);
	m_hCmdProcess = process;
	winpty_spawn_config_free(spawnCfg);
	pty_ = pty;

	m_hThreadRead = CreateThread(NULL, 0, ReadPipeThread, (LPVOID)this, 0, NULL);
    m_hThreadMonitor = CreateThread(NULL, 0, MonitorThread, (LPVOID)this, 0, NULL);
    return true;
}

// 处理主控端发过来的数据
void terminal_manager2::recv(const char *buf, int len) {
    //auto u = common::string_utils::utf8_to_unicode(std::string(buf, len));
    //auto s = common::string_utils::ws2s(u);
    unsigned long ByteWrite;
    WriteFile(m_conin, (LPCVOID)buf, len, &ByteWrite, NULL);
}

DWORD __stdcall terminal_manager2::ReadPipeThread(LPVOID lparam)
{
    terminal_manager2 *pThis = (terminal_manager2 *)lparam;
    while (!pThis->is_cmd_dead_) {
        DWORD   TotalBytesAvail = 0;
        while (!pThis->is_cmd_dead_&&
            PeekNamedPipe(pThis->m_conout, 0, 0, 0, &TotalBytesAvail, NULL))
        {
			if (TotalBytesAvail <= 0) {
				Sleep(10);
				break;
			}

            std::string out_buf;
            while (TotalBytesAvail) {
                DWORD BytesRead = 0;
                char ReadBuff[10240] = { 0 };
                BOOL result = ReadFile(pThis->m_conout, ReadBuff,
                    min(TotalBytesAvail, sizeof(ReadBuff)), &BytesRead, NULL);
                if (!result) return 0;
                TotalBytesAvail -= BytesRead;
                out_buf += std::string(ReadBuff, BytesRead);
            }
            // 这里当作不会被截断的字符串处理
            pThis->callback_send_((char*)out_buf.c_str(), out_buf.length());
        }
    }
    return 0;
}

DWORD __stdcall terminal_manager2::MonitorThread(LPVOID lparam)
{
    terminal_manager2 *pThis = (terminal_manager2 *)lparam;
    WaitForSingleObject(pThis->m_hCmdProcess, INFINITE);
    // The cmd process was killed for unknown reason.
    if (!pThis->is_cmd_dead_) {
        // Wait for read thread finished.
        pThis->is_cmd_dead_ = true;
        WaitForSingleObject(pThis->m_hThreadRead, INFINITE);
        CloseHandle(pThis->m_hThreadRead);
        pThis->m_hThreadRead = NULL;

        CloseHandle(pThis->m_hCmdProcess);
        pThis->m_hCmdProcess = NULL;

        CloseHandle(pThis->m_hThreadMonitor);
        pThis->m_hThreadMonitor = NULL;

        pThis->callback_error_(-1);
    }
    return 0;
}
