#include <cstdio>
#include <ctime>
#include <random>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include "nook/core/note.hpp"
using namespace std;

namespace nook::core {

string to_json(const Note& n) {
	nlohmann::json j;
	j["id"] = n.id;
	j["created_at"] = n.created_at;
	j["text"] = n.text;
	j["role"] = n.role;
	j["tags"] = n.tags;
	return j.dump();
}

Note note_from_json(const string& s) {
	nlohmann::json j = nlohmann::json::parse(s);
	Note n;
	n.id = j.at("id").get<string>();
	n.created_at = j.at("created_at").get<string>();
	n.text = j.at("text").get<string>();
	n.role = j.value("role", "");
	n.tags = j.value("tags", vector<string>{});
	return n;
}

string now_iso_utc() {
	time_t t = time(nullptr);
	tm utc{};
	gmtime_r(&t, &utc);

	char buf[32];
	strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &utc);
	return string(buf);
}

string random_id() {
	static mt19937 rng{random_device{}()};
	uniform_int_distribution<uint32_t> dist(0, 0xFFFFFF);

	char buf[8];
	snprintf(buf, sizeof(buf), "%06x", dist(rng));
	return string(buf);
}

}