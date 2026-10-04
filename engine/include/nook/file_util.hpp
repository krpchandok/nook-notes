#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace nook {

[[noreturn]] void throw_errno(const std::string& what);

void write_all(int fd, std::string_view data);
std::string pread_exact(int fd, uint64_t offset, size_t len);
std::optional<std::string> read_file(const std::string& path);
void write_file_atomically(const std::string& path, std::string_view data);
void fsync_dir(const std::string& dir);

}