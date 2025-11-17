// server.cpp (NULL-safe patched)
// Build: g++ server.cpp -std=c++17 -lsqlite3 -pthread -o server

#include "httplib.h"
#include "json.hpp"
#include <sqlite3.h>
#include <iostream>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <random>
#include <vector>
#include <cstring>

using json = nlohmann::json;

// --- time helper
std::string now_iso()
{
    std::time_t t = std::time(nullptr);
    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&t));
    return std::string(buf);
}

// --- random hex
std::string random_hex(int len = 32)
{
    static std::mt19937_64 rng((unsigned)std::chrono::high_resolution_clock::now().time_since_epoch().count());
    static const char *hex = "0123456789abcdef";
    std::string s;
    s.reserve(len);
    for (int i = 0; i < len; ++i)
        s.push_back(hex[rng() & 0xF]);
    return s;
}

// --- small SHA256 (compact) - production: use libsodium/OpenSSL
typedef unsigned int uint32;
typedef unsigned long long uint64;
static inline uint32 rotr(uint32 x, uint32 n) { return (x >> n) | (x << (32 - n)); }
static inline uint32 ch(uint32 x, uint32 y, uint32 z) { return (x & y) ^ (~x & z); }
static inline uint32 maj(uint32 x, uint32 y, uint32 z) { return (x & y) ^ (x & z) ^ (y & z); }
static inline uint32 bsig0(uint32 x) { return rotr(x, 2) ^ rotr(x, 13) ^ rotr(x, 22); }
static inline uint32 bsig1(uint32 x) { return rotr(x, 6) ^ rotr(x, 11) ^ rotr(x, 25); }
static inline uint32 ssig0(uint32 x) { return rotr(x, 7) ^ rotr(x, 18) ^ (x >> 3); }
static inline uint32 ssig1(uint32 x) { return rotr(x, 17) ^ rotr(x, 19) ^ (x >> 10); }

std::string sha256(const std::string &msg)
{
    static const uint32 k[64] = {
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
        0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
        0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
        0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
        0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
        0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76f51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

    uint64 bitlen = (uint64)msg.size() * 8ULL;
    std::vector<unsigned char> data(msg.begin(), msg.end());
    data.push_back(0x80);
    while ((data.size() * 8) % 512 != 448)
        data.push_back(0x00);
    for (int i = 7; i >= 0; --i)
        data.push_back((bitlen >> (i * 8)) & 0xFF);

    uint32 h0 = 0x6a09e667, h1 = 0xbb67ae85, h2 = 0x3c6ef372, h3 = 0xa54ff53a;
    uint32 h4 = 0x510e527f, h5 = 0x9b05688c, h6 = 0x1f83d9ab, h7 = 0x5be0cd19;

    for (size_t chunk = 0; chunk < data.size(); chunk += 64)
    {
        uint32 w[64];
        memset(w, 0, sizeof(w));
        for (int i = 0; i < 16; ++i)
        {
            w[i] = ((uint32)data[chunk + i * 4] << 24) |
                   ((uint32)data[chunk + i * 4 + 1] << 16) |
                   ((uint32)data[chunk + i * 4 + 2] << 8) |
                   ((uint32)data[chunk + i * 4 + 3]);
        }
        for (int i = 16; i < 64; ++i)
            w[i] = ssig1(w[i - 2]) + w[i - 7] + ssig0(w[i - 15]) + w[i - 16];

        uint32 a = h0, b = h1, c = h2, d = h3, e = h4, f = h5, g = h6, h = h7;
        for (int i = 0; i < 64; ++i)
        {
            uint32 T1 = h + bsig1(e) + ch(e, f, g) + k[i] + w[i];
            uint32 T2 = bsig0(a) + maj(a, b, c);
            h = g;
            g = f;
            f = e;
            e = d + T1;
            d = c;
            c = b;
            b = a;
            a = T1 + T2;
        }
        h0 += a;
        h1 += b;
        h2 += c;
        h3 += d;
        h4 += e;
        h5 += f;
        h6 += g;
        h7 += h;
    }

    std::ostringstream oss;
    oss << std::hex << std::setfill('0') << std::nouppercase;
    auto write32 = [&](uint32 x)
    { oss << std::setw(8) << x; };
    write32(h0);
    write32(h1);
    write32(h2);
    write32(h3);
    write32(h4);
    write32(h5);
    write32(h6);
    write32(h7);
    return oss.str();
}

// small exec helper (no callback)
int exec_sql(sqlite3 *db, const char *sql)
{
    char *err = nullptr;
    int rc = sqlite3_exec(db, sql, nullptr, nullptr, &err);
    if (rc != SQLITE_OK && err)
    {
        std::cerr << "[SQL ERR] " << err << std::endl;
        sqlite3_free(err);
    }
    return rc;
}

// safe helper to convert possibly-NULL column text to std::string
static inline std::string to_str(const unsigned char *t)
{
    return t ? std::string(reinterpret_cast<const char *>(t)) : std::string("");
}

int main()
{
    sqlite3 *db;
    if (sqlite3_open("bank.db", &db) != SQLITE_OK)
    {
        std::cerr << "Cannot open DB: ensure bank.db exists and schema applied\n";
        return 1;
    }

    httplib::Server server;

    server.Get("/", [&](const httplib::Request &, httplib::Response &res)
               { res.set_content("MiniBank API Running!", "text/plain"); });

    // Signup
    server.Post("/signup", [&](const httplib::Request &req, httplib::Response &res)
                {
        try {
            auto j = json::parse(req.body);
            std::string email = j.value("email", "");
            std::string password = j.value("password", "");
            if (email.empty() || password.empty()) { res.set_content(R"({"status":"error","reason":"missing"})", "application/json"); return; }

            std::string salt = random_hex(24);
            std::string hash = sha256(salt + password);

            sqlite3_stmt* stmt = nullptr;
            sqlite3_prepare_v2(db, "INSERT INTO users (email, password_hash, salt, created_at) VALUES (?, ?, ?, ?)", -1, &stmt, nullptr);
            sqlite3_bind_text(stmt, 1, email.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 2, hash.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 3, salt.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 4, now_iso().c_str(), -1, SQLITE_TRANSIENT);

            json out;
            if (sqlite3_step(stmt) == SQLITE_DONE) { out["status"] = "ok"; }
            else { out["status"] = "error"; out["reason"] = "db_insert_failed"; }
            sqlite3_finalize(stmt);
            res.set_content(out.dump(), "application/json");
        } catch(...) { res.set_content(R"({"status":"error","reason":"json_parse_failed"})", "application/json"); } });

    // Login
    server.Post("/login", [&](const httplib::Request &req, httplib::Response &res)
                {
        try {
            auto j = json::parse(req.body);
            std::string email = j.value("email",""); std::string password = j.value("password","");
            if (email.empty() || password.empty()) { res.set_content(R"({"status":"error","reason":"missing"})", "application/json"); return; }

            sqlite3_stmt* stmt = nullptr;
            sqlite3_prepare_v2(db, "SELECT id, password_hash, salt FROM users WHERE email = ?", -1, &stmt, nullptr);
            sqlite3_bind_text(stmt, 1, email.c_str(), -1, SQLITE_TRANSIENT);

            json out;
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                int uid = sqlite3_column_int(stmt, 0);
                const unsigned char* stored_hash_p = sqlite3_column_text(stmt,1);
                const unsigned char* salt_p = sqlite3_column_text(stmt,2);
                std::string stored_hash = to_str(stored_hash_p);
                std::string salt = to_str(salt_p);
                sqlite3_finalize(stmt);
                std::string attempt = sha256(salt + password);
                if (attempt == stored_hash) {
                    // authentication success
                    out["status"] = "ok";
                    out["user_id"] = uid;
                    out["email"] = email;
                } else {
                    // wrong password
                    out["status"] = "invalid";
                    out["reason"] = "wrong_password";
                }
            } else {
                sqlite3_finalize(stmt);
                out["status"] = "invalid";
                out["reason"] = "not_found";
            }
            res.set_content(out.dump(), "application/json");
        } catch(...) { res.set_content(R"({"status":"error","reason":"json_parse_failed"})", "application/json"); } });

    // create_account
    server.Post("/create_account", [&](const httplib::Request &req, httplib::Response &res)
                {
        try {
            auto j = json::parse(req.body);
            int user_id = j.value("user_id", 0);
            std::string type = j.value("type","Savings");
            if (user_id == 0) { res.set_content(R"({"status":"error","reason":"missing_user"})", "application/json"); return; }
            std::string accnum = "ACC" + std::to_string((long long)(std::chrono::high_resolution_clock::now().time_since_epoch().count() % 9000000LL) + 100000LL);

            sqlite3_stmt* stmt = nullptr;
            sqlite3_prepare_v2(db, "INSERT INTO accounts (user_id, account_number, account_type, balance, created_at) VALUES (?, ?, ?, 0, ?)", -1, &stmt, nullptr);
            sqlite3_bind_int(stmt, 1, user_id);
            sqlite3_bind_text(stmt, 2, accnum.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 3, type.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 4, now_iso().c_str(), -1, SQLITE_TRANSIENT);

            json out;
            if (sqlite3_step(stmt) == SQLITE_DONE) { out["status"]="ok"; out["account_number"] = accnum; }
            else { out["status"]="error"; }
            sqlite3_finalize(stmt);
            res.set_content(out.dump(), "application/json");
        } catch(...) { res.set_content(R"({"status":"error","reason":"json_parse_failed"})", "application/json"); } });

    // accounts/{user_id}
    server.Get(R"(/accounts/(\d+))", [&](const httplib::Request &req, httplib::Response &res)
               {
        int user_id = std::stoi(req.matches[1]);
        sqlite3_stmt* stmt = nullptr;
        sqlite3_prepare_v2(db, "SELECT account_number, account_type, balance FROM accounts WHERE user_id = ?", -1, &stmt, nullptr);
        sqlite3_bind_int(stmt, 1, user_id);
        json arr = json::array();
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            json a; a["account_number"] = to_str(sqlite3_column_text(stmt,0));
            a["account_type"] = to_str(sqlite3_column_text(stmt,1));
            a["balance"] = sqlite3_column_double(stmt,2);
            arr.push_back(a);
        }
        sqlite3_finalize(stmt);
        res.set_content(arr.dump(), "application/json"); });

    // deposit
    server.Post("/deposit", [&](const httplib::Request &req, httplib::Response &res)
                {
        try {
            auto j = json::parse(req.body);
            std::string acc = j.value("account_number","");
            double amt = j.value("amount", 0.0);
            if (acc.empty() || amt <= 0.0) { res.set_content(R"({"status":"error","reason":"bad_request"})", "application/json"); return; }

            sqlite3_stmt* stmt = nullptr;
            sqlite3_prepare_v2(db, "UPDATE accounts SET balance = balance + ? WHERE account_number = ?", -1, &stmt, nullptr);
            sqlite3_bind_double(stmt, 1, amt);
            sqlite3_bind_text(stmt, 2, acc.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_step(stmt);
            int changed = sqlite3_changes(db);
            sqlite3_finalize(stmt);

            json out;
            if (changed == 0) { out["status"]="error"; out["reason"]="invalid_account"; res.set_content(out.dump(),"application/json"); return; }

            sqlite3_stmt* logstmt = nullptr;
            sqlite3_prepare_v2(db, "INSERT INTO transactions (tx_uuid, from_account, to_account, amount, created_at) VALUES (?, NULL, ?, ?, ?)", -1, &logstmt, nullptr);
            std::string txid = random_hex(16);
            sqlite3_bind_text(logstmt, 1, txid.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(logstmt, 2, acc.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_double(logstmt, 3, amt);
            sqlite3_bind_text(logstmt, 4, now_iso().c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_step(logstmt);
            sqlite3_finalize(logstmt);

            out["status"]="ok"; out["txid"]=txid;
            res.set_content(out.dump(),"application/json");
        } catch(...) { res.set_content(R"({"status":"error","reason":"json_parse_failed"})", "application/json"); } });

    // withdraw
    server.Post("/withdraw", [&](const httplib::Request &req, httplib::Response &res)
                {
        try {
            auto j = json::parse(req.body);
            std::string acc = j.value("account_number","");
            double amt = j.value("amount", 0.0);
            if (acc.empty() || amt <= 0.0) { res.set_content(R"({"status":"error","reason":"bad_request"})", "application/json"); return; }

            sqlite3_stmt* stmt = nullptr;
            sqlite3_prepare_v2(db, "SELECT balance FROM accounts WHERE account_number = ?", -1, &stmt, nullptr);
            sqlite3_bind_text(stmt, 1, acc.c_str(), -1, SQLITE_TRANSIENT);
            double bal = -1;
            if (sqlite3_step(stmt) == SQLITE_ROW) bal = sqlite3_column_double(stmt,0);
            sqlite3_finalize(stmt);

            json out;
            if (bal < amt || bal < 0) { out["status"]="error"; out["reason"]="insufficient_funds"; res.set_content(out.dump(),"application/json"); return; }

            sqlite3_prepare_v2(db, "UPDATE accounts SET balance = balance - ? WHERE account_number = ?", -1, &stmt, nullptr);
            sqlite3_bind_double(stmt, 1, amt);
            sqlite3_bind_text(stmt, 2, acc.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);

            sqlite3_stmt* logstmt = nullptr;
            sqlite3_prepare_v2(db, "INSERT INTO transactions (tx_uuid, from_account, to_account, amount, created_at) VALUES (?, ?, NULL, ?, ?)", -1, &logstmt, nullptr);
            std::string txid = random_hex(16);
            sqlite3_bind_text(logstmt, 1, txid.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(logstmt, 2, acc.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_double(logstmt, 3, amt);
            sqlite3_bind_text(logstmt, 4, now_iso().c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_step(logstmt);
            sqlite3_finalize(logstmt);

            out["status"]="ok"; res.set_content(out.dump(),"application/json");
        } catch(...) { res.set_content(R"({"status":"error","reason":"json_parse_failed"})", "application/json"); } });

    // transfer
    server.Post("/transfer", [&](const httplib::Request &req, httplib::Response &res)
                {
        try {
            auto j = json::parse(req.body);
            std::string from = j.value("from","");
            std::string to = j.value("to","");
            double amt = j.value("amount",0.0);
            if (from.empty() || to.empty() || amt <= 0.0) { res.set_content(R"({"status":"error","reason":"bad_request"})","application/json"); return; }

            exec_sql(db, "BEGIN IMMEDIATE;");

            sqlite3_stmt* stmt = nullptr;
            sqlite3_prepare_v2(db, "SELECT balance FROM accounts WHERE account_number = ?", -1, &stmt, nullptr);
            sqlite3_bind_text(stmt, 1, from.c_str(), -1, SQLITE_TRANSIENT);
            double bal = -1;
            if (sqlite3_step(stmt) == SQLITE_ROW) bal = sqlite3_column_double(stmt,0);
            sqlite3_finalize(stmt);

            if (bal < amt || bal < 0) { exec_sql(db,"ROLLBACK;"); json out; out["status"]="error"; out["reason"]="insufficient_funds"; res.set_content(out.dump(),"application/json"); return; }

            sqlite3_prepare_v2(db, "UPDATE accounts SET balance = balance - ? WHERE account_number = ?", -1, &stmt, nullptr);
            sqlite3_bind_double(stmt,1,amt); sqlite3_bind_text(stmt,2,from.c_str(),-1,SQLITE_TRANSIENT);
            sqlite3_step(stmt); sqlite3_finalize(stmt);

            sqlite3_prepare_v2(db, "UPDATE accounts SET balance = balance + ? WHERE account_number = ?", -1, &stmt, nullptr);
            sqlite3_bind_double(stmt,1,amt); sqlite3_bind_text(stmt,2,to.c_str(),-1,SQLITE_TRANSIENT);
            sqlite3_step(stmt); sqlite3_finalize(stmt);

            sqlite3_stmt* logstmt = nullptr;
            sqlite3_prepare_v2(db, "INSERT INTO transactions (tx_uuid, from_account, to_account, amount, created_at) VALUES (?, ?, ?, ?, ?)", -1, &logstmt, nullptr);
            std::string txid = random_hex(16);
            sqlite3_bind_text(logstmt,1,txid.c_str(),-1,SQLITE_TRANSIENT);
            sqlite3_bind_text(logstmt,2,from.c_str(),-1,SQLITE_TRANSIENT);
            sqlite3_bind_text(logstmt,3,to.c_str(),-1,SQLITE_TRANSIENT);
            sqlite3_bind_double(logstmt,4,amt);
            sqlite3_bind_text(logstmt,5,now_iso().c_str(),-1,SQLITE_TRANSIENT);
            sqlite3_step(logstmt); sqlite3_finalize(logstmt);

            exec_sql(db, "COMMIT;");
            json out; out["status"]="ok"; out["tx_uuid"]=txid; res.set_content(out.dump(),"application/json");
        } catch(...) { exec_sql(db,"ROLLBACK;"); res.set_content(R"({"status":"error","reason":"json_parse_failed"})","application/json"); } });

    // transactions/{acc}
    server.Get(R"(/transactions/(.*))", [&](const httplib::Request &req, httplib::Response &res)
               {
        std::string acc = req.matches[1];
        sqlite3_stmt* stmt = nullptr;
        sqlite3_prepare_v2(db, "SELECT from_account, to_account, amount, created_at FROM transactions WHERE from_account = ? OR to_account = ? ORDER BY id DESC", -1, &stmt, nullptr);
        sqlite3_bind_text(stmt, 1, acc.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, acc.c_str(), -1, SQLITE_TRANSIENT);
        json arr = json::array();
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            json t;
            const unsigned char* f = sqlite3_column_text(stmt,0);
            const unsigned char* to = sqlite3_column_text(stmt,1);
            t["from"] = to_str(f);
            t["to"] = to_str(to);
            t["amount"] = sqlite3_column_double(stmt,2);
            t["time"] = to_str(sqlite3_column_text(stmt,3));
            arr.push_back(t);
        }
        sqlite3_finalize(stmt);
        res.set_content(arr.dump(), "application/json"); });

    // export csv
    server.Get(R"(/export_transactions/(.*))", [&](const httplib::Request &req, httplib::Response &res)
               {
        std::string acc = req.matches[1];
        sqlite3_stmt* stmt = nullptr;
        sqlite3_prepare_v2(db, "SELECT id, tx_uuid, from_account, to_account, amount, created_at FROM transactions WHERE from_account = ? OR to_account = ? ORDER BY id DESC", -1, &stmt, nullptr);
        sqlite3_bind_text(stmt, 1, acc.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, acc.c_str(), -1, SQLITE_TRANSIENT);
        std::ostringstream csv;
        csv << "id,tx_uuid,from,to,amount,time\n";
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            csv << sqlite3_column_int(stmt,0) << ",";
            csv << "\"" << to_str(sqlite3_column_text(stmt,1)) << "\",";
            csv << "\"" << to_str(sqlite3_column_text(stmt,2)) << "\",";
            csv << "\"" << to_str(sqlite3_column_text(stmt,3)) << "\",";
            csv << sqlite3_column_double(stmt,4) << ",";
            csv << "\"" << to_str(sqlite3_column_text(stmt,5)) << "\"\n";
        }
        sqlite3_finalize(stmt);
        res.set_content(csv.str(), "text/csv"); });

    // ---- PROFILE ENDPOINTS ----
    // GET /profile/{user_id}
    server.Get(R"(/profile/(\d+))", [&](const httplib::Request &req, httplib::Response &res)
               {
        int uid = std::stoi(req.matches[1]);
        sqlite3_stmt* stmt = nullptr;
        sqlite3_prepare_v2(db, "SELECT id, email, name, phone, address, created_at FROM users WHERE id = ?", -1, &stmt, nullptr);
        sqlite3_bind_int(stmt, 1, uid);
        json out;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            out["status"] = "ok";
            out["user"] = {
                {"id", sqlite3_column_int(stmt,0)},
                {"email", to_str(sqlite3_column_text(stmt,1))},
                {"name", to_str(sqlite3_column_text(stmt,2))},
                {"phone", to_str(sqlite3_column_text(stmt,3))},
                {"address", to_str(sqlite3_column_text(stmt,4))},
                {"created_at", to_str(sqlite3_column_text(stmt,5))}
            };
        } else {
            out["status"] = "error";
            out["reason"] = "not_found";
        }
        sqlite3_finalize(stmt);
        res.set_content(out.dump(), "application/json"); });

    // POST /profile/update
    server.Post("/profile/update", [&](const httplib::Request &req, httplib::Response &res)
                {
        try {
            auto j = json::parse(req.body);
            int uid = j.value("user_id", 0);
            std::string name = j.value("name", "");
            std::string phone = j.value("phone", "");
            std::string address = j.value("address", "");
            if (uid == 0) { res.set_content(R"({"status":"error","reason":"missing_user"})","application/json"); return; }
            sqlite3_stmt* stmt = nullptr;
            sqlite3_prepare_v2(db, "UPDATE users SET name = ?, phone = ?, address = ? WHERE id = ?", -1, &stmt, nullptr);
            sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 2, phone.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 3, address.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_int(stmt, 4, uid);
            json out;
            if (sqlite3_step(stmt) == SQLITE_DONE) { out["status"]="ok"; }
            else { out["status"]="error"; out["reason"]="db_update_failed"; }
            sqlite3_finalize(stmt);
            res.set_content(out.dump(),"application/json");
        } catch(...) { res.set_content(R"({"status":"error","reason":"json_parse_failed"})","application/json"); } });

    std::cout << "MiniBank Server running at http://localhost:8080\n";
    bool ok = server.listen("0.0.0.0", 8080);
    if (!ok)
        std::cerr << "Failed to bind port 8080\n";

    sqlite3_close(db);
    return 0;
}
