#ifndef STATE_HPP
#define STATE_HPP

#include <array>
#include <cstdint>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace state {
	constexpr auto MAX_NAME_LEN = 20;

	constexpr auto udpPort = 7777;

	constexpr auto MAP_WIDTH = 40, MAP_HEIGHT = 20;

	static inline auto messages = std::vector<std::string>();

	static inline auto map = std::array<std::string, MAP_HEIGHT>{
		std::string(MAP_WIDTH, '.'),
		std::string(MAP_WIDTH, '.'),
		std::string(MAP_WIDTH, '.'),
		std::string(MAP_WIDTH, '.'),
		std::string(MAP_WIDTH, '.'),
		std::string(MAP_WIDTH, '.'),
		std::string(MAP_WIDTH, '.'),
		std::string(MAP_WIDTH, '.'),
		std::string(MAP_WIDTH, '.'),
		std::string(MAP_WIDTH, '.'),
		std::string(MAP_WIDTH, '.'),
		std::string(MAP_WIDTH, '.'),
		std::string(MAP_WIDTH, '.'),
		std::string(MAP_WIDTH, '.'),
		std::string(MAP_WIDTH, '.'),
		std::string(MAP_WIDTH, '.'),
		std::string(MAP_WIDTH, '.'),
		std::string(MAP_WIDTH, '.'),
		std::string(MAP_WIDTH, '.'),
		std::string(MAP_WIDTH, '.'),
	};

	// fwd declare
	struct playa;
	struct map_object;

	static inline auto mapObjects = std::vector<map_object>();

	static inline auto playaHaters = std::vector<playa>();

	struct playa {
		SOCKET sock = -1;
		uint16_t diamonds = 0;
		char namelen = 0;
		char symbol = 0;
		char x = 0, y = 0;
		char* name = nullptr;

		// playa(const playa& p) = delete;

		// playa(playa&& other)
		// 	: sock(other.sock), diamonds(other.diamonds), namelen(other.namelen), symbol(other.symbol), x(other.x), y(other.y), name(other.name) {
		// 	other.name = nullptr;
		// }

		playa(SOCKET s, char symbol, char namelen, const char* name)
			: sock(s), symbol(symbol), namelen(namelen), name(new char[namelen + 1]{}) {
			auto finder = [x = &this->x, y = &this->y](const auto& xy) {
				return xy.x == *x && xy.y == *y;
			};

			std::vector<playa>::iterator playa;
			std::vector<map_object>::iterator mapObject;

			do {
				this->x = std::rand() % 40;
				this->y = std::rand() % 20;

				playa = std::find_if(playaHaters.begin(), playaHaters.end(), finder);
				mapObject = std::find_if(mapObjects.begin(), mapObjects.end(), finder);
			} while (playa != playaHaters.end() || mapObject != mapObjects.end());

			strncpy(this->name, name, namelen);
		}

		playa(const char* buf)
			: diamonds(ntohs(*(short*) buf)), namelen(buf[2]), symbol(buf[3]), x(buf[4]), y(buf[5]), name(new char[namelen + 1]{}) {
			strncpy(name, buf + 6, namelen);
		}

		auto pack() -> std::span<char> {
			static auto buf = std::array<char, 6 + MAX_NAME_LEN>();
			*(short*) &buf[0] = htons(diamonds);
			buf[2] = namelen;
			buf[3] = symbol;
			buf[4] = x;
			buf[5] = y;
			strncpy(&buf[6], this->name, namelen);
			return {buf.begin(), buf.begin() + 6 + namelen};
		}

		~playa() {
			// delete[] name;
		}
	};

	struct map_object {
		char symbol, x, y;

		auto pack() -> std::array<char, 3> {
			return {this->symbol, this->x, this->y};
		}

		static auto unpack(const char* buf) -> map_object {
			return *(map_object*) buf;
		}
	};
}  // namespace state

inline auto receive(SOCKET s) -> std::span<char> {
	static auto buf = std::vector<char>(1024);
	auto received = 0;
	auto pfd = pollfd{.fd = s, .events = POLLIN};
receive:
	received += recv(s, buf.data() + received, buf.size() - received, 0);
	if (received <= 0) {
		std::cerr << "failed to recv " << WSAGetLastError() << '\n';
		return {};
	}
	if (received == buf.size() && WSAPoll(&pfd, 1, 0) > 0) {
		buf.resize(buf.size() + 1024);
		goto receive;
	}

	return {buf.begin(), buf.begin() + received};
}

inline auto get_playa_num() -> char {
	static auto num = '0';
	if (++num > '9') {
		num = '1';
	}
	for (const auto& playa : state::playaHaters) {
		if (playa.symbol != num) continue;
		if (num == '9') {
			num = '1';
		}
		return get_playa_num();
	}
	if (num > '9') {
		num = '1';
	}
	return num;
}

#define cmd(str) strstr(buf.data(), str) == buf.data()

#endif
