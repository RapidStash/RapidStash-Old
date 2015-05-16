#pragma once
#include <boost/iostreams/device/mapped_file.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/filesystem.hpp>

#include <stdlib.h>
#include <stdint.h>
#include <string>
#include <map>
#include <mutex>
#include <iostream>

namespace bio = boost::iostreams;
typedef bio::mapped_file MAPPED_FILE;

namespace Storage {

	// Metadata about the metadata
	struct MetaMetadata {
		const static uint64_t SIZE = sizeof(uint64_t) * 3;
		uint64_t firstFree;
		uint64_t firstUsed;
		uint64_t numFiles;
	};

	struct FileMeta {
		// Constants
		const static uint64_t NAME_SIZE = 64;
		const static uint64_t SIZE = (sizeof(uint64_t) * 3) + NAME_SIZE;

		char name[NAME_SIZE];		// Name of the file
		uint64_t size;				// File size in bytes
		uint64_t position;			// Position in the file
		uint64_t next;				// Next file index
	};

	class File {
		friend class FSChunk;
	public:
		File() {
			initialized = false;
		}
		File(File &other) {
			metadata = other.metadata;
			container = other.container;
			initialized = other.initialized;
		}
		std::string getName();
		uint64_t getSize();
		uint64_t getPosition();
	protected:
		void init(FileMeta&, FSChunk*);
	private:
		FileMeta *metadata;
		FSChunk *container;
		bool initialized;
	};

	class FSChunk {
		friend class File;
		class FileIterator; // Forward declaration

	public:
		FSChunk(std::string);
		void shutdown();
		File *open(std::string);
		void loadMeta(uint64_t, FileMeta *);
		File *find(std::string);
		void createNewFile(std::string);

		/*
		 *	Iterator over all files
		 */
		FileIterator begin() {
			return _begin;
		}
		FileIterator end() {
			return _end;
		}

		/*
		*	Iterator over free list
		*/
		FileIterator freeBegin() {
			return FileIterator(FREE_LIST, FIRST, this);
		}
		FileIterator freeEnd() {
			return FileIterator(FREE_LIST, LAST, this);
		}

		/*
		*	Iterator over allocated list
		*/
		FileIterator allocatedBegin() {
			return FileIterator(USED_LIST, FIRST, this);
		}
		FileIterator allocatedEnd() {
			return FileIterator(USED_LIST, LAST, this);
		}

	protected:
		// Initialize the file metadata list and initial space reserved for data.
		void initFilesystem();
		void loadMetaMetadata();

		// Some constants for the iterator.
		enum Position {
			FIRST,
			LAST
		};

		enum IteratorType {
			FREE_LIST,
			USED_LIST,
			BOTH
		};

		// Iterator for the file metadata list.
		class FileIterator {
		public:
			FileIterator() {}
			FileIterator(IteratorType t, Position p, FSChunk *fs_) : type(t), fs(fs_) {
				if (type == BOTH) {
					if (p == FIRST) {
						index = 1;
					} else if (p == LAST) {
						index = NUM_FILES + 1;
					}
				} else if (type == FREE_LIST) {
					if (p == FIRST) {
						index = fs->metaMeta.firstFree;
					} else if (p == LAST) {
						index = 0;
					}
				} else  if (type == USED_LIST) {
					if (p == FIRST) {
						index = fs->metaMeta.firstUsed;
					} else if (p == LAST) {
						index = 0;
					}
				}
				if (index > 0) {
					fs->loadMeta(index, &meta);
				}
			}
			FileIterator& operator++() {
				if (index == 0) return *this;
				if (type == BOTH) {
					index++;
				} else {
					index = meta.next;
				}
				if (index > 0) {
					fs->loadMeta(index, &meta);
				}
				return *this;
			}
			bool operator==(FileIterator& other) {
				return index == other.getIndex();
			}
			bool operator!=(FileIterator& other) {
				return index != other.getIndex();
			}
			File operator*() {
				// Return the file meta at the current position.
				File f;
				f.init(meta, fs);
				return f;
			}
			uint64_t getIndex() {
				return index;
			}
		private:
			IteratorType type;
			FileMeta meta;
			uint64_t index;
			FSChunk *fs;
		};

	private:
		// Filesystem stuff
		MAPPED_FILE fp;
		bio::stream<MAPPED_FILE> stream;
		std::string filename;
		MetaMetadata metaMeta;

		FileIterator _begin;
		FileIterator _end;

		// Constants
		static const uint64_t MAX_SIZE = 1024 * 1024 * 1024; // 1GB max
		static const uint64_t NUM_FILES = 2048;	// The max number of files managed by this chunk.
		static const uint64_t MAX_DATA_SIZE = MAX_SIZE - (NUM_FILES * FileMeta::SIZE); // Max size minus size of metadata list

		// Initial size of the filesystem should be enough for the metadata list and some data.  It will grow when necessary.
		static const double INIT_FACTOR;
		static const uint64_t INITIAL_SIZE;
	};

	class Filesystem {
	public:
		Filesystem();
	};
}
