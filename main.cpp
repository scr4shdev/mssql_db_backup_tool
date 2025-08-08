#include <windows.h>
#include <sql.h>
#include <sqlext.h>
#include <iostream>
#include <string>
#include <chrono>
#include <thread>
#include <ctime>
#include <fstream>
#include <algorithm>

// Get path of executable including trailing slash
std::string GetExePath() {
    char buffer[MAX_PATH];
    DWORD length = GetModuleFileNameA(NULL, buffer, MAX_PATH);
    if (length == 0 || length == MAX_PATH) return "";
    std::string fullPath(buffer, length);
    size_t pos = fullPath.find_last_of("\\/");
    if (pos == std::string::npos) return "";
    return fullPath.substr(0, pos + 1);
}

bool CreateDirectoryIfNotExists(const std::string& dirPath) {
    DWORD ftyp = GetFileAttributesA(dirPath.c_str());
    if (ftyp == INVALID_FILE_ATTRIBUTES) {
        // Directory does not exist, try to create it
        if (CreateDirectoryA(dirPath.c_str(), NULL) || GetLastError() == ERROR_ALREADY_EXISTS) {
            return true;
        }
        else {
            std::cerr << "Failed to create directory: " << dirPath << "\n";
            return false;
        }
    }
    else if (ftyp & FILE_ATTRIBUTE_DIRECTORY) {
        // Directory exists
        return true;
    }
    else {
        std::cerr << "Path exists but is not a directory: " << dirPath << "\n";
        return false;
    }
}

std::string GetIniValue(const std::string& section, const std::string& key, const std::string& filename, const std::string& defaultValue) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Failed to open config file: " << filename << "\n";
        return defaultValue;
    }

    std::string line;
    bool inSection = false;

    while (std::getline(file, line)) {
        if (line.empty() || line[0] == ';' || line[0] == '#')
            continue;

        if (line.front() == '[' && line.back() == ']') {
            std::string currentSection = line.substr(1, line.size() - 2);
            inSection = (currentSection == section);
            continue;
        }

        if (inSection) {
            size_t eqPos = line.find('=');
            if (eqPos == std::string::npos) continue;

            std::string currentKey = line.substr(0, eqPos);
            if (currentKey == key) {
                std::string value = line.substr(eqPos + 1);
                return value;
            }
        }
    }

    return defaultValue;
}

// Helper to get current timestamp for backup filename (YYYYMMDD_HHMM)
std::string GetTimestamp() {
    std::time_t now = std::time(nullptr);
    struct tm timeinfo;
    localtime_s(&timeinfo, &now);  // safer version

    char buf[20];
    std::strftime(buf, sizeof(buf), "%Y%m%d_%H%M", &timeinfo);
    return std::string(buf);
}

// ODBC error print helper
void PrintOdbcError(SQLHANDLE handle, SQLSMALLINT type, const std::string& msg) {
    SQLCHAR sqlState[6], msgText[256];
    SQLINTEGER nativeError;
    SQLSMALLINT textLength;
    if (SQLGetDiagRecA(type, handle, 1, sqlState, &nativeError, msgText, sizeof(msgText), &textLength) == SQL_SUCCESS) {
        std::cerr << msg << ": " << msgText << " (SQLState: " << sqlState << ")\n";
    }
    else {
        std::cerr << msg << ": Unknown error\n";
    }
}

// Print *all* SQL diagnostics
void PrintAllDiagnostics(SQLHANDLE handle, SQLSMALLINT handleType) {
    SQLSMALLINT i = 1;
    SQLCHAR sqlState[6], message[256];
    SQLINTEGER nativeError;
    SQLSMALLINT textLength;
    SQLRETURN ret;

    while ((ret = SQLGetDiagRecA(handleType, handle, i, sqlState, &nativeError, message, sizeof(message), &textLength)) == SQL_SUCCESS) {
        std::cerr << "ODBC Diagnostic: SQLState=" << sqlState << ", NativeError=" << nativeError
            << ", Message=" << message << "\n";
        i++;
    }

    if (i == 1) {
        std::cerr << "No ODBC diagnostics available.\n";
    }
}

// Execute SQL command with detailed diagnostics
bool ExecuteSqlCommand(SQLHDBC dbc, const std::string& sql) {
    SQLHSTMT stmt;
    if (SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt) != SQL_SUCCESS) {
        std::cerr << "Failed to allocate statement handle\n";
        PrintAllDiagnostics(dbc, SQL_HANDLE_DBC);
        return false;
    }

    SQLRETURN ret = SQLExecDirectA(stmt, (SQLCHAR*)sql.c_str(), SQL_NTS);
    if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO) {
        std::cerr << "Failed to execute SQL: " << sql << "\n";
        PrintAllDiagnostics(stmt, SQL_HANDLE_STMT);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return false;
    }

    std::cerr << "SQL executed successfully.\n";

    if (ret == SQL_SUCCESS_WITH_INFO) {
        std::cerr << "SQL executed with warnings: " << sql << "\n";
        PrintAllDiagnostics(stmt, SQL_HANDLE_STMT);
    }

    SQLFreeHandle(SQL_HANDLE_STMT, stmt);

    return true;
}



int main() {
    const std::string initString = "MSSQL Database Backup Tool 1.0\n"
        "Description: Allows backup for GameDB & AccountServer databases on a minutes based interval\n";    
    std::cout << initString << std::endl;

    const std::string configFile = GetExePath() + "config.ini";

    std::cout << "Loading config file: " << configFile << std::endl;

    std::string backupDir = GetIniValue("BackupSettings", "BackupDirectory", configFile, "C:\\SQLBackups\\");
    if (!CreateDirectoryIfNotExists(backupDir)) {
        std::cerr << "Cannot proceed without valid backup directory.\n";
        return 1;
    }

    int intervalMinutes = std::stoi(GetIniValue("BackupSettings", "IntervalMinutes", configFile, "15"));
    std::string accountDb = GetIniValue("BackupSettings", "AccountServer", configFile, "AccountServer");
    std::string gameDb = GetIniValue("BackupSettings", "GameDB", configFile, "GameDB");
    std::string sqlInstance = GetIniValue("BackupSettings", "SQLServerInstance", configFile, "localhost\\SQLEXPRESS");
    std::string sqlUser = GetIniValue("BackupSettings", "SQLUser", configFile, "sa");
    std::string sqlPassword = GetIniValue("BackupSettings", "SQLPassword", configFile, "");

    SQLHENV env;
    SQLHDBC dbc;
    SQLRETURN ret;

    ret = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &env);
    if (!SQL_SUCCEEDED(ret)) {
        std::cerr << "Failed to allocate ODBC environment handle\n";
        return 1;
    }

    ret = SQLSetEnvAttr(env, SQL_ATTR_ODBC_VERSION, (void*)SQL_OV_ODBC3, 0);
    if (!SQL_SUCCEEDED(ret)) {
        std::cerr << "Failed to set ODBC environment attribute\n";
        SQLFreeHandle(SQL_HANDLE_ENV, env);
        return 1;
    }

    ret = SQLAllocHandle(SQL_HANDLE_DBC, env, &dbc);
    if (!SQL_SUCCEEDED(ret)) {
        std::cerr << "Failed to allocate ODBC connection handle\n";
        SQLFreeHandle(SQL_HANDLE_ENV, env);
        return 1;
    }

    std::string connStr =
        "Driver={ODBC Driver 17 for SQL Server};"
        "Server=" + sqlInstance + ";"
        "UID=" + sqlUser + ";"
        "PWD=" + sqlPassword + ";"
        "ApplicationIntent=ReadWrite;"
        "TrustServerCertificate=yes;";

    SQLCHAR outConnStr[1024];
    SQLSMALLINT outConnStrLen;
    ret = SQLDriverConnectA(dbc, NULL, (SQLCHAR*)connStr.c_str(), SQL_NTS, outConnStr, sizeof(outConnStr), &outConnStrLen, SQL_DRIVER_COMPLETE);
    if (!SQL_SUCCEEDED(ret)) {
        PrintOdbcError(dbc, SQL_HANDLE_DBC, "Failed to connect to SQL Server");
        SQLFreeHandle(SQL_HANDLE_DBC, dbc);
        SQLFreeHandle(SQL_HANDLE_ENV, env);
        return 1;
    }

    std::cout << "Connected to SQL Server successfully.\n";

    while (true) {
        std::string timestamp = GetTimestamp();

        std::string backupAccount = "BACKUP DATABASE [" + accountDb + "] TO DISK = '" + backupDir + accountDb + "_backup_" + timestamp + ".bak' WITH FORMAT, NAME = 'Full Backup of " + accountDb + "';";
        std::string backupGame = "BACKUP DATABASE [" + gameDb + "] TO DISK = '" + backupDir + gameDb + "_backup_" + timestamp + ".bak' WITH FORMAT, NAME = 'Full Backup of " + gameDb + "';";
        std::cout << "Executing SQL: " << backupAccount << std::endl;
        std::cout << "Executing SQL: " << backupGame << std::endl;
        std::cout << "Starting backup for " << accountDb << "...\n";
        if (!ExecuteSqlCommand(dbc, backupAccount)) {
            std::cerr << "Backup failed for " << accountDb << "\n";
        }
        else {
            std::cout << "Backup succeeded for " << accountDb << "\n";
        }

        std::cout << "Starting backup for " << gameDb << "...\n";
        if (!ExecuteSqlCommand(dbc, backupGame)) {
            std::cerr << "Backup failed for " << gameDb << "\n";
        }
        else {
            std::cout << "Backup succeeded for " << gameDb << "\n";
        }

        std::cout << "Waiting for " << intervalMinutes << " minutes before next backup...\n";
        std::this_thread::sleep_for(std::chrono::minutes(intervalMinutes));
    }

    SQLDisconnect(dbc);
    SQLFreeHandle(SQL_HANDLE_DBC, dbc);
    SQLFreeHandle(SQL_HANDLE_ENV, env);
    std::cout << "Press Enter to exit...";
    std::cin.ignore();
    std::cin.get();
    return 0;
}
