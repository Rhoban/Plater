#ifndef _PLATER_ZIP_H
#define _PLATER_ZIP_H

#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

namespace Plater
{
    /**
     * Minimal ZIP archive writer. Entries are stored without compression
     * (method 0), which is all that is needed to produce a valid OPC/3MF
     * container without pulling in an external zlib dependency.
     */
    class Zip
    {
        public:
            Zip(const std::string &filename);
            ~Zip();

            // True if the output file was opened successfully.
            bool ok() const;

            // Add an uncompressed entry.
            void add(const std::string &name, const std::string &data);

            // Write the central directory and close the file. Returns success.
            bool close();

        private:
            struct Entry {
                std::string name;
                uint32_t crc;
                uint32_t size;
                uint32_t offset;
            };

            void put16(uint16_t v);
            void put32(uint32_t v);

            std::ofstream out;
            std::vector<Entry> entries;
            uint32_t offset;
            bool closed;
    };
}

#endif
