#include <string>
#include <optional>
#include <vector>
#include <algorithm>
#include "nook/sstable.hpp"
#include "nook/lookup.hpp"
#include "nook/memtable.hpp"
#include "nook/entry.hpp"
using namespace std;

namespace nook {

SSTable::SSTable(vector<Entry> entries): entries{std::move(entries)} {}

SSTable SSTable::from_memtable(const MemTable& mt) {
	vector<Entry> entries;
	for (const auto& [key, value] : mt) {
		entries.push_back(Entry{key, value});
	}

	return SSTable(std::move(entries));
}

Lookup SSTable::lookup(const string& key) const {
	auto it = lower_bound(entries.begin(), entries.end(), key, [](const Entry& entry, const string& k) {
		return entry.key < k;
	});

	if (it == entries.end() || it->key != key) {
		return Lookup{State::Absent, ""};
	}

	if (!it->value) {
		return Lookup{State::Deleted, ""};
	}

	return Lookup{State::Found, *it->value};
}

size_t SSTable::size() const {
	return entries.size();
}

}