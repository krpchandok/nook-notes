#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>
#include "nook/db.hpp"
#include "nook/compaction.hpp"
#include "nook/file_util.hpp"
using namespace std;
namespace fs = std::filesystem;

namespace nook {

static optional<uint64_t> parse_table_number(const fs::path& p) {
	if (p.extension() != ".sst") return nullopt;
	string stem = p.stem().string();
	if (stem.empty()) return nullopt;
	for (char c : stem) {
		if (!isdigit(static_cast<unsigned char>(c))) return nullopt;
	}
	return stoull(stem);
}

DB::DB(const string& dir, Options opts): dir{dir}, opts{std::move(opts)} {
	fs::create_directories(dir);
	load_tables();

	string wal_path = (fs::path(dir) / "wal.log").string();
	Wal::ReplayResult replayed = Wal::replay(wal_path);
	for (const WalRecord& r : replayed.records) {
		apply(r);
	}

	wal.emplace(wal_path);
	maybe_flush();
}

void DB::load_tables() {
	vector<uint64_t> numbers;
	for (const auto& entry : fs::directory_iterator(dir)) {
		const fs::path& p = entry.path();
		if (p.extension() == ".tmp") {
			fs::remove(p);
			continue;
		}
		if (auto n = parse_table_number(p)) numbers.push_back(*n);
	}

	sort(numbers.begin(), numbers.end());
	for (uint64_t n : numbers) {
		sstables.push_back(SSTable::open(table_path(n)));
		next_file_number = n + 1;
	}
}

string DB::table_path(uint64_t number) const {
	char name[32];
	snprintf(name, sizeof(name), "%06llu.sst", static_cast<unsigned long long>(number));
	return (fs::path(dir) / name).string();
}

void DB::apply(const WalRecord& r) {
	if (r.type == RecordType::Put) memtable.put(r.key, r.value);
	else memtable.del(r.key);
}

void DB::put(const string& key, const string& value) {
	WalRecord r{RecordType::Put, key, value};
	wal->append(r);
	apply(r);
	maybe_flush();
}

void DB::del(const string& key) {
	WalRecord r{RecordType::Delete, key, ""};
	wal->append(r);
	apply(r);
	maybe_flush();
}

void DB::maybe_flush() {
	if (memtable.get_approx_bytes() >= opts.memtable_bytes) {
		flush();
	}
}

void DB::flush() {
	if (memtable.empty()) return;

	string path = table_path(next_file_number++);
	SSTable::write(path, memtable);
	sstables.push_back(SSTable::open(path));

	wal->reset();
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
	string path = table_path(next_file_number++);
	SSTable::write(path, merged);

	vector<string> old_paths;
	for (const SSTable& t : sstables) old_paths.push_back(t.path());

	sstables.clear();
	sstables.push_back(SSTable::open(path));

	for (const string& p : old_paths) fs::remove(p);
	fsync_dir(dir);
}

size_t DB::num_sstables() const {
	return sstables.size();
}

}