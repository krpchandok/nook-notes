#include <cerrno>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <system_error>
#include <unistd.h>
#include "nook/file_util.hpp"

namespace nook {

void throw_errno(const std::string& what) {
    throw std::system_error(errno, std::generic_category(), what);
}

void write_all(int fd, std::string_view data) {
    const char* p = data.data();
    size_t left = data.size();
    while (left > 0) {
        ssize_t n = ::write(fd, p, left);
        if (n < 0) {
            if (errno == EINTR) continue;
            throw_errno("write");
        }
        p += n;
        left -= static_cast<size_t>(n);
    }
}

std::string pread_exact(int fd, uint64_t offset, size_t len) {
    std::string buf(len, '\0');
    size_t done = 0;
    while (done < len) {
        ssize_t n = ::pread(fd, buf.data() + done, len - done, static_cast<off_t>(offset + done));
        if (n < 0) {
            if (errno == EINTR) continue;
            throw_errno("pread");
        }
        if (n == 0) throw std::runtime_error("pread: unexpected end of file");
        done += static_cast<size_t>(n);
    }
    return buf;
}

std::optional<std::string> read_file(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return std::nullopt;
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

void fsync_dir(const std::string& dir) {
    int fd = ::open(dir.c_str(), O_RDONLY);
    if (fd < 0) throw_errno("open dir " + dir);
    if (::fsync(fd) != 0) {
        int saved = errno;
        ::close(fd);
        errno = saved;
        throw_errno("fsync dir " + dir);
    }
    ::close(fd);
}

void write_file_atomically(const std::string& path, std::string_view data) {
    std::string tmp = path + ".tmp";

    int fd = ::open(tmp.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) throw_errno("open " + tmp);
    try {
        write_all(fd, data);
        if (::fsync(fd) != 0) throw_errno("fsync " + tmp);
    } catch (...) {
        ::close(fd);
        throw;
    }
    ::close(fd);

    if (::rename(tmp.c_str(), path.c_str()) != 0) throw_errno("rename " + tmp);

    std::string dir = std::filesystem::path(path).parent_path().string();
    fsync_dir(dir.empty() ? "." : dir);
}

}