# MSSQL Database Backup Tool for Tales of Pirates Private Servers

**Version:** 1.0

## Overview

This tool provides automated backups of the **AccountServer** and **GameDB** Microsoft SQL Server databases used in **Tales of Pirates** private servers. It runs on Windows and performs full backups at configurable time intervals, saving backup files with timestamps in a specified directory.

The tool is designed to ensure reliable and regular backups to help private server administrators safeguard their game data.

---

## Features

- Automated periodic backups of AccountServer and GameDB databases
- Configurable backup interval (in minutes)
- Configurable backup output directory
- Uses ODBC connection to MSSQL server with configurable credentials
- Creates backup files named with the database name and timestamp (`YYYYMMDD_HHMM`)
- Logs backup progress and errors to the console
- Automatically creates backup directory if it does not exist

---

## Requirements

- Windows OS
- Microsoft ODBC Driver 17 for SQL Server installed
- C++17 compatible compiler (tested with MSVC)
- Access to the MSSQL Server hosting the Tales of Pirates databases
- Configuration file (`config.ini`) placed in the same directory as the executable

---

## Configuration

The tool uses a `config.ini` file with the following structure:

```ini
[BackupSettings]
BackupDirectory=C:\SQLBackups\
IntervalMinutes=15
AccountServer=AccountServer
GameDB=GameDB
SQLServerInstance=localhost\SQLEXPRESS
SQLUser=sa
SQLPassword=your_password_here
```

- **BackupDirectory:** Directory where backup files will be saved.
- **IntervalMinutes:** Time interval between backups, in minutes.
- **AccountServer:** Name of the AccountServer database.
- **GameDB:** Name of the GameDB database.
- **SQLServerInstance:** MSSQL server instance name.
- **SQLUser:** Username for MSSQL authentication.
- **SQLPassword:** Password for MSSQL authentication.

---

## Usage

1. Build or download the executable.
2. Place the `config.ini` file in the same folder as the executable, and update the settings accordingly.
3. Run the executable. It will connect to the MSSQL server and start performing backups periodically.
4. To stop the tool, terminate the process manually (e.g., close the console window).

---

## Notes

- Ensure the SQL user has sufficient privileges to perform database backups.
- Backup files are overwritten if the timestamp matches an existing backup file name.
- The tool outputs detailed diagnostic messages on failure to aid troubleshooting.
- Intended specifically for use with **Tales of Pirates** private servers but can be adapted for other MSSQL databases.

---

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
