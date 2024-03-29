#include "Filesystem.h"
#include <iostream>

#pragma region FSChunk Code
// Set up some of the static vars
const double Storage::FSChunk::INIT_FACTOR = 0.01;  // Initially allocate 1% of the maximum size
const uint64_t Storage::FSChunk::INITIAL_SIZE = (NUM_FILES * FileMeta::SIZE) + (uint64_t)(MAX_DATA_SIZE * INIT_FACTOR);

Storage::FSChunk::FSChunk(std::string fname_) : filename(fname_) {
	// MMAP the file chunk.
	bool exists = boost::filesystem::exists(filename);

	bio::mapped_file_params p(filename);
	if (!exists) {
		p.new_file_size = INITIAL_SIZE;
	}
	p.offset = 0;
	p.mode = std::ios::in | std::ios::out;

	fp = MAPPED_FILE(p);
	stream.open(fp);

	//_begin = FileIterator(BOTH, FIRST, this);
	_end = FileIterator(BOTH, LAST, this);

	// If the file doesn't exist, create initial linked list of file metadata
	if (!exists) {
		// Set up the meta metadata
		metaMeta.firstFree = 1;
		metaMeta.firstUsed = 0;
		metaMeta.numFiles = 0;
		initFilesystem();
	} else {
		// Read the meta metadata
		loadMetaMetadata();
	}
}

void Storage::FSChunk::loadMetaMetadata() {
	stream.seekg(0);
	stream.read(reinterpret_cast<char*>(&metaMeta), sizeof(MetaMetadata));
}

void Storage::FSChunk::writeMetaMetadata() {
	stream.seekp(0);
	stream.write(reinterpret_cast<char*>(&metaMeta), sizeof(MetaMetadata));
}

void Storage::FSChunk::initFilesystem() {
	FileMeta meta;

	// Init meta
	memset(meta.name, '\0', FileMeta::NAME_SIZE);
	meta.size = meta.position = 0;
	meta.next = 2;
	
	// First write the meta metadata.
	writeMetaMetadata();
	

	// Copy in the first N-1 empty file metadata blocks.
	for (uint64_t b = 0; b < NUM_FILES - 1; ++b, ++meta.next) {
		stream.write(meta.name, FileMeta::NAME_SIZE);
		stream.write(reinterpret_cast<char*>(&meta), 3 * sizeof(uint64_t));
	}

	// Copy in the last block
	meta.next = 0;
	stream.write(meta.name, FileMeta::NAME_SIZE);
	stream.write(reinterpret_cast<char*>(&meta), 3 * sizeof(uint64_t));

	// Flush it out to the file.
	stream.flush();
}

Storage::File* Storage::FSChunk::find(std::string name) {
	for (auto it = allocatedBegin(); it != allocatedEnd(); ++it) {
		File &f = *it;
		if (f.getName() == name) {
			return new File(f);
		}
	}
	return NULL;
}

Storage::File* Storage::FSChunk::open(std::string name) {
	Storage::File *ret = find(name);
	if (ret == NULL) {
		ret = createNewFile(name);
	}
	return ret;
}

// JAR: Should probably return the new  file since we are probably trying to open it
Storage::File* Storage::FSChunk::createNewFile(std::string name) {
	// TODO: Create a new file
	return NULL;
}

void Storage::FSChunk::saveMeta(uint64_t pos, Storage::FileMeta *meta) {
	// Translate the position
	uint64_t position = pos - 1;

	// Load metadata for a specific file
	stream.seekp(MetaMetadata::SIZE + (position * FileMeta::SIZE));

	// Write filename
	stream.write(meta->name, FileMeta::NAME_SIZE);

	// Write remaining
	stream.write(reinterpret_cast<char*>(meta), 3 * sizeof(uint64_t));
}

void Storage::FSChunk::loadMeta(uint64_t pos, Storage::FileMeta *meta) {
	// Translate the position
	uint64_t position = pos - 1;

	// Load metadata for a specific file
	stream.seekg(MetaMetadata::SIZE + ( position*FileMeta::SIZE ));

	// Copy the filename
	stream.read(meta->name, FileMeta::NAME_SIZE);

	/* Read remaining */
	stream.read(reinterpret_cast<char*>(meta), 3 * sizeof(uint64_t));

}

void Storage::FSChunk::shutdown() {
	static bool beenHere = false;
	if (!beenHere) { // Imagine if we did this twice
		stream.flush();
		stream.close();
		fp.close();
		beenHere = true;
	}
}
#pragma endregion

#pragma region File Code
void Storage::File::init(FileMeta* meta, Storage::FSChunk* container) {
	metadata = new FileMeta();	// TODO: Memory leak
	memcpy(metadata, meta, sizeof(FileMeta));
	initialized = true;
}

std::string Storage::File::getName() {
	return metadata->name;
}

uint64_t Storage::File::getSize() {
	return metadata->size;
}

uint64_t Storage::File::getPosition() {
	// Get the position of the file in the filsystem chunk
	return metadata->position;
}
#pragma endregion

#pragma region Filesystem Code
Storage::Filesystem::Filesystem() {
	/* Empty */
}
#pragma endregion