// CmdEmulator.cpp : Defines the entry point for the console application.
//
#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <conio.h>
#include <windows.h>

#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif

int unicode_to_utf8(char* out, wchar_t ch, int cp = 65001) {
	return WideCharToMultiByte(cp, 0, &ch, 1, out, 8, NULL, NULL);
}

DWORD WINAPI ThreadRecv(LPVOID lpThreadParam) {
	HANDLE hPipe = (HANDLE)lpThreadParam;
	while (1) {
		char szReadBuffer[1024 + 1] = { 0 }; // 必须得加1，否则可能会将后面的乱码打出来
		DWORD cbBytesRead;
		BOOL fSuccess = ReadFile(
			hPipe,			// handle to pipe 
			szReadBuffer,   // buffer to receive data 
			1024,			// size of buffer 
			&cbBytesRead,	// number of bytes read 
			NULL);			// not overlapped I/O 

		if (!fSuccess || cbBytesRead == 0) {
			DisconnectNamedPipe(hPipe);
			CloseHandle(hPipe);
			break;
		}
		printf(szReadBuffer);
	}
	return 0;
}

BOOL PipeWrite(HANDLE hPipe, const char* buf, DWORD len) {
	DWORD cbWritten = 0;
	BOOL fSuccess = WriteFile(
		hPipe,       // pipe handle   
		buf,             // message   
		len,      // message length   
		&cbWritten,        // bytes written   
		NULL);             // not overlapped   
	if (!fSuccess || cbWritten != len) {
		return FALSE;
		// If failed, what should I do???
	}
	return TRUE;
}

int main(int argc, char* argv[])
{
	if (argc != 3) {
		printf("Param error!\r\n");
		return 0;
	}
	printf("Initializing...\r\n");
	SetConsoleTitle(argv[2]);

	// 1. 设置输出模式
	HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
	DWORD dwMode = 0;
	GetConsoleMode(hOut, &dwMode);
	dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
	SetConsoleMode(hOut, dwMode);

	// 2.连接管道
	char szPipeName[MAX_PATH] = { "\\\\.\\pipe\\" };
	strcat(szPipeName, argv[1]);

	char szPipeInCmd[MAX_PATH] = { 0 };
	strcpy(szPipeInCmd, szPipeName);
	strcat(szPipeInCmd, "inCMD");
	char szPipeOutCmd[MAX_PATH] = { 0 };
	strcpy(szPipeOutCmd, szPipeName);
	strcat(szPipeOutCmd, "outCMD");
	HANDLE hPipeInCmd = CreateFileA(
		szPipeInCmd,   // pipe name   
		GENERIC_READ |  // read and write access   
		GENERIC_WRITE,
		0,              // no sharing   
		NULL,           // default security attributes  
		OPEN_EXISTING,  // opens existing pipe   
		0,              // default attributes   
		NULL);          // no template file   

	if (hPipeInCmd == INVALID_HANDLE_VALUE) {
		printf("Create Pipe Failed!\r\n");
		return 0;
	}

	HANDLE hPipeOutCmd = CreateFileA(
		szPipeOutCmd,   // pipe name   
		GENERIC_READ |  // read and write access   
		GENERIC_WRITE,
		0,              // no sharing   
		NULL,           // default security attributes  
		OPEN_EXISTING,  // opens existing pipe   
		0,              // default attributes   
		NULL);          // no template file   

	if (hPipeOutCmd == INVALID_HANDLE_VALUE) {
		printf("Create Pipe Failed!\r\n");
		return 0;
	}

	// 3. 收发数据
	system("cls");
	DWORD dwThreadId;
	HANDLE hRecvThread = CreateThread(
		NULL,                   // default security attributes
		0,                      // use default stack size  
		ThreadRecv,				// thread function name
		hPipeInCmd,		            // argument to thread function 
		0,                      // use default creation flags 
		&dwThreadId);			// returns the thread identifier 
	if (hRecvThread == NULL) {
		printf("Create Thread Failed!\r\n");
		return 0;
	}

	while (true) {
		wchar_t ch = (wchar_t)_getwch();
		if (ch == (wchar_t)0xe0) {
			ch = (wchar_t)_getwch();
			if (ch == (wchar_t)0x48) {
				PipeWrite(hPipeOutCmd, "\x1B[A", 3);
				continue;
			}
			if (ch == (wchar_t)0x50) {
				PipeWrite(hPipeOutCmd, "\x1B[B", 3);
				continue;
			}
			if (ch == (wchar_t)0x4B) {
				PipeWrite(hPipeOutCmd, "\x1B[D", 3);
				continue;
			}
			if (ch == (wchar_t)0x4D) {
				PipeWrite(hPipeOutCmd, "\x1B[C", 3);
				continue;
			}
		}
		char out[8] = { 0 };
		int len = unicode_to_utf8(out, ch);
		PipeWrite(hPipeOutCmd, out, len);
	}

	CloseHandle(hPipeInCmd);
	CloseHandle(hPipeOutCmd);
	CloseHandle(hRecvThread);
	return 0;
}
