#include "file_descriptor.h"

#include <fcntl.h>
#include <unistd.h>

FileDescriptor::FileDescriptor(const std::string& path)
{
	fd = open(path.c_str(), O_RDONLY);
	if (fd < 0)
	{
		throw std::runtime_error("Nie można otworzyć pliku: " + path);
	}
}

FileDescriptor::~FileDescriptor()
{
	if (fd >= 0)
		close(fd);
}
