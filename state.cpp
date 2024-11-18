#define _CRT_SECURE_NO_WARNINGS

#include "state.hpp"

namespace state {
	playa::playa(SOCKET s, char symbol, char namelen, const char* name)
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

}  // namespace state
