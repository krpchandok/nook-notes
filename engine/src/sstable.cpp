#include <algorithm>
#include <fcntl.h>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>
#include "nook/sstable.hpp"
#include "nook/coding.hpp"
#include "nook/crc32.hpp"
#include "nook/file_util.hpp"

namespace nook {

namespace {

constexpr size_t kIndexInterval = 16;
constexpr size_t kEntryHeaderSize = 9;
constexpr size_t kFooterSize = 32;
constexpr uint32_t kMagic = 0x4B4F4F4E;

void encode_entry(std::string& out, const Entry& e) {
    out.push_back(static_cast<char>(e.value ? 0 : 1));
    put_u32(out, static_cast<uint32_t>(e.key.size()));
    put_u32(out, e.value ? static_cast<uint32_t>(e.value->size()) : 0);
    out += e.key;
    if (e.value) out += *e.value;
}

Entry decode_entry(std::string_view data, size_t& pos) {
    if (data.size() - pos < kEntryHeaderSize) throw std::runtime_error("sstable: truncated entry");

    uint8_t type = static_cast<uint8_t>(data[pos]);
    uint32_t key_len = get_u32(data, pos + 1);
    uint32_t val_len = get_u32(data, pos + 5);
    pos += kEntryHeaderSize;

    if (data.size() - pos < static_cast<uint64_t>(key_len) + val_len) {
        throw std::runtime_error("sstable: truncated entry");
    }
    if (type > 1) throw std::runtime_error("sstable: bad entry type");

    Entry e;
    e.key = std::string(data.substr(pos, key_len));
    pos += key_len;
    if (type == 0) e.value = std::string(data.substr(pos, val_len));
    pos += val_len;
    return e;
}

}

void SSTable::write(const std::string& path, const std::vector<Entry>& entries) {
    std::string buf;
    std::vector<IndexEntry> index;

    for (size_t i = 0; i < entries.size(); ++i) {
        if (i % kIndexInterval == 0) index.push_back({entries[i].key, buf.size()});
        encode_entry(buf, entries[i]);
    }
    uint64_t data_end = buf.size();

    for (const IndexEntry& ie : index) {
        put_u32(buf, static_cast<uint32_t>(ie.key.size()));
        buf += ie.key;
        put_u64(buf, ie.offset);
    }

    uint32_t checksum = crc32(buf);
    put_u64(buf, data_end);
    put_u64(buf, index.size());
    put_u64(buf, entries.size());
    put_u32(buf, checksum);
    put_u32(buf, kMagic);

    write_file_atomically(path, buf);
}

void SSTable::write(const std::string& path, const MemTable& mt) {
    std::vector<Entry> entries;
    for (const auto& [key, value] : mt) entries.push_back({key, value});
    write(path, entries);
}

SSTable SSTable::open(const std::string& path) {
    SSTable t;
    t.path_ = path;
    t.fd_ = ::open(path.c_str(), O_RDONLY);
    if (t.fd_ < 0) throw_errno("open " + path);

    struct stat st;
    if (::fstat(t.fd_, &st) != 0) throw_errno("fstat " + path);
    uint64_t file_size = static_cast<uint64_t>(st.st_size);
    if (file_size < kFooterSize) throw std::runtime_error("sstable too small: " + path);

    std::string footer = pread_exact(t.fd_, file_size - kFooterSize, kFooterSize);
    uint64_t data_end    = get_u64(footer, 0);
    uint64_t index_count = get_u64(footer, 8);
    uint64_t entry_count = get_u64(footer, 16);
    uint32_t checksum    = get_u32(footer, 24);
    uint32_t magic       = get_u32(footer, 28);

    if (magic != kMagic) throw std::runtime_error("not an sstable: " + path);

    uint64_t body_size = file_size - kFooterSize;
    if (data_end > body_size) throw std::runtime_error("sstable footer corrupt: " + path);

    std::string body = pread_exact(t.fd_, 0, body_size);
    if (crc32(body) != checksum) throw std::runtime_error("sstable checksum mismatch: " + path);

    size_t pos = data_end;
    for (uint64_t i = 0; i < index_count; ++i) {
        if (body.size() - pos < 4) throw std::runtime_error("sstable index corrupt: " + path);
        uint32_t key_len = get_u32(body, pos);
        pos += 4;
        if (body.size() - pos < static_cast<uint64_t>(key_len) + 8) {
            throw std::runtime_error("sstable index corrupt: " + path);
        }
        std::string key = body.substr(pos, key_len);
        pos += key_len;
        uint64_t offset = get_u64(body, pos);
        pos += 8;
        t.index_.push_back({std::move(key), offset});
    }

    t.data_end_ = data_end;
    t.entry_count_ = entry_count;
    return t;
}

SSTable::~SSTable() {
    if (fd_ >= 0) ::close(fd_);
}

SSTable::SSTable(SSTable&& other) noexcept
    : path_(std::move(other.path_)),
      fd_(other.fd_),
      data_end_(other.data_end_),
      entry_count_(other.entry_count_),
      index_(std::move(other.index_)) {
    other.fd_ = -1;
}

SSTable& SSTable::operator=(SSTable&& other) noexcept {
    if (this != &other) {
        if (fd_ >= 0) ::close(fd_);
        path_ = std::move(other.path_);
        fd_ = other.fd_;
        data_end_ = other.data_end_;
        entry_count_ = other.entry_count_;
        index_ = std::move(other.index_);
        other.fd_ = -1;
    }
    return *this;
}

Lookup SSTable::lookup(const std::string& key) const {
    auto it = std::upper_bound(index_.begin(), index_.end(), key,
        [](const std::string& k, const IndexEntry& ie) { return k < ie.key; });

    if (it == index_.begin()) return Lookup{State::Absent, ""};

    uint64_t end = (it == index_.end()) ? data_end_ : it->offset;
    --it;
    uint64_t start = it->offset;

    std::string block = pread_exact(fd_, start, static_cast<size_t>(end - start));
    size_t pos = 0;
    while (pos < block.size()) {
        Entry e = decode_entry(block, pos);
        if (e.key == key) {
            if (e.value) return Lookup{State::Found, *e.value};
            return Lookup{State::Deleted, ""};
        }
        if (e.key > key) break;
    }
    return Lookup{State::Absent, ""};
}

std::vector<Entry> SSTable::read_all() const {
    std::vector<Entry> entries;
    entries.reserve(static_cast<size_t>(entry_count_));
    std::string data = pread_exact(fd_, 0, static_cast<size_t>(data_end_));
    size_t pos = 0;
    while (pos < data.size()) entries.push_back(decode_entry(data, pos));
    return entries;
}

}