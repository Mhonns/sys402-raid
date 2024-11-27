// hearty-store-get.cpp
#include <iostream>
#include <fstream>
#include <algorithm>
#include "hearty-store-common.hpp"

class StoreGet {
private:
    int store_id;
    StoreMetadata store_metadata;
    std::vector<BlockMetadata> block_metadata;

    bool loadMetadata() {
        std::ifstream file(utils::getMetadataPath(store_id), std::ios::binary);
        if (!file) {
            std::cerr << "Failed to open metadata file" << std::endl;
            return false;
        }

        file.read(reinterpret_cast<char*>(&store_metadata), sizeof(StoreMetadata));
        
        block_metadata.resize(NUM_BLOCKS);
        for (auto& block : block_metadata) {
            file.read(reinterpret_cast<char*>(&block), sizeof(BlockMetadata));
        }

        return true;
    }

    int findBlockByObjectId(const std::string& object_id) {
        for (size_t i = 0; i < NUM_BLOCKS; i++) {
            if (block_metadata[i].is_used && block_metadata[i].object_id == object_id) {
                return i;
            }
        }
        return -1;
    }

    bool reconstructFromParity(int block_num, std::ostream& out) {
        if (store_metadata.ha_group_id == -1) {
            return false;
        }

        // TODO: Implement parity reconstruction
        // 1. Read corresponding blocks from other stores in HA group
        // 2. Read parity block
        // 3. Reconstruct data using XOR operations
        // 4. Write reconstructed data to output stream

        return false;  // Not implemented yet
    }

    bool readFromReplica(const std::string& object_id, std::ostream& out) {
        if (store_metadata.replica_of == -1 && !store_metadata.is_replica) {
            return false;
        }

        // TODO: Implement replica reading
        // 1. Determine replica store ID
        // 2. Load replica's metadata
        // 3. Find block in replica
        // 4. Read data from replica

        return false;  // Not implemented yet
    }

    bool readBlock(int block_num, std::ostream& out) {
        std::ifstream data_file(utils::getDataPath(store_id), std::ios::binary);
        if (!data_file) {
            std::cerr << "Failed to open data file" << std::endl;
            return false;
        }

        // Seek to the correct block
        data_file.seekg(block_num * BLOCK_SIZE);

        // Read only the actual data size, not the entire block
        std::vector<char> buffer(block_metadata[block_num].data_size);
        data_file.read(buffer.data(), block_metadata[block_num].data_size);

        if (!data_file) {
            std::cerr << "Failed to read data" << std::endl;
            return false;
        }

        // Write to output stream
        out.write(buffer.data(), block_metadata[block_num].data_size);
        return true;
    }

public:
    StoreGet(int id) : store_id(id) {}

    bool get(const std::string& object_id, std::ostream& out) {
        if (!loadMetadata()) {
            return false;
        }

        // Check if store is destroyed
        if (store_metadata.is_destroyed) {
            // Try to reconstruct from parity or read from replica
            int block_num = findBlockByObjectId(object_id);
            if (block_num != -1) {
                if (reconstructFromParity(block_num, out)) {
                    return true;
                }
                if (readFromReplica(object_id, out)) {
                    return true;
                }
            }
            std::cerr << "Store is destroyed and reconstruction failed" << std::endl;
            return false;
        }

        // Find the block containing our object
        int block_num = findBlockByObjectId(object_id);
        if (block_num == -1) {
            std::cerr << "Object not found: " << object_id << std::endl;
            return false;
        }

        // Read and output the data
        return readBlock(block_num, out);
    }
};

int main(int argc, char* argv[]) {
    // Check command usages 
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " [store-id] [object-id]" << std::endl;
        return 1;
    }

    try {
        int store_id = std::stoi(argv[1]);
        std::string object_id = argv[2];

        // Check if store exists
        if (!std::filesystem::exists(utils::getStorePath(store_id))) {
            std::cerr << "Store " << store_id << " does not exist" << std::endl;
            return 1;
        }

        StoreGet store_get(store_id);
        if (!store_get.get(object_id, std::cout)) {
            return 1;
        }

        std::cerr << "Successfully get the object " << object_id << std::endl;
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}