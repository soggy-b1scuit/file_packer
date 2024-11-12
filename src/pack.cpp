/*pack.cpp*/
#include "pack.hpp"

const char* zng_getErrorString(int err)
{
    switch(err)
    {
        case Z_ERRNO:
            return "FILE I/O ERROR";
        case Z_STREAM_ERROR:
            return "STREAM ERROR";
        case Z_DATA_ERROR:
            return "DATA ERROR";
        case Z_MEM_ERROR:
            return "MEMORY ERROR";
        case Z_BUF_ERROR:
            return "BUFFER ERROR";
        case Z_VERSION_ERROR:
            return "VERSION ERROR";
        default:
            return "?";
    }
}

namespace res
{
    packinfo pack;
    bool pack_mounted = false;
}

cfileinfo compress(const fileinfo file)
{
    size_t size = zng_compressBound(file.size);
    auto out = std::make_unique<byte[]>(size);
    
    int err = zng_compress2(out.get(), &size, file.data, file.size, 9);

    if (err != Z_OK)
    {
        std::cout << zng_getErrorString(err) << '\n';
        return { nullptr, 0, 0 };
    }
    std::cout << "COMPRESSING FILE\nsize: " << size << "\nosize: " << file.size << '\n';
    return { out.release(), (uint32_t)size, file.size };
}

fileinfo uncompress(const cfileinfo file)
{
    size_t size = file.osize;
    auto out = std::make_unique<byte[]>(size);

    int err = zng_uncompress2(out.get(), &size, file.data, (size_t*)&file.size);

    if (err != Z_OK)
    {
        std::cout << zng_getErrorString(err);
        return { nullptr, 0 };
    }

    return { out.release(), (uint32_t)size };
}

void detail(const fileinfo file)
{
    std::string lenoob((char*)file.data, file.size);

    std::cout   << "size: " << file.size
                << ", data: " << lenoob << '\n';
}

void detail(const cfileinfo file)
{
    std::string lenoob((char*)file.data, file.size);

    std::cout   << "size: " << file.size << ", osize: " << file.osize
                << ", data: " << (char*)(file.data + '\0') << '\n';
}

int write_file(const char* path, const fileinfo file)
{
    fs::path dir = fs::path(path).parent_path();

    if (dir != "" && !fs::exists(dir))
    {
        fs::create_directories(dir);
    }

    std::ofstream fstream{ path, std::ios::binary };

    if (!fstream) return 1;

    fstream.write((char*)file.data, file.size);

    fstream.close();

    return 0;
}

int write_file(const char* path, const cfileinfo file)
{
    fs::path dir = fs::path(path).parent_path();

    if (dir != "" && !fs::exists(dir))
    {
        fs::create_directories(dir);
    }

    std::ofstream fstream{ path, std::ios::binary };

    if (!fstream) return 1;
    
    fstream.write(reinterpret_cast<const char*>(&file.osize), 4);

    fstream.write((char*)(file.data), file.size);

    fstream.close();

    return 0;
}

fileinfo read_file(const char* path)
{
    std::ifstream fstream{ path, std::ios::binary };

    if (!fstream) 
    {
        return { nullptr, 0 };
    }

    fstream.seekg(0, std::ios::end);

    uint32_t size = fstream.tellg();

    byte* out = new byte[size];

    fstream.seekg(0, std::ios::beg);

    fstream.read((char*)out, size);

    fstream.close();
    
    return { out, size };
}

cfileinfo read_cfile(const char* path)
{
    std::ifstream fstream{ path, std::ios::binary };

    if (!fstream)
    {
        return { nullptr, 0, 0 };
    }

    fstream.seekg(0, std::ios::end);

    uint32_t size = fstream.tellg();
    size -= 4;

    uint32_t osize;

    byte* out = new byte[size];

    fstream.seekg(0, std::ios::beg);

    fstream.read((char*)&osize, 4);

    fstream.read((char*)out, size);

    fstream.close();

    std::cout << "READ CFILE\n" << path << "\nsize: " << size << "\nosize: "  << osize << '\n';
    return { out, size, osize };
}

void append_bytes(std::vector<byte> &vector, std::string data)
{
    const byte* out = reinterpret_cast<byte*>(data.data());
    vector.insert(vector.end(), out, out + data.size());
}

void append_bytes(std::vector<byte> &vector, byte* data, size_t size)
{
    vector.insert(vector.end(), data, data + size);
}

int write_pack(const char* path)
{
    std::vector<byte> header;
    std::vector<byte> blob;

    uint32_t offset = 0;
    
    for (const auto& i: fs::recursive_directory_iterator(path))
    {
        if (!i.is_regular_file()) continue;

        std::string strpath = i.path().string();
        uint32_t pathsize = strpath.length();

        fileinfo file = read_file(strpath.c_str());   

        cfileinfo cfile = compress(file);

        std::cout << strpath << '\n';

        append_bytes(header, offset);
        append_bytes(header, pathsize);
        append_bytes(header, strpath);

        append_bytes(blob, cfile.size);
        append_bytes(blob, cfile.osize);
        append_bytes(blob, cfile.data, cfile.size);

        offset += (4 * 2) + cfile.size;
    }

    uint32_t hsize = header.size();
    uint32_t bsize = blob.size();

    std::ofstream fstream { "game.bin", std::ios::binary };

    if (!fstream) return 1;

    fstream.write(reinterpret_cast<const char*>(&hsize), 4);
    fstream.write(reinterpret_cast<const char*>(&bsize), 4);
    
    fstream.write(reinterpret_cast<const char*>(header.data()), hsize);
    fstream.write(reinterpret_cast<const char*>(blob.data()), bsize);

    return 0;
}

/*
pack format:
first header:
4 bytes header size
4 bytes blob size

second header:
4 bytes file offset in blob
4 bytes path size           ( I LOVE uint32_t )
plain text path (variable size ofc)
repeat for each file

blob:
4 bytes file size
4 bytes original file size
data
repeat for each file
*/

void read_pack_header(const char* path)
{
    std::ifstream fstream{ path, std::ios::binary };
    
    if (!fstream) return;

    uint32_t offset;
    uint32_t blobsize;

    fstream.read((char*)&offset, 4);
    fstream.read((char*)&blobsize, 4);

    res::pack.offset = offset;
    res::pack.blobsize = blobsize;    
    int ass = 45;

    std::cout << "READING...\noffset:" << offset << "\nblobsize:" << blobsize << '\n';

    int position = 0;

    while (position < offset)
    {
        rfileinfo rfile;
        uint32_t path_size;
        
        fstream.read((char*)&rfile.offset, 4);
        fstream.read((char*)&path_size, 4);

        char* fpath = new char[path_size];

        fstream.read(fpath, path_size);
        fpath[path_size] = '\0';

        std::string fpath_string(fpath);

        res::pack.files[fpath] = rfile;
        
        delete[] fpath;

        position += (sizeof(uint32_t) * 2) + path_size;
    }
}

fileinfo read_pack_file(std::string path)
{
    if (res::pack.files.find(path) == res::pack.files.end()) return { nullptr, 0 };
    rfileinfo* rfile = &res::pack.files[path];

    std::ifstream fstream{ "game.bin", std::ios::binary };

    if (!fstream) return { nullptr, 0 };

    fstream.seekg(res::pack.offset + 8 + rfile->offset, std::ios::beg);

    cfileinfo cfile;

    fstream.read((char*)&cfile.size, 4);
    fstream.read((char*)&cfile.osize, 4);

    cfile.data = new byte[cfile.size];

    fstream.read((char*)cfile.data, cfile.size);

    fstream.close();

    fileinfo file = uncompress(cfile);
    delete[] cfile.data;

    return file;
}

void dump_pack()
{
    for (const auto& i: res::pack.files)
    {
        std::string path = i.first;
        fileinfo file = read_pack_file(path);
        path = "out/" + path;
        write_file(path.c_str(),file);
    }
}