#define _CRT_SECURE_NO_WARNINGS

#include "client.hpp"

#include <WS2tcpip.h>
#include <Winuser.h>

#include <iostream>
#include <map>
#include <thread>

#include "state.hpp"

#pragma comment(lib, "user32.lib")

static bool drawPlayerboard = false;
static bool drawCmds = false;
static char selfSymbol = '\0';

static auto cmdHistory = std::map<std::string, int>();

auto drawMap() -> void {
	system("cls");

	if (drawPlayerboard) {
		std::cout << '\n';
		for (const auto& playa : state::playaHaters) {
			auto nameBuf = std::string(state::MAX_NAME_LEN, ' ');
			for (int i = 0; i < playa.namelen; ++i) {
				nameBuf[i] = playa.name[i];
			}
			std::cout << ' ' << playa.symbol << ' ' << nameBuf << ' ' << playa.diamonds << '\n';
		}
	} else if (drawCmds) {
		std::cout << '\n';
		for (const auto& [cmd, count] : cmdHistory) {
			std::cout << ' ' << cmd << ": " << count << '\n';
		}
	} else {
		std::cout << "+1234567890123456789012345678901234567890\n";
		for (int i = 0; i < state::map.size(); ++i) {
			std::cout << (i + 1) % 10 << state::map[i] << '\n';
		}
	}
}

auto keyJob(HANDLE h, SOCKET s) -> void {
	auto numRead = DWORD();
	for (auto ir = INPUT_RECORD{};;) {
		ReadConsoleInput(h, &ir, 1, &numRead);

		if (ir.EventType != KEY_EVENT || !ir.Event.KeyEvent.bKeyDown) continue;

		// TODO: implement chat

		switch (ir.Event.KeyEvent.wVirtualKeyCode) {
		case VK_LEFT:
			if (!drawPlayerboard) {
				send(s, "movel", 5, 0);
				cmdHistory["movel"]++;
			}
			break;
		case VK_RIGHT:
			if (!drawPlayerboard) {
				send(s, "mover", 5, 0);
				cmdHistory["mover"]++;
			}
			break;
		case VK_UP:
			if (!drawPlayerboard) {
				send(s, "moveu", 5, 0);
				cmdHistory["moveu"]++;
			}
			break;
		case VK_DOWN:
			if (!drawPlayerboard) {
				send(s, "moved", 5, 0);
				cmdHistory["moved"]++;
			}
			break;
		case VK_HOME:
			send(s, "beaml", 5, 0);
			cmdHistory["beaml"]++;
			break;
		case VK_END:
			send(s, "beamr", 5, 0);
			cmdHistory["beamr"]++;
			break;
		case VK_PRIOR:	// page up
			send(s, "beamu", 5, 0);
			cmdHistory["beamu"]++;
			break;
		case VK_NEXT:  // page down
			send(s, "beamd", 5, 0);
			cmdHistory["beamd"]++;
			break;
		case 'B':
			send(s, "bomb", 4, 0);
			cmdHistory["bomb"]++;
			break;
		case VK_TAB:
			drawPlayerboard = !drawPlayerboard;
			drawCmds = false;
			drawMap();
			break;
		case VK_SPACE:
			drawCmds = !drawCmds;
			drawPlayerboard = false;
			drawMap();
			break;
		}
	}
}

auto loop(SOCKET sock) -> int {
	for (;;) {
		auto buf = receive(sock);

		if (!buf.size()) {
			std::cerr << "server died\n";
			return -1;
		}

		bool updateMap = true;
		if (cmd("move")) {
			auto direction = buf[4];
			auto symbol = buf[5];
			auto newDiamondsCount = ntohs(*(short*) &buf[6]);
			auto playa = std::find_if(state::playaHaters.begin(), state::playaHaters.end(), [symbol](const state::playa& playa) {
				return playa.symbol == symbol;
			});
			if (playa == state::playaHaters.end()) {
				std::cerr << "warn: sync issue: playa '" << symbol << "' not present on client\n";
				continue;
			}
			state::map[playa->y][playa->x] = '.';
			switch (direction) {
			case 'u':
				playa->y--;
				break;
			case 'd':
				playa->y++;
				break;
			case 'r':
				playa->x++;
				break;
			case 'l':
				playa->x--;
				break;
			}
			state::map[playa->y][playa->x] = playa->symbol;
			updateMap = !drawPlayerboard || playa->diamonds != newDiamondsCount;
			playa->diamonds = newDiamondsCount;
		} else if (cmd("newp")) {
			auto& newPlaya = state::playaHaters.emplace_back(&buf[4]);
			state::map[newPlaya.y][newPlaya.x] = newPlaya.symbol;
		} else if (cmd("chat")) {
			const auto& playa = std::find_if(state::playaHaters.begin(), state::playaHaters.end(), [symbol = buf[4]](const state::playa& playa) {
				return playa.symbol == symbol;
			});

			if (playa == state::playaHaters.end()) {
				std::cerr << "warn: sync issue: playa '" << buf[4] << "' sent a message but not present locally";
				continue;
			}

			auto message = std::string(playa->name, playa->namelen);
			message += ": ";
			message.append_range(buf.subspan(5));
			state::messages.emplace_back(message);
		} else if (cmd("bomb")) {
			auto symbol = buf[4];

			auto playa = std::find_if(state::playaHaters.begin(), state::playaHaters.end(), [symbol](const state::playa& playa) {
				return playa.symbol == symbol;
			});

			if (playa == state::playaHaters.end()) {
				std::cerr << "warn: sync issue: no killed playa '" << symbol << "' found\n";
				continue;
			}

			playa->diamonds--;

			auto oldMap = state::map;

			for (auto x = playa->x - 2; x <= playa->x + 2; ++x) {
				for (auto y = playa->y - 2; y <= playa->y + 2; ++y) {
					if ((playa->y == y && playa->x == x) || y < 0 || y > 19) continue;

					oldMap[y][x] = '.';
					state::map[y][x] = '@';
				}
			}

			drawMap();
			state::map = oldMap;

			updateMap = false;
		} else if (cmd("beam")) {
			auto symbol = buf[4];
			auto direction = buf[5];
			auto endX = buf[6];
			auto endY = buf[7];

			auto playa = std::find_if(state::playaHaters.begin(), state::playaHaters.end(), [symbol](const state::playa& playa) {
				return playa.symbol == symbol;
			});

			if (playa == state::playaHaters.end()) {
				std::cerr << "warn: sync issue: no killed playa '" << symbol << "' found\n";
				continue;
			}

			playa->diamonds--;

			auto oldMap = state::map;

			char x = playa->x, y = playa->y;
			switch (direction) {
			case 'r':
				x++;
				for (x; x < endX; ++x) {
					state::map[y][x] = '=';
				}
				break;
			case 'l':
				x--;
				for (x; x > endX; --x) {
					state::map[y][x] = '=';
				}
				break;
			case 'd':
				y++;
				for (y; y < endY; ++y) {
					state::map[y][x] = '|';
				}
				break;
			case 'u':
				y--;
				for (y; y > endY; --y) {
					state::map[y][x] = '|';
				}
				break;
			}

			drawMap();

			state::map = oldMap;

			updateMap = false;
		} else if (cmd("kill")) {
			if (char symbol; (symbol = buf[4]) == selfSymbol) {
				std::cout << "you died!\n";
				return 0;
			} else {
				auto playa = std::find_if(state::playaHaters.begin(), state::playaHaters.end(), [symbol = buf[4]](const state::playa& playa) {
					return playa.symbol == symbol;
				});

				if (playa == state::playaHaters.end()) {
					std::cerr << "warn: sync issue: no killed playa '" << symbol << "' found\n";
					continue;
				}

				state::map[playa->y][playa->x] = '.';
				state::playaHaters.erase(playa);
			}
		} else {
			std::cerr << "unknown server cmd " << buf.data() << '\n';
		}
		if (updateMap) drawMap();
	}

	return 0;
}

auto client(sockaddr_in serverAddr) -> int {
	auto inetBuf = std::array<char, INET6_ADDRSTRLEN>();
	inet_ntop(serverAddr.sin_family, &serverAddr.sin_addr, inetBuf.data(), inetBuf.size());
	std::cout << "connecting to server on port " << inetBuf.data() << ':' << htons(serverAddr.sin_port) << '\n';

	auto sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	if (sock == -1) {
		std::cerr << "socket creation failed " << WSAGetLastError();
		return -1;
	}

	// "ping" + symbol + len + name
	auto ping = std::array<char, 4 + 2 + state::MAX_NAME_LEN>{
		'p',
		'i',
		'n',
		'g',
	};

	std::cout << "Enter name: ";
	std::cin.getline(&ping[6], state::MAX_NAME_LEN);

	std::cout << "Enter symbol: ";
	std::cin >> ping[4];
	selfSymbol = ping[4];

	ping[5] = strnlen(&ping[6], state::MAX_NAME_LEN);

	if (connect(sock, (sockaddr*) &serverAddr, sizeof(serverAddr)) == -1) {
		closesocket(sock);
		std::cerr << "connect failed";
		return -1;
	}

	send(sock, ping.data(), 4 + 2 + ping[5], 0);

	auto buf = receive(sock);
	if (!buf.size()) {
		std::cerr << "ping failed";
		return -1;
	}

	for (int i = 0; buf[i] != '\0'; ++i) {
		switch (buf[i]) {
		case 'o': {
			auto mapObject = state::map_object::unpack(&buf[i + 1]);
			state::map[mapObject.y][mapObject.x] = mapObject.symbol;
			break;
		}
		case 'p': {
			auto& playa = state::playaHaters.emplace_back(&buf[i + 1]);
			state::map[playa.y][playa.x] = playa.symbol;
			break;
		}
		}
		for (i; buf[i] != '\n'; ++i);
	}

	drawMap();

	auto kjTh = std::thread(keyJob, GetStdHandle(STD_INPUT_HANDLE), sock);

	if (loop(sock) == -1) {
		std::cerr << "loop failed";
		closesocket(sock);
		kjTh.~thread();
		return -1;
	}

	kjTh.~thread();

	return closesocket(sock);
}
