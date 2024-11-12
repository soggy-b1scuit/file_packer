/*pack.hpp*/
#ifndef ASSETS_HPP
#define ASSETS_HPP

#include <iostream>
#include <cstdint>
#include <fstream>
#include <filesystem>

#include <zlib-ng.h>

#include <vector>
#include <unordered_map>

#include <algorithm>

namespace fs = std::filesystem;

using byte = uint8_t;

struct rfileinfo
{
    uint32_t offset;
};

struct fileinfo 
{
    byte* data;
    uint32_t size;
};

struct cfileinfo
{
    byte* data;
    uint32_t size;
    uint32_t osize;
};

struct packinfo
{
    std::unordered_map<std::string, rfileinfo> files;
    uint32_t offset;
    uint32_t blobsize;
};

namespace res
{
    extern packinfo pack;
    extern bool pack_mounted;
}

cfileinfo compress(const fileinfo file);
fileinfo uncompress(const cfileinfo file);

void detail(const fileinfo file);
void detail(const cfileinfo file);

int write_file(const char* path, const fileinfo file);
int write_file(const char* path, const cfileinfo file);

fileinfo read_file(const char* path);
cfileinfo read_cfile(const char* path);

template <typename T>
void append_bytes(std::vector<byte> &vector, T data)
{
    const byte* out = reinterpret_cast<byte*>(&data);
    vector.insert(vector.end(), out, out + sizeof(T));
}

void append_bytes(std::vector<byte> &vector, std::string data);

void append_bytes(std::vector<byte> &vector, byte* data, size_t size);

int write_pack(const char* path);

void read_pack_header(const char* path);

fileinfo read_pack_file(std::string path);

fileinfo read_pack_file_fast(std::string path);

void dump_pack();

#endif