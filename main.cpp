#define _CRT_SECURE_NO_WARNINGS

#include <WinSock2.h>

#include <iostream>

#include "client.hpp"
#include "server.hpp"
#include "state.hpp"

#pragma comment(lib, "Ws2_32.lib")

auto main(int argc, char** argv) -> int {
	std::srand(time(nullptr));
	system("cls");
	state::playaHaters.reserve(10);

	auto data = WSADATA{};
	if (WSAStartup(MAKEWORD(2, 2), &data)) {
		std::cerr << "WSAStartup failed";
		return -1;
	}

	if (LOBYTE(data.wVersion) != 2 || HIBYTE(data.wVersion) != 2) {
		std::cerr << "Version 2.2 of WinSock is unavailable";
		WSACleanup();
		return -1;
	}

	auto sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	setsockopt(sock, SOL_SOCKET, SO_BROADCAST, "1", 1);

	auto broadcastAddr = sockaddr_in{
		.sin_family = AF_INET,
		.sin_port = htons(state::udpPort),
		.sin_addr = {.S_un = {.S_addr = INADDR_BROADCAST}},
	};

	int result = 0;
	std::cout << "scanning for a server for 5 secs...\n";
	auto pollFd = pollfd{.fd = sock, .events = POLLIN};

server:
	for (int i = 0; result == 0 && i < 25; ++i) {
		if (sendto(sock, "?", 1, 0, (sockaddr*) &broadcastAddr, sizeof(broadcastAddr)) == -1) {
			std::cerr << "udp socket dispatch failed";
			return -1;
		}
		result = WSAPoll(&pollFd, 1, 5000 / 25);
	}
	if (result == 0) {
		std::cout << "no server responded. becoming one\n";

		closesocket(sock);
		if (server() == -1) {
			std::cerr << "\nserver failed";
		}
	} else if (result > 0 && pollFd.revents & POLLIN) {
		auto yes = std::array<char, 3>();
		recv(sock, yes.data(), yes.size(), 0);
		if (strncmp(yes.data(), "Yes", 3)) goto server;
		std::cout << "found some enemies\n";

		auto port = short();
		auto addrfrom = sockaddr_in{};
		auto size = (int) sizeof(addrfrom);
		recvfrom(sock, (char*) &port, 2, 0, (sockaddr*) &addrfrom, &size);
		addrfrom.sin_port = port;

		closesocket(sock);
		if (client(addrfrom) == -1) {
			std::cerr << "\nclient failed";
		}
	} else {
		std::cerr << "poll failed " << WSAGetLastError();
	}

	WSACleanup();
}
