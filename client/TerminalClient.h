#pragma once
#include <windows.h>
#include <string>
#include <thread>
#include <functional>
#include "../string_utils.h"
#pragma comment(lib, "rpcrt4.lib")


// 全交互式CMD主控端封装
class TerminalClient {
public:
	typedef std::function<void(void* buf, int len)> func_callback;
	typedef std::function<void()> func_failed;

	TerminalClient() {
		hCmdProcess_ = NULL;
		hPipeInCMD_ = NULL;
		hPipeOutCMD_ = NULL;
		hJob_ = CreateJobObject(NULL, NULL);
		if (hJob_) {
			JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli = { 0 };
			jeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
			SetInformationJobObject(hJob_, JobObjectExtendedLimitInformation, &jeli, sizeof(jeli));
		}
	}
	~TerminalClient() {
		if (hCmdProcess_) {
			HANDLE hTmp = hCmdProcess_;
			hCmdProcess_ = NULL;
			TerminateProcess(hTmp, -1);
			CloseHandle(hTmp);
			Sleep(100);
		}
		if (hPipeInCMD_) {
			CloseHandle(hPipeInCMD_);
		}
		if (hPipeOutCMD_) {
			CloseHandle(hPipeOutCMD_);
		}
		if (hJob_)
			CloseHandle(hJob_);
	}

	// CmdEmulatorFullPath表示cmd模拟器全路径
	// cb 表示cmd模拟器输入数据发送回调
	bool init(const std::string& CmdEmulatorFullPath, const std::string& title, func_callback cb, func_failed fail) {
		if (!cb || CmdEmulatorFullPath.empty()) {
			return false;
		}
		cb_ = cb;

		// 1. 创建管道
		std::string randPipeName = GetGuidString();
		std::string pipe = std::string("\\\\.\\pipe\\") + randPipeName + "inCMD";
		HANDLE hPipe = CreateNamedPipeA(
			pipe.c_str(),             // pipe name   
			PIPE_ACCESS_DUPLEX,       // read/write access   
			PIPE_TYPE_MESSAGE |       // message type pipe   
			PIPE_READMODE_MESSAGE |   // message-read mode   
			PIPE_WAIT,                // blocking mode   
			PIPE_UNLIMITED_INSTANCES, // max. instances    
			4096,                  // output buffer size   
			4096,                  // input buffer size   
			0,                        // client time-out   
			NULL);                    // default security attribute   
		if (hPipe == INVALID_HANDLE_VALUE) {
			return false;
		}
		hPipeInCMD_ = hPipe;

		pipe = std::string("\\\\.\\pipe\\") + randPipeName + "outCMD";
		hPipe = CreateNamedPipeA(
			pipe.c_str(),             // pipe name   
			PIPE_ACCESS_DUPLEX,       // read/write access   
			PIPE_TYPE_MESSAGE |       // message type pipe   
			PIPE_READMODE_MESSAGE |   // message-read mode   
			PIPE_WAIT,                // blocking mode   
			PIPE_UNLIMITED_INSTANCES, // max. instances    
			4096,                  // output buffer size   
			4096,                  // input buffer size   
			0,                        // client time-out   
			NULL);                    // default security attribute   
		if (hPipe == INVALID_HANDLE_VALUE) {
			CloseHandle(hPipeInCMD_);
			hPipeInCMD_ = NULL;
			return false;
		}
		hPipeOutCMD_ = hPipe;

		// 2.创建进程
		std::string commandLine = CmdEmulatorFullPath;
		commandLine += " ";
		commandLine += randPipeName;
		commandLine += " \"";
		commandLine += title;
		commandLine += "\"";
		PROCESS_INFORMATION pi = { 0 };
		STARTUPINFOA si = { sizeof(si) };
		if (CreateProcessA(
			NULL,
			(LPSTR)commandLine.c_str(), //在Unicode版本中此参数不能为常量字符串，因为此参数会被修改    
			NULL,
			NULL,
			FALSE,
			CREATE_NEW_CONSOLE,
			NULL,
			NULL,
			&si,
			&pi))
		{
			if (hJob_) {
				AssignProcessToJobObject(hJob_, pi.hProcess);
			}
			CloseHandle(pi.hThread);
			//CloseHandle(pi.hProcess);
			hCmdProcess_ = pi.hProcess;
			std::thread t([](func_failed fail, HANDLE*h) {
				WaitForSingleObject(*h, INFINITE);
				if (*h == NULL)
					return;
				fail();
			}, fail, &hCmdProcess_);
			t.detach();
		}
		else {
			CloseHandle(hPipeInCMD_);
			hPipeInCMD_ = NULL;
			CloseHandle(hPipeOutCMD_);
			hPipeOutCMD_ = NULL;
			return false;
		}

		BOOL fConnected = ConnectNamedPipe(hPipeInCMD_, NULL) ? TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);
		if (!fConnected) {
			CloseHandle(hPipeInCMD_);
			hPipeInCMD_ = NULL;
			CloseHandle(hPipeOutCMD_);
			hPipeOutCMD_ = NULL;
			return false;
		}

		fConnected = ConnectNamedPipe(hPipeOutCMD_, NULL) ? TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);
		if (!fConnected) {
			CloseHandle(hPipeInCMD_);
			hPipeInCMD_ = NULL;
			CloseHandle(hPipeOutCMD_);
			hPipeOutCMD_ = NULL;
			return false;
		}

		// 3. 创建读管道线程
		HANDLE hTmp = hPipeOutCMD_;
		std::thread th([hTmp](func_callback cb) {
			while (true) {
				char szReadBuffer[1024] = { 0 };
				DWORD cbBytesRead;
				BOOL fSuccess = ReadFile(
					hTmp,			 // handle to pipe 
					szReadBuffer,    // buffer to receive data 
					1024,			 // size of buffer 
					&cbBytesRead,	 // number of bytes read 
					NULL);			 // not overlapped I/O 
				if (!fSuccess && GetLastError() == 0x218) {
					Sleep(1000);
					continue;
				}
				if (!fSuccess || cbBytesRead == 0) {
					DisconnectNamedPipe(hTmp);
					break;
				}
				cb(szReadBuffer, cbBytesRead);
			}
		}, cb);
		th.detach();
		return true;
	}

	// 处理远端发过来的数据
	void recv(const char *buf, int len) {
		auto u = common::string_utils::utf8_to_unicode(std::string(buf, len));
		auto x = common::string_utils::unicode_to_mutibyte(u, GetOEMCP());
		DWORD cbWritten = 0;
		BOOL fSuccess = WriteFile(
			hPipeInCMD_,       // pipe handle   
			x.c_str(),             // message   
			x.length(),      // message length   
			&cbWritten,        // bytes written   
			NULL);             // not overlapped   
		if (!fSuccess || cbWritten != cbWritten) {
			// If failed, what should I do???
		}
	}
private:
	std::string GetGuidString() {
		UUID uuid = { 0 };
		std::string guid;

		// Create uuid or load from a string by UuidFromString() function
		::UuidCreate(&uuid);

		// If you want to convert uuid to string, use UuidToString() function
		RPC_CSTR szUuid = NULL;
		if (::UuidToStringA(&uuid, &szUuid) == RPC_S_OK)
		{
			guid = (char*)szUuid;
			::RpcStringFreeA(&szUuid);
		}
		return guid;
	}

	HANDLE hJob_;
	HANDLE hPipeInCMD_;
	HANDLE hPipeOutCMD_;
	HANDLE hCmdProcess_;
	func_callback cb_;
};
