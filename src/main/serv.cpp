#include <iostream>
#include <fstream>
#include <cstring>
#include <cstdlib>
#include <csignal>
#include <string_view>
#include <type_traits>
#include <algorithm>
#include <deque>
#include <climits>
#include "netfiletransferer.h"
//#include <windows.networking.sockets.h>
//#pragma comment(lib, "Ws2_32.lib")
#define _CRT_SECURE_NO_WARNINGS

#define NO_FILES 3
#define IMPROPER_ARGS 2
#define LINE_MAX_CHAR 250

void inputValid(std::string prompt)
{
	std::cout << "Need to provide valid input.\n";
}

/*Enforces only valid answers as input. Varargs can only be strings.*/
template<typename... Targs>
std::enable_if_t<std::conjunction_v<std::is_constructible<std::string, Targs>...>, std::string>
inputValid(std::string prompt, Targs... args)
{
	std::string valid_list[sizeof...(args) + 1] = { std::string(args)..., "" };
	std::string ans;
	bool repeat = true;
	
	std::cout << prompt;
	while (repeat && std::getline(std::cin, ans)) {

		for (std::string item : valid_list) {
			if (item == ans && item != "") {
				repeat = false;
				break;
			}
		}

		if (repeat)
			std::cout << prompt;
		
	}

	return ans;
}


int inputRange(std::string prompt, int min, int max) 
{
	if (min > max) {
		std::cout << "The minimum value pass to the inputRange function was \
						larger than the maximum value.\n";
		raise(SIGTERM);
	}

	int ans = INT_MIN;

	while (true) {
		std::cout << prompt;
		if (!(std::cin >> ans)) {
			std::cin.clear();
		}
		std::cin.ignore(1000, '\n');

		if (ans >= min && ans <= max)
			break;

	}

	return ans;
}


std::deque<const char*> readFileList(std::string filename)
{
	std::ifstream listfile(filename);
	std::string line;
	std::deque<const char*> lines;

	while (std::getline(listfile, line) && line.size() < LINE_MAX_CHAR) {
		std::cout << line.c_str() << " to be transferred.\n";
		char* copy = new char[line.size() + 1];
		std::strcpy(copy, line.c_str());
		lines.push_back(copy);
	}

	return lines;
}


void destConnectionLoop(NetFileTransferrer* nft)
{
	while (nft->connect()) {
		std::cout << "\nConnected\n";
		while (nft->check_connection()) {
			if (nft->info_exchange()) {
				uint32_t amount = nft->receive();
				std::cout << amount << " bytes received.\n";
				if (nft->save())
					std::cout << "File " << nft->get_file() << " written.\n";
				nft->reset();
			} else {
				std::cout << "Could not exchange information.\n";
			}
		} //File loop
		std::cout << "Connection terminated by client.\n";
	} //Connect loop
	std::cout << "No more connections incoming, ending program\n";
}


int manualOp()
{
	std::string res;
	NetFileTransferrer* nft = nullptr;

	std::string ans = inputValid("Will this be the source or the destination? (s or d): ", "s", "d");

	if (ans == "s") {
		
		std::string addr;
		nft = new NetFileTransferrer(true);
		
		std::cout << "Enter server address to send to: ";
		std::getline(std::cin, addr);
		int dest_port = inputRange("Enter the server port number to send to (1029 to 49150): ", 1029, 49150);
		nft->set_destination(addr, dest_port);

		if (nft->connect() && nft->check_connection()) {
			std::cout << "Enter file to send: ";
			std::getline(std::cin, ans);
			std::cout << ans << std::endl;
			nft->set_file(ans);

			if (nft->info_exchange()) {
				uint32_t amount = nft->send();
				std::cout << amount << " bytes sent.\n";
			} else {
				std::cout << "Could not exhange information.\n";
			}
		}
		delete nft;

	} else if (ans == "d") {
		
		nft = new NetFileTransferrer(false);
		nft->set_port(inputRange("Enter the server port number to listen on (1029 to 49150): ", 1029, 49150));
		destConnectionLoop(nft);
		
		delete nft;

	} else {
		std::cout << "No choice was given.\n";
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}


int autoOp(bool isSrc, std::deque<const char*>& files, int port, std::string addr = "")
{
	bool isRandPort = false;
	if (port == 0 && isSrc) {
		std::cout << "Improper port number given. Exiting.\n";
		return IMPROPER_ARGS;
	} else if (port == 0 && !isSrc) {
		isRandPort = true;
	}

	NetFileTransferrer* nft = new NetFileTransferrer(isSrc);

	if (isSrc) {
		std::cout << "Sending to " << addr << " on port " << port << ".\n";
		nft->set_destination(addr, port);

		if (nft->connect()) {
			while (files.size() > 0 && nft->check_connection()) {
				nft->set_file(files.front());
				std::cout << "Sending " << files.front() << std::endl;
				files.pop_front();

				if (nft->info_exchange()) {
					size_t amount = nft->send();
					std::cout << amount << " bytes sent.\n";
				} else {
					std::cout << "Could not exhange information.\n";
				}
				nft->reset();
			}//while
		}
	} else {
		nft->set_port(port);
		destConnectionLoop(nft);
	}

	delete nft;
	return EXIT_SUCCESS;
}


void stopHandle(int signum)
{
	if (signum == SIGABRT)
		std::cout << "Program aborted.\n";
	else if (signum == SIGINT)
		std::cout << "Program interrupted.\n";
	else if (signum == SIGTERM)
		std::cout << "Program terminated.\n";

	WSACleanup();
	exit(signum);
}


int testFunc()
{
	std::string ans = inputValid("Will this be the source or the destination? (s or d): ", "s", "d");

	if (ans == "s") {
		TCP_Client cl;
		cl.change_conn("127.0.0.1", 5000);
		cl.connect_to();
		std::string smsg(9000, '*');
		cl.send_msg(smsg);
	} else if (ans == "d") {
		TCP_Server sv;
		sv.change_port(5000);
		sv.bind_socket();
		sv.await_conn();
		sv.get_conn();
		std::vector<char> msg, tmsg;
		int cnt = 0, sum = 0;
		do {
			cnt = sv.receive_msg(tmsg);
			msg.insert(msg.end(), tmsg.begin(), tmsg.end());
			if (cnt > 0)
				sum += cnt;
		} while (cnt > 0);
		std::cout << sum << " : " << msg.size() << std::endl;
	}

	return 0;
}


int main(int argc, const char* argv[])
{
	int exit_code = 0;
	signal(SIGABRT, stopHandle);
	signal(SIGINT, stopHandle);
	signal(SIGTERM, stopHandle);

	std::cout << "*****************************\n";
	std::cout << "** File Transferrer Program **\n";
	std::cout << "*****************************\n\n";
	std::vector<const char*> args(argv, argv + argc);
	std::deque<const char*> files;

	if (argc > 1 && std::strcmp(args[1], "help") == 0) {
		std::cout << "Usage: " << args[0] << " [-s]|[-d] [Port] [Address] [Files...] [-f filelist_file]\n";
		std::cout << "Use -s or s to set as a file source. "
			<< "Use -d or d to set as a file destination. "
			<< "Can only set source or destination exclusvely. "
			<< "If using arguments, port number and "
			<< "destination address (for source option) "
			<< "must be provided. Use -f to specify a text file "
			<< "containing a list of files to transfer. Only "
			<< "to be used when running as a file source. "
			<< "Running with no args will "
			<< "require manual input of information.\n";
	}

	//Options tests
	auto src_opt = [](const char* arg) -> bool {
		return std::strcmp(arg, "-s") == 0 || std::strcmp(arg, "s") == 0;
	};
	auto dest_opt = [](const char* arg) -> bool {
		return std::strcmp(arg, "-d") == 0 || std::strcmp(arg, "d") == 0;
	};

	bool isSrc = std::any_of(args.cbegin(), args.cend(), src_opt);
	bool isDst = std::any_of(args.cbegin(), args.cend(), dest_opt);


	if ((argc < 2 || argc == 2 && isSrc) || (isSrc && isDst) || (!isSrc && !isDst)) {

		if ((isSrc && isDst || !isSrc && !isDst || argc == 2 && isSrc) && argc != 1) {
			std::cout << "Improper arguments. Running manually.\n";
		}
		
		exit_code = manualOp();

	} else {

		if (isSrc) {
			if (argc > 5 && std::strcmp(args[4], "-f") == 0)
				files = readFileList(args[5]);
			else
				files.assign(args.cbegin() + 4, args.cend());

			if (files.size() > 0)
				exit_code = autoOp(isSrc, files, std::atoi(args[2]), args[3]);
			else 
				exit_code = NO_FILES;
		} else {
			int port_no = (argc == 2) ? 0 : std::atoi(args[2]);
			exit_code = autoOp(isSrc, files, port_no);
		}
	}

	if (exit_code == NO_FILES)
		std::cout << "No file list provided.\n";

	if (exit_code == 0)
		WSACleanup();

	return exit_code;
}