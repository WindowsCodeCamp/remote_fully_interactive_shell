#pragma once
#include <functional>
#include <windows.h>
#include <winpty.h>

typedef std::function<void(char* buf, int len)> TERMINAL_CALLBACK_SEND;
typedef std::function<void(int len)> TERMINAL_CALLBACK_ERROR;

class terminal_manager2 {
public:
    terminal_manager2();
    ~terminal_manager2();
    terminal_manager2(const terminal_manager2&) = delete;
    terminal_manager2(terminal_manager2&&) = delete;
    terminal_manager2& operator=(const terminal_manager2&) = delete;
    terminal_manager2& operator=(terminal_manager2&&) = delete;

    // ע��ص�������ע��ص����������߳�ִ�У������Ҫ�����д����̵߳��ȣ�
    void register_callback(TERMINAL_CALLBACK_SEND, TERMINAL_CALLBACK_ERROR);
    
    // �����ն�
    bool start();
    
    // �������ض˷�����������
    void recv(const char *buf, int len);

private:
    TERMINAL_CALLBACK_SEND callback_send_;
    TERMINAL_CALLBACK_ERROR callback_error_;

	winpty_t* pty_;
	HANDLE m_conin;
	HANDLE m_conout;

    HANDLE m_hCmdProcess;
    HANDLE m_hThreadMonitor;
    HANDLE m_hThreadRead;

    static DWORD __stdcall MonitorThread(LPVOID lparam);
    static DWORD __stdcall ReadPipeThread(LPVOID lparam);
    bool is_cmd_dead_;
};
