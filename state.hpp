#ifndef STATE_HPP
#define STATE_HPP

#include <WinSock2.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace state {
	constexpr auto MAX_NAME_LEN = 20;

	static inline auto playaBuf = std::vector<char>(6 + MAX_NAME_LEN);

	static inline auto playaNum = '1';

	struct playa {
		SOCKET sock = -1;
		uint16_t diamonds = 0;
		char namelen = 0;
		char symbol = playaNum;
		char x = 0, y = 0;
		char* name = nullptr;

		// playa(const playa& p) = delete;

		playa(SOCKET s, char symbol, char namelen, const char* name);

		playa(const char* buf)
			: diamonds(ntohs(*((short*) buf))), namelen(buf[2]), symbol(buf[3]), x(buf[4]), y(buf[5]), name(new char[namelen + 1]) {
			strncpy(name, buf + 6, namelen);
		}

		auto pack() -> const std::vector<char>& {
			*((short*) &playaBuf[0]) = htons(diamonds);
			playaBuf[2] = namelen;
			playaBuf[3] = symbol;
			playaBuf[4] = x;
			playaBuf[5] = y;
			strncpy(&playaBuf[6], this->name, namelen);
			playaBuf.resize(6 + namelen);
			return playaBuf;
		}

		~playa() {
			// delete[] name;
		}
	};

	static inline auto map = std::array<std::string, 20>{
		"........................................",
		"........................................",
		"........................................",
		"........................................",
		"........................................",
		"........................................",
		"........................................",
		"........................................",
		"........................................",
		"........................................",
		"........................................",
		"........................................",
		"........................................",
		"........................................",
		"........................................",
		"........................................",
		"........................................",
		"........................................",
		"........................................",
		"........................................",
	};

	static inline auto playaHaters = std::vector<playa>();

	struct map_object {
		char symbol, x, y;
		bool is;

		auto pack() -> std::array<char, 4> {
			return {this->symbol, this->x, this->y, this->is};
		}

		static auto unpack(char* buf) -> map_object {
			return *(map_object*) buf;
		}
	};

	static inline auto mapObjects = std::vector<map_object>{
		{.symbol = '*', .x = 5, .y = 1, .is = true},
		{.symbol = '#', .x = 16, .y = 3, .is = true},
		{.symbol = '#', .x = 17, .y = 3, .is = true},
		{.symbol = '#', .x = 18, .y = 3, .is = true},
		{.symbol = '#', .x = 19, .y = 3, .is = true},
		{.symbol = '#', .x = 20, .y = 3, .is = true},
		{.symbol = '#', .x = 21, .y = 3, .is = true},
		{.symbol = '#', .x = 22, .y = 3, .is = true},
		{.symbol = '#', .x = 23, .y = 3, .is = true},
		{.symbol = '#', .x = 24, .y = 3, .is = true},
		{.symbol = '#', .x = 25, .y = 3, .is = true},
		{.symbol = '#', .x = 26, .y = 3, .is = true},
		{.symbol = '#', .x = 27, .y = 3, .is = true},
		{.symbol = '*', .x = 33, .y = 4, .is = true},
		{.symbol = '#', .x = 7, .y = 6, .is = true},
		{.symbol = '#', .x = 8, .y = 6, .is = true},
		{.symbol = '#', .x = 9, .y = 6, .is = true},
		{.symbol = '#', .x = 10, .y = 6, .is = true},
		{.symbol = '#', .x = 11, .y = 6, .is = true},
		{.symbol = '#', .x = 12, .y = 7, .is = true},
		{.symbol = '#', .x = 13, .y = 8, .is = true},
		{.symbol = '#', .x = 18, .y = 8, .is = true},
		{.symbol = '#', .x = 19, .y = 8, .is = true},
		{.symbol = '#', .x = 20, .y = 8, .is = true},
		{.symbol = '#', .x = 21, .y = 8, .is = true},
		{.symbol = '#', .x = 22, .y = 8, .is = true},
		{.symbol = '#', .x = 23, .y = 8, .is = true},
		{.symbol = '#', .x = 24, .y = 8, .is = true},
		{.symbol = '#', .x = 25, .y = 8, .is = true},
		{.symbol = '#', .x = 26, .y = 8, .is = true},
		{.symbol = '#', .x = 27, .y = 8, .is = true},
		{.symbol = '#', .x = 28, .y = 8, .is = true},
		{.symbol = '#', .x = 29, .y = 8, .is = true},
		{.symbol = '#', .x = 14, .y = 9, .is = true},
		{.symbol = '*', .x = 4, .y = 10, .is = true},
		{.symbol = '#', .x = 15, .y = 10, .is = true},
		{.symbol = '#', .x = 25, .y = 10, .is = true},
		{.symbol = '#', .x = 26, .y = 10, .is = true},
		{.symbol = '#', .x = 27, .y = 10, .is = true},
		{.symbol = '#', .x = 28, .y = 10, .is = true},
		{.symbol = '#', .x = 16, .y = 11, .is = true},
		{.symbol = '#', .x = 17, .y = 12, .is = true},
		{.symbol = '#', .x = 18, .y = 13, .is = true},
		{.symbol = '#', .x = 34, .y = 13, .is = true},
		{.symbol = '#', .x = 35, .y = 13, .is = true},
		{.symbol = '#', .x = 36, .y = 13, .is = true},
		{.symbol = '#', .x = 37, .y = 13, .is = true},
		{.symbol = '*', .x = 8, .y = 14, .is = true},
		{.symbol = '#', .x = 19, .y = 14, .is = true},
		{.symbol = '#', .x = 20, .y = 15, .is = true},
		{.symbol = '*', .x = 29, .y = 15, .is = true},
		{.symbol = '#', .x = 2, .y = 17, .is = true},
		{.symbol = '#', .x = 3, .y = 17, .is = true},
		{.symbol = '#', .x = 4, .y = 17, .is = true},
		{.symbol = '#', .x = 5, .y = 17, .is = true},
		{.symbol = '#', .x = 6, .y = 17, .is = true},
		{.symbol = '#', .x = 7, .y = 17, .is = true},
		{.symbol = '#', .x = 8, .y = 17, .is = true},
		{.symbol = '#', .x = 9, .y = 17, .is = true},
		{.symbol = '#', .x = 10, .y = 17, .is = true},
		{.symbol = '#', .x = 11, .y = 17, .is = true},
		{.symbol = '#', .x = 12, .y = 17, .is = true},
		{.symbol = '#', .x = 13, .y = 17, .is = true},
		{.symbol = '*', .x = 28, .y = 18, .is = true},
	};

	constexpr auto udpPort = 7777;

	static inline auto receiveBuf = std::vector<char>(1024);

	static inline auto messages = std::vector<std::string>();
}  // namespace state

inline auto receive(SOCKET s) -> std::span<char> {
	auto received = 0;
receive:
	received += recv(s, state::receiveBuf.data() + received, state::receiveBuf.size() - received, 0);
	if (received <= 0) {
		std::cerr << "failed to recv " << WSAGetLastError() << '\n';
		return {};
	}
	if (received == state::receiveBuf.size()) {	 // its not okay if received filled the entire buf in our app
		state::receiveBuf.resize(state::receiveBuf.size() + 1024);
		goto receive;
	}

	return {state::receiveBuf.begin(), state::receiveBuf.begin() + received};
}

inline auto get_playa_num() -> char {
	for (const auto& playa : state::playaHaters) {
		if (playa.symbol == state::playaNum) {
			if (state::playaNum == '0') {
				state::playaNum = '1';
			}
			state::playaNum++;
			return get_playa_num();
		}
	}
	if (state::playaNum == '0') {
		state::playaNum = '1';
		return '0';
	}
	return state::playaNum++;
}

#define cmd(str) strstr(buf.data(), str) == buf.data()

#endif
