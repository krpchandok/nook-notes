#include <optional>
#include <string>
#include <vector>
#include "nook/db.hpp"
#include "nook/compaction.hpp"
using namespace std;

namespace nook {

DB::DB(Options opts): opts{std::move(opts)} {}

void DB::put(const string& key, const string& value) {
	memtable.put(key, value);
	maybe_flush();
}

void DB::del(const string& key) {
	memtable.del(key);
	maybe_flush();
}

void DB::maybe_flush() {
	if (memtable.get_approx_bytes() >= opts.memtable_bytes) {
		flush();
	}
}

void DB::flush() {
	if (memtable.empty()) return;
	sstables.push_back(SSTable::from_memtable(memtable));
	memtable.clear();
}

optional<string> DB::get(const string& key) const {
	Lookup l = memtable.lookup(key);
	if (l.state == State::Found) return l.value;
	if (l.state == State::Deleted) return nullopt;

	for (auto it = sstables.rbegin(); it != sstables.rend(); ++it) {
		Lookup sl = it->lookup(key);
		if (sl.state == State::Found) return sl.value;
		if (sl.state == State::Deleted) return nullopt;
	}

	return nullopt;
}

void DB::compact() {
	flush();
	if (sstables.empty()) return;

	vector<Entry> merged = merge_tables(sstables, true);
	sstables.clear();
	sstables.push_back(SSTable(std::move(merged)));
}

size_t DB::num_sstables() const {
	return sstables.size();
}

}