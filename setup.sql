-- ================================
-- MiniBank SQL Setup
-- ================================

PRAGMA foreign_keys = ON;

-- -------------------------
-- USERS TABLE
-- -------------------------
CREATE TABLE IF NOT EXISTS users (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    email TEXT UNIQUE NOT NULL,
    password_hash TEXT NOT NULL,
    salt TEXT NOT NULL,
    created_at TEXT NOT NULL
);

-- -------------------------
-- ACCOUNTS TABLE
-- -------------------------
CREATE TABLE IF NOT EXISTS accounts (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id INTEGER NOT NULL,
    account_number TEXT UNIQUE NOT NULL,
    account_type TEXT NOT NULL,
    balance REAL NOT NULL DEFAULT 0,
    created_at TEXT NOT NULL,

    FOREIGN KEY(user_id) REFERENCES users(id)
);

-- -------------------------
-- TRANSACTIONS TABLE
-- -------------------------
CREATE TABLE IF NOT EXISTS transactions (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    tx_uuid TEXT NOT NULL,
    from_account TEXT,
    to_account TEXT,
    amount REAL NOT NULL,
    created_at TEXT NOT NULL
);

-- -------------------------
-- AUDIT LOG TABLE
-- -------------------------
CREATE TABLE IF NOT EXISTS audit_logs (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id INTEGER,
    action TEXT NOT NULL,
    details TEXT,
    created_at TEXT NOT NULL
);