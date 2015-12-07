#include "pkdb.hh"
#include "widgets.hh"
#include "io.hh"

#include "sqlite3.h"

static sqlite3* sqlite;
static const char* PKDB_FILENAME = "Host Database.sqlite";

class PreparedStatement {
  const char* sql; int sql_len;
  sqlite3_stmt* stmt;
  bool owari;
public:
  PreparedStatement(const char* sql) : sql(sql), sql_len(strlen(sql)+1),
                                       stmt(nullptr) {}
  bool Prepare() {
    if(stmt) return true;
    owari = false;
    int res = sqlite3_prepare(sqlite, sql, sql_len, &stmt, nullptr);
    return res == SQLITE_OK && stmt != nullptr;
  }
  void Reset() {
    if(stmt) { sqlite3_reset(stmt); owari = false; }
  }
  bool BindBlob(int i, const void* blob, size_t len) {
    if(stmt)
      return sqlite3_bind_blob(stmt, i, blob, len, SQLITE_TRANSIENT)
        == SQLITE_OK;
    else
      return false;
  }
  bool BindText(int i, const std::string& text) {
    if(stmt)
      return sqlite3_bind_text(stmt, i, text.data(), text.size(),
                               SQLITE_TRANSIENT) == SQLITE_OK;
    else
      return false;
  }
  // returns false on error
  bool Step() {
    if(owari) return true;
    int res = sqlite3_step(stmt);
    switch(res) {
    case SQLITE_DONE:
      owari = true;
      /* fall through */
    case SQLITE_ROW:
      return true;
    default:
      return false;
    }
  }
  // returns false if the value is not a blob of exactly that length
  bool GetBlob(int column, void* out, int len) {
    const void* blob = sqlite3_column_blob(stmt, column);
    int bytes = sqlite3_column_bytes(stmt, column);
    if(bytes != len || blob == nullptr) return false;
    memcpy(out, blob, len);
    return true;
  }
  bool GetString(int column, std::string& out) {
    const unsigned char* text = sqlite3_column_text(stmt, column);
    if(text == nullptr) return false;
    int bytes = sqlite3_column_bytes(stmt, column);
    out = std::string(reinterpret_cast<const char*>(text), bytes);
    return true;
  }
  bool IsFinished() const { return stmt && owari; }
  // returns true on success, false on failure
  bool Finish() {
    bool ret;
    while((ret = Step()) && !IsFinished()) {}
    return ret;
  }
  void Finalize() {
    if(stmt) {
      sqlite3_finalize(stmt);
      stmt = nullptr;
    }
  }
};
static PreparedStatement
  maybe_create_table("CREATE TABLE IF NOT EXISTS hosts"
                     " (host TEXT PRIMARY KEY NOT NULL,"
                     " public_key BLOB NOT NULL);"),
  exclusive_lock("PRAGMA locking_mode = EXCLUSIVE;"),
  get_public_key("SELECT public_key FROM hosts WHERE host = ?1 LIMIT 1;"),
  change_public_key("UPDATE hosts SET public_key = ?2 WHERE host = ?1;"),
  add_public_key("INSERT INTO hosts(host, public_key) VALUES (?1, ?2);"),
  wipe_public_key("DELETE FROM hosts WHERE host = ?1;");

bool PKDB::Init(Display&, std::string& whynot) {
  const char* dbpath = IO::GetConfigFilePath(PKDB_FILENAME);
  sqlite3_config(SQLITE_CONFIG_SINGLETHREAD);
  int res = sqlite3_open(dbpath, &sqlite);
  if(res == SQLITE_CANTOPEN) {
    if(sqlite) sqlite3_close(sqlite);
    IO::TryCreateConfigDirectory();
    res = sqlite3_open(dbpath, &sqlite);
  }
  if(res != SQLITE_OK) {
    whynot = sqlite3_errmsg(sqlite);
    if(sqlite) sqlite3_close(sqlite);
    sqlite3_shutdown();
    safe_free(const_cast<char*>(dbpath));
    return false;
  }
  exclusive_lock.Prepare() && exclusive_lock.Finish();
  exclusive_lock.Finalize();
  if(!maybe_create_table.Prepare() || !maybe_create_table.Finish()
     || !get_public_key.Prepare()) {
    whynot = std::string("The public key database seems to be corrupted. You might try deleting the database. Its location:\n\n")+dbpath;
    safe_free(const_cast<char*>(dbpath));
    PKDB::Fini();
    return false;
  }
  safe_free(const_cast<char*>(dbpath));
  return true;
}

void PKDB::Fini() {
  maybe_create_table.Finalize();
  get_public_key.Finalize();
  change_public_key.Finalize();
  add_public_key.Finalize();
  wipe_public_key.Finalize();
  sqlite3_close(sqlite);
  sqlite3_shutdown();
}

bool PKDB::GetPublicKey(const std::string& canon_name,
                        uint8_t public_key[TTTP_PUBLIC_KEY_LENGTH]) {
  if(!get_public_key.Prepare()) return false;
  get_public_key.Reset();
  if(!get_public_key.BindText(1, canon_name)) return false;
  if(!get_public_key.Step()) return false;
  if(get_public_key.IsFinished()) return false;
  if(!get_public_key.GetBlob(0, public_key, TTTP_PUBLIC_KEY_LENGTH))
    return false;
  get_public_key.Finish();
  return true;
}

void PKDB::ChangePublicKey(const std::string& canon_name,
                           const uint8_t public_key[TTTP_PUBLIC_KEY_LENGTH]) {
  if(!change_public_key.Prepare()) goto err;
  change_public_key.Reset();
  if(!change_public_key.BindText(1, canon_name)) goto err;
  if(!change_public_key.BindBlob(2, public_key, TTTP_PUBLIC_KEY_LENGTH))
    goto err;
  if(!change_public_key.Finish()) goto err;
  return;
 err:
  die("Updating the public key in the database failed. The database is probably corrupted.");
}

void PKDB::AddPublicKey(const std::string& canon_name,
                        const uint8_t public_key[TTTP_PUBLIC_KEY_LENGTH]) {
  if(!add_public_key.Prepare()) goto err;
  add_public_key.Reset();
  if(!add_public_key.BindText(1, canon_name)) goto err;
  if(!add_public_key.BindBlob(2, public_key, TTTP_PUBLIC_KEY_LENGTH))
    goto err;
  if(!add_public_key.Finish()) goto err;
  return;
 err:
  die("Adding the public key to the database failed. The database is probably corrupted.");
}

void PKDB::WipePublicKey(const std::string& canon_name) {
  if(!wipe_public_key.Prepare()) goto err;
  wipe_public_key.Reset();
  if(!wipe_public_key.BindText(1, canon_name)) goto err;
  if(!wipe_public_key.Finish()) goto err;
  return;
 err:
  die("Removing public keys from the database failed. The database is probably corrupted.");
}

