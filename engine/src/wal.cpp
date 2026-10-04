#include <fcntl.h>
#include <unistd.h>
#include "nook/wal.hpp"
#include "nook/file_util.hpp"

namespace nook {

Wal::ReplayResult Wal::replay(const std::string& path) {
    ReplayResult result;

    std::optional<std::string> data = read_file(path);
    if (!data) return result;

    size_t offset = 0;
    while (auto rec = decode_record(*data, offset)) {
        result.records.push_back(std::move(*rec));
    }
    result.valid_bytes = offset;
    return result;
}

Wal::Wal(const std::string& path) {
    ReplayResult existing = replay(path);

    fd_ = ::open(path.c_str(), O_WRONLY | O_CREAT, 0644);
    if (fd_ < 0) throw_errno("open " + path);

    if (::ftruncate(fd_, static_cast<off_t>(existing.valid_bytes)) != 0) throw_errno("ftruncate " + path);
    if (::lseek(fd_, 0, SEEK_END) < 0) throw_errno("lseek " + path);
}

Wal::~Wal() {
    if (fd_ >= 0) ::close(fd_);
}

Wal::Wal(Wal&& other) noexcept : fd_(other.fd_) {
    other.fd_ = -1;
}

Wal& Wal::operator=(Wal&& other) noexcept {
    if (this != &other) {
        if (fd_ >= 0) ::close(fd_);
        fd_ = other.fd_;
        other.fd_ = -1;
    }
    return *this;
}

void Wal::append(const WalRecord& r) {
    write_all(fd_, encode_record(r));
    if (::fsync(fd_) != 0) throw_errno("fsync");
}

void Wal::reset() {
    if (::ftruncate(fd_, 0) != 0) throw_errno("ftruncate");
    if (::lseek(fd_, 0, SEEK_SET) < 0) throw_errno("lseek");
    if (::fsync(fd_) != 0) throw_errno("fsync");
}

}