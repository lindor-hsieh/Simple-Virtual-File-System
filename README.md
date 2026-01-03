# 📂 Simple Virtual File System (MyFS)

A robust user-space file system simulation implemented in C. This project simulates a disk partition using a single file (`my_fs.dump`) and provides a command-line interface (CLI) to interact with the virtual file system, featuring inode management, permission control, encryption, and disk defragmentation.

![Language](https://img.shields.io/badge/language-C-blue.svg)
![Platform](https://img.shields.io/badge/platform-Windows-lightgrey.svg)
![License](https://img.shields.io/badge/license-MIT-green.svg)

## ✨ Features

### 🛠 Core File Operations
- **Navigation:** `ls` (list), `cd` (change dir), `pwd` (print working dir).
- **Management:** `mkdir` (create dir), `rmdir` (remove dir), `touch` (create file), `rm` (remove file), `cp` (copy), `mv` (move/rename).
- **I/O Redirection:** Supports `>` to redirect command output to files (e.g., `ls -l > filelist.txt`).

### 📝 Text Editing & Search
- **Nano Editor:** Built-in line editor (`nano`) to create and modify text files.
- **Search:**
  - `grep`: Search for keywords inside files.
  - `find`: Recursive file search by name.
- **Content View:** `cat` (view text), `hexdump` (view binary/hex).

### 🔒 Security & Permissions
- **Permission System:** Unix-like permission bits (Read/Write/Exec).
  - Use `chmod` to change modes (e.g., `chmod 4 file.txt` for Read-Only).
  - Protected operations: Cannot write to R-only files, cannot run non-Exec files.
- **Encryption:** `encrypt` / `decrypt` files using XOR cipher with a custom key.
- **Execution:** `run` to execute binary files (`.exe`) stored in the VFS.

### 💾 Host Interaction
- **Put:** Import files from the host computer (Windows) to VFS.
- **Get:** Export files from VFS to the host computer.

### 🔧 System Maintenance
- **Visualization:** `diskmap` to visualize disk block usage (heatmap).
- **Optimization:** `defrag` to consolidate fragmented blocks.
- **Status:** `status` to view inode/block usage statistics.
- **Persistence:** Automatically saves file system state to `my_fs.dump` on exit.

---

## 🚀 Getting Started

### Prerequisites
- GCC Compiler (MinGW for Windows)
- Make (optional, but recommended)

### Build
Use the provided `Makefile` to compile the project:

```powershell
# Clean previous builds
make clean

# Compile the project
make
```
### Run
Start the file system shell:

```powershell
.\myfs.exe
```
Upon starting, you will be prompted to:

Load: Load an existing file system (`my_fs.dump`).

New Partition: Create a fresh file system (Warning: Erases old data).

## 📖 Usage Examples

### 1. Basic File Management
```bash
/ $ mkdir documents
/ $ cd documents
/documents $ touch notes.txt
/documents $ nano notes.txt
(Enter text and save)
```
### 2. Import & Execute Programs
To test execution, compile a simple C program on your host machine first:

```bash
# Host machine
gcc hello.c -o hello.exe
```
Then import and run it in MyFS:

```bash
# Inside MyFS
/ $ put hello.exe
/ $ chmod 7 hello.exe
/ $ run hello.exe
```
### 3. Encryption Demo
```bash
/ $ encrypt notes.txt mysecretkey
/ $ cat notes.txt
(Shows garbage data)
/ $ decrypt notes.txt mysecretkey
/ $ cat notes.txt
(Content restored)
```
### 4. Disk Maintenance
```bash
/ $ diskmap
(Shows block usage map)
/ $ defrag
(Optimizes storage layout)
```
## 📂 Project Structure
```plaintext
Simple-Virtual-File-System/
├── src/            # Source code (.c files)
│   ├── main.c      # Entry point & shell loop
│   ├── fs.c        # File system core logic
│   ├── commands.c  # Command implementations
│   ├── inode.c     # Inode management
│   ├── bitmap.c    # Block allocation bitmap
│   ├── security.c  # Encryption logic
│   └── ...
├── include/        # Header files (.h files)
├── obj/            # Compiled object files (ignored by git)
├── Makefile        # Build configuration
└── README.md       # Project documentation
```
## ⚙️ Technical Details
Block Size: 1024 bytes (default).

Inode Table: Stores metadata (name, size, permissions, block pointers).

Superblock: Tracks global file system state (total size, free blocks).

Data Persistence: The entire file system is serialized into a single binary file (`.dump`).

## 🤝 Contributing
Contributions are welcome! Feel free to open issues or submit pull requests.

## 📄 License
This project is open-source and available under the MIT License.