#include <map>
#include <optional>
#include <string>
#include <vector>
#include "nook/compaction.hpp"
using namespace std;

namespace nook {

vector<Entry> merge_tables(const vector<SSTable>& tables, bool drop_tombstones) {
	map<string, optional<string>> merged;

	for (const SSTable& table : tables) {
        for (const Entry& e : table.read_all()) {
            merged[e.key] = e.value;
        }
    }

	vector<Entry> result;
	result.reserve(merged.size());
	for (const auto& [key, value] : merged) {
		if (!drop_tombstones || value) {
			result.push_back(Entry{key, value});
		}
	}

	return result;
}

}