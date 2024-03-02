#pragma once

#include <iostream>
#include <fstream>
#include <iterator>
#include <sstream>
#include <vector>

namespace File{
    static bool BufferToDisk(const std::vector<unsigned char>& buffer, const std::string& filename){
        std::ofstream outfile(filename, std::ios::out | std::ios::binary);

        if (outfile.is_open()) {
            outfile.write(reinterpret_cast<const char*>(buffer.data()), buffer.size());
            outfile.close();
        } else {
            return false;
        }
        return true;
    }

    static std::vector<unsigned char> DiskToBuffer(const std::string& filename) {
        std::vector<unsigned char> buffer;
        std::ifstream infile(filename, std::ios::in | std::ios::binary);

        if (infile.is_open()) {
            infile.seekg(0, std::ios::end);
            std::streampos fileSize = infile.tellg();
            infile.seekg(0, std::ios::beg);

            buffer.resize(fileSize);

            infile.read(reinterpret_cast<char*>(buffer.data()), fileSize);

            infile.close();
        }

        return buffer;
    }
}

namespace String {
    static std::vector<unsigned char> StringToBytes(const std::string& str){
        return std::vector<unsigned char>(str.begin(), str.end());
    }
}

/* Not my implementation */
namespace B64 {
        static std::string base64_encode(const std::string& in) {

            std::string out;

            int val = 0, valb = -6;
            for (unsigned char c : in) {
                val = (val << 8) + c;
                valb += 8;
                while (valb >= 0) {
                    out.push_back("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[(val >> valb) & 0x3F]);
                    valb -= 6;
                }
            }
            if (valb > -6) out.push_back("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[((val << 8) >> (valb + 8)) & 0x3F]);
            while (out.size() % 4) out.push_back('=');
            return out;
        }

        static std::string base64_decode(const std::string& in) {

            std::string out;

            std::vector<int> T(256, -1);
            for (int i = 0; i < 64; i++) T["ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[i]] = i;

            int val = 0, valb = -8;
            for (unsigned char c : in) {
                if (T[c] == -1) break;
                val = (val << 6) + T[c];
                valb += 6;
                if (valb >= 0) {
                    out.push_back(char((val >> valb) & 0xFF));
                    valb -= 8;
                }
            }
            return out;
        }
    }
