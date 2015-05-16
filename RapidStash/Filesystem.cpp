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

	_begin = FileIterator(BOTH, FIRST, this);
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
	stream.read(reinterpret_cast<char*>(&(metaMeta.firstFree)), sizeof(uint64_t));
	stream.read(reinterpret_cast<char*>(&(metaMeta.firstUsed)), sizeof(uint64_t));
	stream.read(reinterpret_cast<char*>(&(metaMeta.numFiles)), sizeof(uint64_t));
}

void Storage::FSChunk::initFilesystem() {
	char name[FileMeta::NAME_SIZE];
	memset(name, '\0', FileMeta::NAME_SIZE);
	uint64_t size = 0;
	uint64_t position = 0;
	uint64_t offset = 0;
	uint64_t next = 2;

	// First write the meta metadata.
	stream.seekg(0);
	stream.write(reinterpret_cast<char*>(&(metaMeta.firstFree)), sizeof(uint64_t));
	stream.write(reinterpret_cast<char*>(&(metaMeta.firstUsed)), sizeof(uint64_t));
	stream.write(reinterpret_cast<char*>(&(metaMeta.numFiles)), sizeof(uint64_t));

	// Copy in the first N-1 empty file metadata blocks.
	for (uint64_t b = 0; b < NUM_FILES - 1; ++b, ++next) {
		stream.seekg(MetaMetadata::SIZE + ( b * FileMeta::SIZE ));
		stream.write(name, FileMeta::NAME_SIZE);
		stream.write(reinterpret_cast<char*>(&size), sizeof(uint64_t));
		stream.write(reinterpret_cast<char*>(&position), sizeof(uint64_t));
		stream.write(reinterpret_cast<char*>(&next), sizeof(uint64_t));
	}

	// Copy in the last block
	uint64_t b = NUM_FILES - 1;
	next = 0;
	stream.seekg(MetaMetadata::SIZE + ( b * FileMeta::SIZE ));
	stream.write(name, FileMeta::NAME_SIZE);
	stream.write(reinterpret_cast<char*>(&size), sizeof(uint64_t));
	stream.write(reinterpret_cast<char*>(&position), sizeof(uint64_t));
	stream.write(reinterpret_cast<char*>(&next), sizeof(uint64_t));

	// Flush it out to the file.
	stream.flush();
}

Storage::File *Storage::FSChunk::find(std::string name) {
	for (auto it = allocatedBegin(); it != allocatedEnd(); ++it) {
		File &f = *it;
		if (f.getName() == name) {
			return new File(f);
		}
	}
	return NULL;
}

Storage::File *Storage::FSChunk::open(std::string name) {
	// Open file
	Storage::File *ret = find(name);
	if (ret == NULL) {
		// File not found.  Create it.
		createNewFile(name);
	}
	return NULL;
}

void Storage::FSChunk::createNewFile(std::string name) {

}

void Storage::FSChunk::loadMeta(uint64_t pos, Storage::FileMeta *meta) {
	// Translate the position
	uint64_t position = pos - 1;

	// Load metadata for a specific file
	stream.seekg(MetaMetadata::SIZE + ( position*FileMeta::SIZE ));

	// Copy the filename
	stream.read(meta->name, FileMeta::NAME_SIZE);

	// Copy the file size
	stream.read(reinterpret_cast<char*>(&(meta->size)), sizeof(uint64_t));

	// Copy the file position
	stream.read(reinterpret_cast<char*>(&(meta->position)), sizeof(uint64_t));

	// Copy the next file pointer
	stream.read(reinterpret_cast<char*>(&(meta->next)), sizeof(uint64_t));
}

void Storage::FSChunk::shutdown() {
	stream.flush();
	stream.close();
	fp.close();
}
#pragma endregion

#pragma region File Code
void Storage::File::init(FileMeta &meta, Storage::FSChunk* container) {
	metadata = new FileMeta();
	memcpy(metadata, &meta, sizeof(FileMeta));
	initialized = true;
}
std::string Storage::File::getName() {
	// Get size of file
	return metadata->name;
}
uint64_t Storage::File::getSize() {
	// Get size of file
	return metadata->size;
}
uint64_t Storage::File::getPosition() {
	// Get the position of the file in the filsystem chunk
	return metadata->position;
}
#pragma endregion

#pragma region Filesystem Code
Storage::Filesystem::Filesystem() {
	// Default constructor.
}
#pragma endregion