#include "nook/memtable.hpp"
#include "nook/entry.hpp"
#include <string>
#include <optional>
using namespace std;

namespace nook {
size_t MemTable::entry_size(const std::string& key, const std::optional<std::string>& value) {
    return key.size() + (value ? value->size() : 0);
}

void MemTable::put(const string& key, const string& value) {
	if (entries.count(key)) {
		approx_bytes -= entry_size(key, entries[key]);
	}
	
	approx_bytes += entry_size(key, value);
	entries[key] = value;
}

void MemTable::del(const std::string& key) {
	if (entries.count(key)) {
		approx_bytes -= entry_size(key, entries[key]);
	}

	approx_bytes += entry_size(key,	nullopt);
	entries[key] = nullopt;
}

// map doesnt work on const keys so find via iterator
Lookup MemTable::lookup(const std::string& key) const {
    auto it = entries.find(key);
    if (it == entries.end()) return Lookup{State::Absent, ""};
    if (!it->second) return Lookup{State::Deleted, ""};
    return Lookup{State::Found, *it->second};
}

size_t MemTable::get_approx_bytes() const {
	return approx_bytes;
}

bool MemTable::empty() const {
	return entries.empty();
}

void MemTable::clear() {
	entries.clear();
	approx_bytes = 0;
}
std::vector<Entry> MemTable::scan(const std::string& start, const std::string& end) const {
    std::vector<Entry> out;
    for (auto it = entries.lower_bound(start); it != entries.end(); ++it) {
        if (!end.empty() && it->first >= end) break;
        out.push_back(Entry{it->first, it->second});
    }
    return out;
}

}