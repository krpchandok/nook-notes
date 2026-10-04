#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <optional>
#include <set>
#include <map>
#include <string>
#include <vector>
#include "nook/db.hpp"
#include "nook/compaction.hpp"
#include "nook/file_util.hpp"
#include "nook/keys.hpp"
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

string DB::manifest_path() const {
	return (fs::path(dir) / "MANIFEST").string();
}

string DB::table_path(uint64_t number) const {
	char name[32];
	snprintf(name, sizeof(name), "%06llu.sst", static_cast<unsigned long long>(number));
	return (fs::path(dir) / name).string();
}

void DB::load_tables() {
	optional<Manifest> loaded = Manifest::load(manifest_path());

	if (loaded) {
		manifest = *loaded;
	} else {
		vector<uint64_t> numbers;
		for (const auto& entry : fs::directory_iterator(dir)) {
			if (auto n = parse_table_number(entry.path())) numbers.push_back(*n);
		}
		sort(numbers.begin(), numbers.end());

		manifest.tables = numbers;
		manifest.next_file_number = numbers.empty() ? 1 : numbers.back() + 1;
		manifest.save(manifest_path());
	}

	for (uint64_t n : manifest.tables) {
		sstables.push_back(SSTable::open(table_path(n)));
		manifest.next_file_number = max(manifest.next_file_number, n + 1);
	}

	remove_orphans();
}

void DB::remove_orphans() {
	set<uint64_t> live(manifest.tables.begin(), manifest.tables.end());

	vector<fs::path> doomed;
	for (const auto& entry : fs::directory_iterator(dir)) {
		const fs::path& p = entry.path();
		if (p.extension() == ".tmp") {
			doomed.push_back(p);
		} else if (auto n = parse_table_number(p); n && !live.count(*n)) {
			doomed.push_back(p);
		}
	}

	for (const fs::path& p : doomed) fs::remove(p);
	if (!doomed.empty()) fsync_dir(dir);
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

	uint64_t n = manifest.next_file_number++;
	string path = table_path(n);
	SSTable::write(path, memtable);
	SSTable table = SSTable::open(path);

	manifest.tables.push_back(n);
	manifest.save(manifest_path());

	sstables.push_back(std::move(table));
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
	uint64_t n = manifest.next_file_number++;
	string path = table_path(n);
	SSTable::write(path, merged);
	SSTable table = SSTable::open(path);

	vector<uint64_t> old_tables = manifest.tables;
	manifest.tables = {n};
	manifest.save(manifest_path());

	sstables.clear();
	sstables.push_back(std::move(table));

	for (uint64_t old : old_tables) fs::remove(table_path(old));
	fsync_dir(dir);
}

size_t DB::num_sstables() const {
	return sstables.size();
}

vector<DB::KeyValue> DB::scan(const string& start, const string& end) const {
	map<string, optional<string>> merged;

	for (const SSTable& t : sstables) {
		for (Entry& e : t.scan(start, end)) {
			merged[e.key] = std::move(e.value);
		}
	}
	for (Entry& e : memtable.scan(start, end)) {
		merged[e.key] = std::move(e.value);
	}

	vector<KeyValue> out;
	for (auto& [key, value] : merged) {
		if (value) out.emplace_back(key, std::move(*value));
	}
	return out;
}

vector<DB::KeyValue> DB::scan_prefix(const string& prefix) const {
	return scan(prefix, prefix_successor(prefix));
}

}