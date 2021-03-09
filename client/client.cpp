#include <cstdlib>
#include <string>
#include <iostream>
#include <boost/asio.hpp>
#include <locale.h>
#include <conio.h>
#include <functional>
#include "TerminalClient.h"

#define CMD_EMULATOR_EXE "CmdEmulator.exe"

using boost::asio::ip::tcp;
enum { max_length = 1024 };


int main(int argc, char* argv[])
{
	char reply[max_length] = { 0 };
	try
	{
		boost::asio::io_context io_context;
		tcp::socket socket_(io_context);
		tcp::resolver resolver(io_context);

		TerminalClient tc;
		tc.init(CMD_EMULATOR_EXE, "console default title",
			[&](void* buf, int len) {boost::asio::write(socket_, boost::asio::buffer(buf, len)); },
			[] {printf("failed.\r\n"); });

		std::function<void(void)> read_some_data = [&]() {
			ZeroMemory(reply, max_length);
			socket_.async_read_some(boost::asio::buffer(reply, max_length), [&](const boost::system::error_code ec, std::size_t length) {
				if (!ec) {
					tc.recv(reply, length);
					read_some_data();
				}
			});
		};

		boost::asio::connect(socket_, resolver.resolve("127.0.0.1", "5000"));
		read_some_data();

		std::thread t([&io_context]() { io_context.run(); });
		t.join();
	}
	catch (std::exception& e)
	{
		std::cerr << "Exception: " << e.what() << "\n";
	}

	return 0;
}
