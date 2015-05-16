#include "Filesystem.h"
#include <iostream>

int main(void) {
	Storage::FSChunk f("test.raw");
	Storage::File *file = f.open("testfile");
	for (auto it = f.begin(); it != f.end(); ++it) {
		Storage::File &m = *it;
		std::cout << "Name: " << m.getName() << std::endl;
		std::cout << "Size: " << m.getSize() << std::endl;
		std::cout << "Position: " << m.getPosition() << std::endl << std::endl;
	}
	f.shutdown();
	return 0;
}