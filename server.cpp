#define _CRT_SECURE_NO_WARNINGS

#include "server.hpp"

#include <WS2tcpip.h>

#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>
#include <thread>
#include <vector>

static short listeningPort;

#define dispatchToPlayas(buf)                    \
	for (auto pfd : pfds) {                      \
		send(pfd.fd, buf.data(), buf.size(), 0); \
	}

// udp is only used in broadcast packets to tell the sending client about server's tcp port
auto handleUdp(SOCKET s) -> void {
	std::cout << "udp: answering with port\n";

	auto incomingaddr = sockaddr_in{};
	auto incomingsize = (int) sizeof(incomingaddr);
	auto question = '\0';
	recvfrom(s, &question, 1, 0, (sockaddr*) &incomingaddr, &incomingsize);
	if (question == '?') {
		sendto(s, "Yes", 3, 0, (sockaddr*) &incomingaddr, incomingsize);
		// listening port MUST BE NETWORK BYTE ORDER
		sendto(s, (char*) &listeningPort, 2, 0, (sockaddr*) &incomingaddr, incomingsize);
	}
}

auto handlePing(SOCKET s, const std::span<char>& buf, const std::span<pollfd>& pfds) -> void {
	// ping + symbol + len + name
	auto& newPlaya = state::playaHaters.emplace_back(s, buf[4], buf[5], &buf[6]);

	for (const auto& playa : state::playaHaters) {
		if ((&newPlaya != &playa && !strncmp(newPlaya.name, playa.name, playa.namelen > newPlaya.namelen ? playa.namelen : newPlaya.namelen)) ||
			(&newPlaya != &playa && newPlaya.symbol == playa.symbol) ||
			newPlaya.symbol == '#' ||
			newPlaya.symbol == '*' ||
			newPlaya.symbol == '.') {
			delete[] newPlaya.name;
			newPlaya.name = new char[]{"Playa \0"};
			newPlaya.namelen = 7;
			newPlaya.symbol = get_playa_num();
			newPlaya.name[6] = newPlaya.symbol;
		}
	}

	auto outBuf = std::vector<char>();

	outBuf.reserve((5 + 2 + state::MAX_NAME_LEN) * state::playaHaters.size() + (3 + 2) * state::mapObjects.size() + 1);

#define pack(prefix, container)                                     \
	for (auto it = container.begin(); it < container.end(); ++it) { \
		outBuf.emplace_back(prefix);                                \
		outBuf.append_range(it->pack());                            \
		outBuf.emplace_back('\n');                                  \
	}

	pack('p', state::playaHaters);
	pack('o', state::mapObjects);
	outBuf.emplace_back(0);

#undef pack

	auto sent = 0;
	do {
		sent += send(s, outBuf.data() + sent, outBuf.size() - sent, 0);

		if (sent == -1) {
			std::cerr << "send failed " << WSAGetLastError();
			return;
		}
	} while (sent < outBuf.size());

	auto newPlayaBuf = std::vector{'n', 'e', 'w', 'p'};
	newPlayaBuf.append_range(newPlaya.pack());
	newPlayaBuf.emplace_back('\n');
	newPlayaBuf.emplace_back(0);
	for (const auto& pfd : pfds) {
		if (pfd.fd != newPlaya.sock) {
			send(pfd.fd, newPlayaBuf.data(), newPlayaBuf.size(), 0);
		}
	}
}

auto handleMove(SOCKET s, const std::span<char>& buf, const std::span<pollfd>& pfds) -> void {
	auto playa = std::find_if(state::playaHaters.begin(), state::playaHaters.end(), [s](const state::playa& playa) {
		return playa.sock == s;
	});

	if (playa == state::playaHaters.end()) {
		return;
	}

	auto x = playa->x, y = playa->y;
	switch (buf[4]) {
	case 'u':
		if (playa->y == 0) {
			return;
		}
		y--;
		break;
	case 'd':
		if (playa->y == 19) {
			return;
		}
		y++;
		break;
	case 'r':
		if (playa->x == 39) {
			return;
		}
		x++;
		break;
	case 'l':
		if (playa->x == 0) {
			return;
		}
		x--;
		break;
	default:
		return;
	}

	auto playaCollider = [&playa, x, y](const state::playa& p) {
		return (&*playa != &p) && p.x == x && p.y == y;
	};

	if (std::vector<state::playa>::iterator obj; (obj = std::find_if(state::playaHaters.begin(), state::playaHaters.end(), playaCollider)) != state::playaHaters.end()) {
		std::cerr << "collision detected '" << playa->symbol << "' hit a playa '" << obj->symbol << "' at " << (int) obj->x << ' ' << (int) obj->y << '\n';
		return;
	}

	auto objectMapCollider = [x, y](const state::map_object& mo) {
		return mo.symbol == '#' && mo.x == x && mo.y == y;
	};

	if (std::vector<state::map_object>::iterator obj; (obj = std::find_if(state::mapObjects.begin(), state::mapObjects.end(), objectMapCollider)) != state::mapObjects.end()) {
		std::cerr << "collision detected '" << playa->symbol << "' hit an object '" << obj->symbol << "' at " << (int) obj->x << ' ' << (int) obj->y << '\n';
		return;
	}

	auto starCollider = [x, y](const state::map_object& mo) {
		return mo.symbol == '*' && mo.x == x && mo.y == y;
	};

	if (std::vector<state::map_object>::iterator star; (star = std::find_if(state::mapObjects.begin(), state::mapObjects.end(), starCollider)) != state::mapObjects.end()) {
		state::mapObjects.erase(star);
		playa->diamonds++;
	}

	playa->x = x;
	playa->y = y;

	auto response = std::array{'m', 'o', 'v', 'e', buf[4], playa->symbol, '\0', '\0'};
	*(short*) &response[6] = htons(playa->diamonds);
	dispatchToPlayas(response);
}

auto handleChatEntry(SOCKET s, const std::span<char>& buf, const std::span<pollfd>& pfds) -> void {
	const auto& playa = std::find_if(state::playaHaters.begin(), state::playaHaters.end(), [s](const state::playa& playa) {
		return playa.sock == s;
	});

	if (playa == state::playaHaters.end()) {
		std::cerr << "received message from a missing player\n";
		return;
	}

	auto message = std::vector{'c', 'h', 'a', 't', playa->symbol};
	message.append_range(buf.subspan(5));
	dispatchToPlayas(message);
}

auto handleBeam(SOCKET s, const std::span<char>& buf, const std::span<pollfd>& pfds) -> void {
	const auto direction = buf[4];

	if (direction != 'u' && direction != 'l' && direction != 'r' && direction != 'd') return;

	auto playa = std::find_if(state::playaHaters.begin(), state::playaHaters.end(), [s](const state::playa& playa) {
		return playa.sock == s;
	});

	if (playa == state::playaHaters.end()) {
		std::cerr << "bean from a missing playa\n";
		return;
	}

	if (playa->diamonds == 0) {
		return;
	}

	playa->diamonds--;

	auto obstacles = std::vector<state::map_object>();
	auto hitPlayas = std::vector<state::playa>();
	obstacles.reserve(40);
	hitPlayas.reserve(state::playaHaters.size());

	auto onBeamPath = [x = playa->x, y = playa->y, direction](const auto& xy) {
		switch (direction) {
		case 'u':
			return xy.x == x && xy.y < y;
		case 'l':
			return xy.x < x && xy.y == y;
		case 'r':
			return xy.x > x && xy.y == y;
		case 'd':
			return xy.x == x && xy.y > y;
		default:
			return false;
		}
	};

	std::copy_if(state::mapObjects.begin(), state::mapObjects.end(), std::back_inserter(obstacles), onBeamPath);
	std::copy_if(state::playaHaters.begin(), state::playaHaters.end(), std::back_inserter(hitPlayas), onBeamPath);

	auto distance = [x = playa->x, y = playa->y](const auto& xy) -> char {
		using std::pow, std::abs;
		return abs(pow(xy.x - x, 2) + pow(xy.y - y, 2));
	};

	auto compare = [distance, invert = direction == 'd' || direction == 'l'](const auto& a, const auto& b) {
		return invert ? distance(a) < distance(b) : distance(a) > distance(b);
	};

	auto obstacle = std::min_element(obstacles.begin(), obstacles.end(), compare);

	auto hitPlaya = std::min_element(hitPlayas.begin(), hitPlayas.end(), [playa = &*playa, compare](const state::playa& p1, const state::playa& p2) {
		return !(&p1 == playa && &p2 != playa) && compare(p1, p2);
	});

	// beamer, direction, endX, endY
	auto msg = std::array{'b', 'e', 'a', 'm', playa->symbol, direction, char(direction == 'r' ? 40 : -1), char(direction == 'd' ? 20 : -1)};
	if (hitPlaya != hitPlayas.end() && (obstacle == obstacles.end() || distance(*hitPlaya) < distance(*obstacle))) {  // if playa is closer than to the obstacle, kill them
		msg[6] = hitPlaya->x;
		msg[7] = hitPlaya->y;

		send(hitPlaya->sock, msg.data(), msg.size(), 0);
		auto killMsg = std::array{'k', 'i', 'l', 'l', hitPlaya->symbol};
		std::erase_if(state::playaHaters, [&hitPlaya](const state::playa& playa) {
			if (playa.sock == hitPlaya->sock) {
				shutdown(playa.sock, SD_SEND);
				return true;
			}
			return false;
		});
		dispatchToPlayas(killMsg);
	} else if (obstacle != obstacles.end()) {
		msg[6] = obstacle->x;
		msg[7] = obstacle->y;
	}

	dispatchToPlayas(msg);
}

auto handleBomb(SOCKET s, const std::span<pollfd>& pfds) -> void {
	auto playa = std::find_if(state::playaHaters.begin(), state::playaHaters.end(), [s](const state::playa& playa) {
		return playa.sock == s;
	});

	if (playa == state::playaHaters.end()) {
		std::cerr << "bomb used by missing playa\n";
		return;
	}

	if (playa->diamonds == 0) {
		return;
	}

	playa->diamonds--;

	auto killedPlayas = std::vector<state::playa*>();
	killedPlayas.reserve(state::playaHaters.size());

	for (auto x = playa->x - 2; x <= playa->x + 2; ++x) {
		if (x < 0 || x > 39) continue;

		for (auto y = playa->y - 2; y <= playa->y + 2; ++y) {
			if ((playa->y == y && playa->x == x) || y < 0 || y > 19) continue;

			std::erase_if(state::mapObjects, [x, y](state::map_object mo) {
				return mo.x == x && mo.y == y;
			});

			for (auto& p : state::playaHaters) {
				if (&*playa != &p && p.x == x && p.y == y) {
					killedPlayas.emplace_back(&p);
				}
			}
		}
	}

	if (killedPlayas.size() > 0) {
		for (auto* playa : killedPlayas) {
			auto killMsg = std::array{'k', 'i', 'l', 'l', playa->symbol};

			dispatchToPlayas(killMsg);
			shutdown(playa->sock, SD_SEND);
		}
	}

	auto buf = std::array{'b', 'o', 'm', 'b', playa->symbol};
	dispatchToPlayas(buf);
}

auto logTcp(SOCKET s, const std::span<char>& buf) -> void {
	std::cout << "tcp: " << s << ": ";
	for (int i = 0; i < buf.size(); ++i) {
		std::cout << buf[i];
	}
	std::cout << '\n';
}

// tcp is used for all of the user commands
auto handleTcp(SOCKET s, const std::span<pollfd>& pfds) -> void {
	auto buf = receive(s);
	if (!buf.size()) {
		std::cerr << "read failed for " << s << '\n';
		return;
	}
	logTcp(s, buf);

	if (cmd("ping")) {
		handlePing(s, buf, pfds);
	} else if (cmd("move")) {
		handleMove(s, buf, pfds);
	} else if (cmd("chat")) {
		handleChatEntry(s, buf, pfds);
	} else if (cmd("beam")) {
		handleBeam(s, buf, pfds);
	} else if (cmd("bomb")) {
		handleBomb(s, pfds);
	} else {
		std::cerr << "no appropriate handle could be found\n";
	}
}

auto loop(SOCKET tcp, SOCKET udp) -> int {
	auto incomingaddr = sockaddr_in{};
	auto incomingsize = (int) sizeof(incomingaddr);

	auto pfds = std::vector{pollfd{.fd = tcp, .events = POLLIN}, pollfd{.fd = udp, .events = POLLIN}};
	pfds.reserve(12);

	std::cout << "accepting connections...\n";

	for (int e;;) {
		if ((e = WSAPoll(pfds.data(), pfds.size(), -1)) == -1) {
			std::cerr << "poll failed " << WSAGetLastError();
			return -1;
		}

		if (pfds[0].revents & POLLIN) {
			// 🧦
			auto sock = accept(tcp, (sockaddr*) &incomingaddr, &incomingsize);
			if (sock == -1) {
				std::cerr << "could not accept incoming conn\n";
				continue;
			} else if (pfds.size() == pfds.capacity()) {
				std::cerr << "rejecting connection as player count is 10\n";
				closesocket(sock);
				continue;
			}
			auto inetBuf = std::array<char, INET6_ADDRSTRLEN>();
			inet_ntop(incomingaddr.sin_family, &incomingaddr.sin_addr, inetBuf.data(), inetBuf.size());
			std::cout << "client " << sock << " connected " << inetBuf.data() << ':' << incomingaddr.sin_port << '\n';

			pfds.emplace_back(sock, POLLIN);
		} else if (pfds[1].revents & POLLIN) {
			handleUdp(pfds[1].fd);
		} else
			for (int i = 2; i < pfds.size(); ++i) {
				// TODO: implement e counter
				auto pfd = pfds[i];
				if (pfd.revents & POLLIN) {
					handleTcp(pfd.fd, {pfds.begin() + 2, pfds.end()});
				} else if (pfd.revents & (POLLHUP | POLLERR | POLLNVAL)) {
					if (pfds.size() >= 3) {
						std::swap(pfds[i], pfds[pfds.size() - 1]);
						std::cout << "client " << pfd.fd << " disconnected\n";
						closesocket(pfd.fd);
						pfds.pop_back();
						std::erase_if(state::playaHaters, [sock = pfd.fd](const state::playa& playa) {
							return playa.sock == sock;
						});
					}
				} else if (pfd.revents) {
					std::cout << "unhandled event " << pfd.revents << '\n';
				}
			}
	}

	return 0;
}

auto generateMap() -> std::vector<state::map_object> {
	return {
		{.symbol = '*', .x = 5, .y = 1},
		{.symbol = '#', .x = 16, .y = 3},
		{.symbol = '#', .x = 17, .y = 3},
		{.symbol = '#', .x = 18, .y = 3},
		{.symbol = '#', .x = 19, .y = 3},
		{.symbol = '#', .x = 20, .y = 3},
		{.symbol = '#', .x = 21, .y = 3},
		{.symbol = '#', .x = 22, .y = 3},
		{.symbol = '#', .x = 23, .y = 3},
		{.symbol = '#', .x = 24, .y = 3},
		{.symbol = '#', .x = 25, .y = 3},
		{.symbol = '#', .x = 26, .y = 3},
		{.symbol = '#', .x = 27, .y = 3},
		{.symbol = '*', .x = 33, .y = 4},
		{.symbol = '#', .x = 7, .y = 6},
		{.symbol = '#', .x = 8, .y = 6},
		{.symbol = '#', .x = 9, .y = 6},
		{.symbol = '#', .x = 10, .y = 6},
		{.symbol = '#', .x = 11, .y = 6},
		{.symbol = '#', .x = 12, .y = 7},
		{.symbol = '#', .x = 13, .y = 8},
		{.symbol = '#', .x = 18, .y = 8},
		{.symbol = '#', .x = 19, .y = 8},
		{.symbol = '#', .x = 20, .y = 8},
		{.symbol = '#', .x = 21, .y = 8},
		{.symbol = '#', .x = 22, .y = 8},
		{.symbol = '#', .x = 23, .y = 8},
		{.symbol = '#', .x = 24, .y = 8},
		{.symbol = '#', .x = 25, .y = 8},
		{.symbol = '#', .x = 26, .y = 8},
		{.symbol = '#', .x = 27, .y = 8},
		{.symbol = '#', .x = 28, .y = 8},
		{.symbol = '#', .x = 29, .y = 8},
		{.symbol = '#', .x = 14, .y = 9},
		{.symbol = '*', .x = 4, .y = 10},
		{.symbol = '#', .x = 15, .y = 10},
		{.symbol = '#', .x = 25, .y = 10},
		{.symbol = '#', .x = 26, .y = 10},
		{.symbol = '#', .x = 27, .y = 10},
		{.symbol = '#', .x = 28, .y = 10},
		{.symbol = '#', .x = 16, .y = 11},
		{.symbol = '#', .x = 17, .y = 12},
		{.symbol = '#', .x = 18, .y = 13},
		{.symbol = '#', .x = 34, .y = 13},
		{.symbol = '#', .x = 35, .y = 13},
		{.symbol = '#', .x = 36, .y = 13},
		{.symbol = '#', .x = 37, .y = 13},
		{.symbol = '*', .x = 8, .y = 14},
		{.symbol = '#', .x = 19, .y = 14},
		{.symbol = '#', .x = 20, .y = 15},
		{.symbol = '*', .x = 29, .y = 15},
		{.symbol = '#', .x = 2, .y = 17},
		{.symbol = '#', .x = 3, .y = 17},
		{.symbol = '#', .x = 4, .y = 17},
		{.symbol = '#', .x = 5, .y = 17},
		{.symbol = '#', .x = 6, .y = 17},
		{.symbol = '#', .x = 7, .y = 17},
		{.symbol = '#', .x = 8, .y = 17},
		{.symbol = '#', .x = 9, .y = 17},
		{.symbol = '#', .x = 10, .y = 17},
		{.symbol = '#', .x = 11, .y = 17},
		{.symbol = '#', .x = 12, .y = 17},
		{.symbol = '#', .x = 13, .y = 17},
		{.symbol = '*', .x = 28, .y = 18},
	};
}

auto server() -> int {
	state::mapObjects = generateMap();

	auto udp = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

	auto addr = sockaddr_in{
		.sin_family = AF_INET,
		.sin_port = htons(state::udpPort),
		.sin_addr = {.S_un = {.S_addr = INADDR_ANY}},
	};

	if (bind(udp, (sockaddr*) &addr, sizeof(addr)) == -1) {
		std::cerr << "udp port bind failed " << WSAGetLastError();
		return -1;
	}

	auto tcp = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (tcp == -1) {
		std::cerr << "tcp socket creation failed";
		return -1;
	}

	addr.sin_port = 0;
	addr.sin_addr.S_un.S_addr = INADDR_ANY;
	if (bind(tcp, (sockaddr*) &addr, sizeof(addr)) == -1) {
		closesocket(tcp);
		std::cerr << "tcp port bind failed";
		return -1;
	}

	auto len = (int) sizeof(addr);
	memset(&addr, 0, len);
	if (int res; (res = getsockname(tcp, (sockaddr*) &addr, &len)) == -1) {
		std::cerr << WSAGetLastError() << " getsockname failed\n";
		return -1;
	} else {
		listeningPort = addr.sin_port;
		auto buf = std::array<char, INET6_ADDRSTRLEN>();
		inet_ntop(addr.sin_family, &addr.sin_addr, buf.data(), buf.size());
		std::cout << "tcp listening on " << buf.data() << ':' << ntohs(addr.sin_port) << '\n';
	}

	if (setsockopt(tcp, SOL_SOCKET, SO_REUSEADDR, "1", 1) == -1) {
		std::cerr << "failed to reuse addr\n";
	}

	if (listen(tcp, 10) == -1) {
		closesocket(tcp);
		std::cerr << "listen failed " << WSAGetLastError();
		return -1;
	}

	if (loop(tcp, udp) == -1) {
		closesocket(tcp);
		return -1;
	}

	return closesocket(tcp);
}
