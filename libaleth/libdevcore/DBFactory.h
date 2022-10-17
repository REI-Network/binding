// Aleth: Ethereum C++ client, tools and libraries.
// Copyright 2014-2019 Aleth Authors.
// Licensed under the GNU General Public License, Version 3.

#pragma once

#include "Common.h"
#include "db.h"

#include <boost/filesystem.hpp>

namespace dev
{
namespace db
{
enum class DatabaseKind
{
    LevelDB,
    RocksDB,
    MemoryDB,
    ExternalLevelDB
};

bool isDiskDatabase();
DatabaseKind databaseKind();
void setDatabaseKindByName(std::string const& _name);
void setDatabaseKind(DatabaseKind _kind);
boost::filesystem::path databasePath();

class DBFactory
{
public:
    DBFactory() = delete;
    ~DBFactory() = delete;

    static std::unique_ptr<DatabaseFace> create();
    static std::unique_ptr<DatabaseFace> create(void* db);
    static std::unique_ptr<DatabaseFace> create(boost::filesystem::path const& _path);
    static std::unique_ptr<DatabaseFace> create(DatabaseKind _kind);
    static std::unique_ptr<DatabaseFace> create(
        DatabaseKind _kind, boost::filesystem::path const& _path, void* db = nullptr);

private:
};
}  // namespace db
}  // namespace dev