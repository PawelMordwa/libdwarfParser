#ifndef FILE_DESCRIPTOR_H
#define FILE_DESCRIPTOR_H

#include <stdexcept>
#include <string>

// Prosta klasa RAII do obsługi deskryptora pliku
class FileDescriptor
{
	int fd;

   public:
	FileDescriptor(const std::string& path);
	~FileDescriptor();
	int get() const { return fd; }

	// Usuń kopiowanie
	FileDescriptor(const FileDescriptor&) = delete;
	FileDescriptor& operator=(const FileDescriptor&) = delete;
};

#endif	// FILE_DESCRIPTOR_H
