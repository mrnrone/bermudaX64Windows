/*
 * Bermuda Syndrome engine rewrite
 * Copyright (C) 2007-2011 Gregory Montoir
 */

#ifdef BERMUDA_WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#ifdef BERMUDA_POSIX
#include <dirent.h>
#include <sys/stat.h>
#endif

#ifdef _MSC_VER
 //not #if defined(_WIN32) || defined(_WIN64) because we have strncasecmp in mingw
#define strncasecmp _strnicmp
#define strcasecmp _stricmp
#endif

#ifndef _MSC_VER
#include <sys/ipc.h>
#include <sys/param.h>
#endif // !_MSC_VER
#include <sys/stat.h>
//#include "unistd.h"
#include "file.h"
#include "fs.h"
#include "str.h"

#define MAXPATHLEN 2048

#ifndef S_ISDIR
#define S_ISDIR(mode)  (((mode) & S_IFMT) == S_IFDIR)
#endif

#ifndef S_ISREG
#define S_ISREG(mode)  (((mode) & S_IFMT) == S_IFREG)
#endif

struct FileSystem_impl {
    FileSystem_impl() :
        _rootDir(0), _fileList(0), _fileCount(0), _filePathSkipLen(0) {
    }

    virtual ~FileSystem_impl() {
        free(_rootDir);
        for (int i = 0; i < _fileCount; ++i) {
            free(_fileList[i]);
        }
        free(_fileList);
    }

    void setDataDirectory(const char *dir) {
        _rootDir = strdup(dir);
        _filePathSkipLen = strlen(dir) + 1;
        buildFileListFromDirectory(dir);
    }

    int findFileIndex(const char *file) const {
        for (int i = 0; i < _fileCount; ++i) {
            if (strcasecmp(_fileList[i], file) == 0) {
                return i;
            }
        }
        return -1;
    }

    virtual const char *findFilePath(const char *file) const {
        const int i = findFileIndex(file);
        return (i < 0) ? 0 : _fileList[i];
    }

    virtual bool exists(const char *filePath) const {
        return findFilePath(filePath) != 0;
    }

    virtual void buildFileListFromDirectory(const char *dir) {
    }

    void addFileToList(const char *filePath) {
        _fileList = (char **)realloc(_fileList, (_fileCount + 1) * sizeof(char *));
        if (_fileList) {
            _fileList[_fileCount] = strdup(filePath + _filePathSkipLen);
            ++_fileCount;
        }
    }

    static FileSystem_impl *create();

    char *_rootDir;
    char **_fileList;
    int _fileCount;
    int _filePathSkipLen;

};

#ifdef BERMUDA_WIN32

const wchar_t *GetWC(const char *c)
{
    const size_t cSize = strlen(c) + 1;
    wchar_t* wc = new wchar_t[cSize];
    mbstowcs(wc, c, cSize);

    return wc;
}

#define MAX_FILE_PATH   2048
struct FileSystem_Win32 : FileSystem_impl {
    void buildFileListFromDirectory(const char *dir) {
        WIN32_FIND_DATA findData;
        char searchPath[MAX_FILE_PATH] = { 0 };
        snprintf(searchPath, sizeof(searchPath), "%s/*", dir);

        const wchar_t* tmpSearchPath = GetWC(searchPath);

        HANDLE h = FindFirstFile(tmpSearchPath, &findData);
        if (h) {
            do {
                char fileName[MAX_FILE_PATH] = { 0 };
                wcstombs(&fileName[0], findData.cFileName, MAX_FILE_PATH);

                char filePath[MAX_FILE_PATH] = { 0 };
                snprintf(filePath, MAX_FILE_PATH, "%s\\%s", dir, fileName);
                if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) 
                {
                    char tmpFileName[1024] = { 0 };
                    wcstombs(&tmpFileName[0], findData.cFileName, MAX_FILE_PATH);

                    if (strcmp(tmpFileName, "..") == 0 || strcmp(tmpFileName, ".") == 0) {
                        continue;
                    }
                    buildFileListFromDirectory(filePath);
                } else {
                    addFileToList(filePath);
                }
            } while (FindNextFile(h, &findData));
            FindClose(h);
        }

        delete tmpSearchPath;
    }
};
FileSystem_impl *FileSystem_impl::create() { return new FileSystem_Win32; }
#endif

#ifdef BERMUDA_POSIX
struct FileSystem_POSIX : FileSystem_impl {
    void buildFileListFromDirectory(const char *dir) {
        DIR *d = opendir(dir);
        if (d) {
            dirent *de;
            while ((de = readdir(d)) != NULL) {
                if (de->d_name[0] == '.') {
                    continue;
                }
                char filePath[MAXPATHLEN];
                snprintf(filePath, sizeof(filePath), "%s/%s", dir, de->d_name);
                struct stat st;
                if (stat(filePath, &st) == 0) {
                    if (S_ISDIR(st.st_mode)) {
                        buildFileListFromDirectory(filePath);
                    } else {
                        addFileToList(filePath);
                    }
                }
            }
            closedir(d);
        }
    }
};
FileSystem_impl *FileSystem_impl::create() { return new FileSystem_POSIX; }
#endif

#ifdef __EMSCRIPTEN__
struct FileSystem_Emscripten : FileSystem_impl {
    const char *findFilePath(const char *file) const {
        return file;
    }
    bool exists(const char *filePath) const {
        char path[MAXPATHLEN];
        snprintf(path, sizeof(path), "%s/%s", _rootDir, filePath);
        return File().open(path);
    }
};
FileSystem_impl *FileSystem_impl::create() { return new FileSystem_Emscripten; }
#endif

struct FileSystem_romfs: FileSystem_impl {
    static const int kHeaderSize = 16; // signature (8), size (4), checksum (4)
    static const int kMaxEntries = 2048;

    struct Entry {
        uint32_t offset;
        uint32_t size;
    };

    FileSystem_romfs(const char *filePath) {
        _filePath = strdup(filePath);
        _f.open(filePath);
        _f.seek(kHeaderSize);
        const int len = readString(0);
        const int align = (len + 15) & ~15;
        _f.seek(kHeaderSize + align);
        readTOC("");
    }

    ~FileSystem_romfs() {
        free(_filePath);
    }

    uint32_t readLong() {
        const uint32_t num = _f.readUint32BE();
        return num;
    }

    int readString(char *s) {
        int len = 0;
        while (1) {
            const char c = _f.readByte();
            if (s) {
                *s++ = c;
            }
            if (c == 0) {
                break;
            }
            ++len;
        }
        return len;
    }

    void readTOC(const char *dirPath, int level = 0) {
        int pos;
        do {
            const uint32_t nextOffset = readLong();
            const uint32_t specInfo = readLong();
            const uint32_t dataSize = readLong();
            readLong();
            char name[32];
            readString(name);
            char path[MAXPATHLEN];
            snprintf(path, sizeof(path), "%s/%s", dirPath, name);
            switch (nextOffset & 7) {
            case 1:
                if (name[0] != '.') {
                    _f.seek(specInfo);
                    readTOC(path, level + 1);
                }
                break;
            case 2:
                addFileToList(&path[1]);
                assert(_fileCount <= (int)ARRAYSIZE(_fileEntries));
                _fileEntries[_fileCount - 1].offset = (_f.tell() + 15) & ~15;
                _fileEntries[_fileCount - 1].size = dataSize;
                break;
            }
            pos = nextOffset & ~15;
            _f.seek(pos);
        } while (pos != 0);
    }

    const Entry *findFileEntry(const char *path) const {
        const int i = findFileIndex(path);
        return (i < 0) ? 0 : &_fileEntries[i];
    }

    char *_filePath;
    File _f;
    Entry _fileEntries[kMaxEntries];
};

FileSystem::FileSystem(const char *dataPath)
    : _impl(0), _romfs(false) {
    struct stat st;
    if (stat(dataPath, &st) == 0 && S_ISREG(st.st_mode)) {
        File f;
        if (f.open(dataPath)) {
            char sig[8];
            if (f.read(sig, sizeof(sig)) == sizeof(sig) && memcmp(sig, "-rom1fs-", 8) == 0) {
                _romfs = true;
                _impl = new FileSystem_romfs(dataPath);
                return;
            }
        }
    }
    _impl = FileSystem_impl::create();
    _impl->setDataDirectory(dataPath);
}

FileSystem::~FileSystem() {
    delete _impl;
}

static char *fixPath(const char *src) {
    char *path = (char *)malloc(strlen(src) + 4 + 1);
    if (path) {
        char *dst = path;
        if (strncmp(src, "..", 2) == 0) {
            src += 3;
        } 
        else 
        {
            if (strncmp(src, "SCN", 3) != 0)
            {
                strcpy(dst, "SCN\\");
                dst += 4;
            }
        }

        do {
            if (*src == '\\') {
                *dst = '\\';
            } else {
                *dst = *src;
            }
            ++dst;
        } while (*src++);
#ifdef __EMSCRIPTEN__
        stringToUpperCase(path);
#endif
    }
    return path;
}

File *FileSystem::openFile(const char *path, bool errorIfNotFound) {
    File *f = 0;
    char *fixedPath = fixPath(path);
    if (fixedPath) {
        if (_romfs) {
            const FileSystem_romfs::Entry *e = ((FileSystem_romfs *)_impl)->findFileEntry(fixedPath);
            if (e) {
                f = new File(e->offset, e->size);
                if (!f->open(((FileSystem_romfs *)_impl)->_filePath, "rb")) {
                    delete f;
                    f = 0;
                }
            }
        } else {
            const char *filePath = _impl->findFilePath(fixedPath);
            if (filePath) {
                f = new File;
                char fileSystemPath[MAXPATHLEN];
                snprintf(fileSystemPath, MAXPATHLEN, "%s%s", _impl->_rootDir, filePath);
                if (!f->open(fileSystemPath, "rb")) {
                    delete f;
                    f = 0;
                }
            }
        }
        free(fixedPath);
    }
    if (errorIfNotFound && !f) {
        error("Unable to open '%s'", path);
    }
    return f;
}

void FileSystem::closeFile(File *f) {
    if (f) {
        f->close();
        delete f;
    }
}

bool FileSystem::existFile(const char *path) {
    // bool exists = false;
    // char *fixedPath = fixPath(path);
    // if (fixedPath) {
    //     exists = _impl->exists(fixedPath);
    //     free(fixedPath);
    // }
    // return exists;

    return _impl->exists(path);
}
