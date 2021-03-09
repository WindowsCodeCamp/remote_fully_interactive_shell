#include <winsock2.h>
#include "terminal_manager2.h"
#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>
#include <list>
#include <boost/asio.hpp>

using boost::asio::ip::tcp;
class session
	: public std::enable_shared_from_this<session>
{
public:
	session(tcp::socket socket)
		: socket_(std::move(socket))
	{
		tm_ = new terminal_manager2();
		tm_->register_callback(std::bind(
			&session::do_write, this, std::placeholders::_1, std::placeholders::_2),
			std::bind(&session::some_error, this, std::placeholders::_1));
		tm_->start();
	}

	~session() {
		printf("~~~~~ ~session()....\n");
	}

	void start()
	{
		do_read();
	}

private:
	void some_error(int) {
		delete tm_;
		tm_ = nullptr;
	};

	void do_read()
	{
		auto self(shared_from_this());
		ZeroMemory(data_, max_length);
		socket_.async_read_some(boost::asio::buffer(data_, max_length),
			[this, self](boost::system::error_code ec, std::size_t length)
		{
			if (ec) {
				some_error(0);
				return;
			}

			printf(data_);
			if (tm_) {
				tm_->recv(data_, length);
				do_read();
			}
		});
	}

	// 就本例而言，同一个socket读写不在一个线程，这样设计可能会导致资源（线程句柄）泄漏
	// WSASend api会持有当前线程句柄，线程正常结束没问题，暴力强杀会句柄泄露。
	void do_write(char* buf, int len)
	{
		auto self(shared_from_this());
		boost::asio::async_write(socket_, boost::asio::buffer(buf, len),
			[this, self](boost::system::error_code ec, std::size_t length)
		{
			if (ec) {
				some_error(0);
			}
		});
	}

	tcp::socket socket_;
	enum { max_length = 1024 };
	char data_[max_length];
	terminal_manager2 *tm_;
};

class server
{
public:
	server(boost::asio::io_context& io_context, short port)
		: acceptor_(io_context, tcp::endpoint(tcp::v4(), port))
	{
		do_accept();
	}

private:
	void do_accept()
	{
		acceptor_.async_accept(
			[this](boost::system::error_code ec, tcp::socket socket)
		{
			if (!ec)
			{
				auto s = std::make_shared<session>(std::move(socket));
				s->start();
			}

			do_accept();
		});
	}

	tcp::acceptor acceptor_;
};

int main(int argc, char* argv[])
{
	unsigned short port = 5000;
	std::cout << "listening port:" << port << std::endl;
	try
	{
		boost::asio::io_context io_context;
		server s(io_context, port);
		io_context.run();
	}
	catch (std::exception& e)
	{
		std::cerr << "Exception: " << e.what() << "\n";
	}

	return 0;
}