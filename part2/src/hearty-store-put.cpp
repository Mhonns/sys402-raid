#include <iostream>
#include <fstream>
#include <string>
#include <filesystem>
#include <vector>
#include <random>
#include <chrono>
#include <cstring>
#include "hearty-store-common.hpp"

class StorePut {
private:
    int store_id;
    StoreMetadata store_metadata;
    std::vector<BlockMetadata> block_metadata;

    std::string generateUniqueId() {
        // Generate a random ID using timestamp and random number
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()
        ).count();
        
        std::random_device rd;      // Random device
        std::mt19937 gen(rd());     // RNG initialized with the seed from 'rd'
        // Generate random integers between 1000 and 9999 (inclusive)
        std::uniform_int_distribution<> dis(1000, 9999); 
        
        return std::to_string(timestamp) + "_" + std::to_string(dis(gen));
    }

    bool loadMetadata() {
        std::ifstream file(utils::getMetadataPath(store_id), std::ios::binary);
        if (!file) {
            std::cerr << "Failed to open metadata file" << std::endl;
            return false;
        }

        // Read store metadata
        file.read(reinterpret_cast<char*>(&store_metadata), sizeof(StoreMetadata));
        
        // Read block metadata
        block_metadata.resize(NUM_BLOCKS);
        for (auto& block : block_metadata) {
            file.read(reinterpret_cast<char*>(&block), sizeof(BlockMetadata));
        }

        return true;
    }

    bool saveMetadata() {
        std::ofstream file(utils::getMetadataPath(store_id), std::ios::binary);
        if (!file) {
            std::cerr << "Failed to open metadata file for writing" << std::endl;
            return false;
        }

        // Write store metadata
        file.write(reinterpret_cast<char*>(&store_metadata), sizeof(StoreMetadata));
        
        // Write block metadata
        for (const auto& block : block_metadata) {
            file.write(reinterpret_cast<const char*>(&block), sizeof(BlockMetadata));
        }
        file.close();

        return true;
    }

    int findFreeBlock() {
        for (size_t i = 0; i < NUM_BLOCKS; i++) {
            if (!block_metadata[i].is_used) {
                return i;
            }
        }
        return -1;
    }

    bool writeToBlock(const std::string& file_path, int block_num, const std::string& object_id) {
        std::ifstream input_file(file_path, std::ios::binary);
        if (!input_file) {
            std::cerr << "Failed to open input file" << std::endl;
            return false;
        }

        std::fstream data_file(utils::getDataPath(store_id), std::ios::binary | std::ios::in | std::ios::out);
        if (!data_file) {
            std::cerr << "Failed to open data file" << std::endl;
            return false;
        }

        // Seek to the correct block
        data_file.seekp(block_num * BLOCK_SIZE);

        // Read input file and write to block
        std::vector<char> buffer(BLOCK_SIZE);
        input_file.read(buffer.data(), BLOCK_SIZE);
        size_t bytes_read = input_file.gcount();

        data_file.write(buffer.data(), bytes_read);

        // Update metadata
        block_metadata[block_num].is_used = true;
        block_metadata[block_num].object_id = object_id;
        block_metadata[block_num].data_size = bytes_read;
        block_metadata[block_num].timestamp = std::time(nullptr);
        store_metadata.used_blocks++;

        input_file.close();
        return true;
    }

    bool updateParity() {
        if (store_metadata.ha_group_id == -1) {
            return true;  // Not part of HA group
        }
        
        // TODO Implement parity update logic here
        // This would involve reading corresponding blocks from other stores
        // and updating the parity file
        
        return true;
    }

    bool syncWithReplica() {
        if (!store_metadata.is_replica && store_metadata.replica_of == -1) {
            return true;  // Not part of a replica pair
        }

        int related_id = store_metadata.replica_of; 
        
        // Open related store's data file
        std::string target_path = utils::getDataPath(related_id);
        std::fstream target_file(target_path, std::ios::binary | std::ios::in | std::ios::out);
        if (!target_file) {
            std::cerr << "Failed to open replica store data file" << std::endl;
            return false;
        }

        // Open source store's data file
        std::string source_path = utils::getDataPath(store_metadata.store_id);
        std::ifstream source_file(source_path, std::ios::binary);
        if (!source_file) {
            std::cerr << "Failed to open source store data file" << std::endl;
            return false;
        }

        // Copy block by block to ensure atomic updates
        const size_t BUFFER_SIZE = BLOCK_SIZE;
        std::vector<char> buffer(BUFFER_SIZE);

        for (size_t block = 0; block < NUM_BLOCKS; block++) {
            // Read block from source
            source_file.seekg(block * BLOCK_SIZE);
            source_file.read(buffer.data(), BUFFER_SIZE);
            size_t bytes_read = source_file.gcount();

            // Write block to target
            target_file.seekp(block * BLOCK_SIZE);
            target_file.write(buffer.data(), bytes_read);
            
            if (!target_file) {
                std::cerr << "Failed to write to replica at block " << block << std::endl;
                return false;
            }
        }

        // Sync metadata
        std::string source_meta_path = utils::getMetadataPath(store_metadata.store_id);
        std::string target_meta_path = utils::getMetadataPath(related_id);
        std::ifstream source_meta(source_meta_path, std::ios::binary);
        std::ofstream target_meta(target_meta_path, std::ios::binary | std::ios::trunc);
        if (!source_meta || !target_meta) {
            std::cerr << "Failed to open metadata files for sync" << std::endl;
            return false;
        }

        // Read and adjust source metadata
        StoreMetadata target_metadata;
        source_meta.read(reinterpret_cast<char*>(&target_metadata), sizeof(StoreMetadata));
        // Preserve the replica relationship while updating other fields
        if (store_metadata.is_replica) {
            // If we're the replica, the target is the original
            target_metadata.store_id = related_id;
            target_metadata.is_replica = false;
            target_metadata.replica_of = store_metadata.store_id;
        } else {
            // If we're the original, the target is the replica
            target_metadata.store_id = related_id;
            target_metadata.is_replica = true;
            target_metadata.replica_of = store_metadata.store_id;
        }
        target_meta.write(reinterpret_cast<char*>(&target_metadata), sizeof(StoreMetadata));

        // Ensure everything is written
        target_file.flush();
        target_meta.flush();

        if (!target_file || !target_meta) {
            std::cerr << "Failed to sync replica" << std::endl;
            return false;
        }

        return true;
    }

public:
    StorePut(int id) : store_id(id) {}

    std::string put(const std::string& file_path) {
        // Check if store exists and load metadata
        if (!loadMetadata()) {
            return "";
        }

        // Check if store is destroyed
        if (store_metadata.is_destroyed) {
            std::cerr << "Store is destroyed" << std::endl;
            return "";
        }

        // Check file size
        uintmax_t file_size = std::filesystem::file_size(file_path);
        if (file_size > BLOCK_SIZE) {
            std::cerr << "File too large (max 1MB)" << std::endl;
            return "";
        }

        // Find free block
        int block_num = findFreeBlock();
        if (block_num == -1) {
            std::cerr << "No free blocks available" << std::endl;
            return "";
        }

        // Generate unique ID
        std::string object_id = generateUniqueId();

        // Write file to block
        if (!writeToBlock(file_path, block_num, object_id)) {
            return "";
        }

        // Save updated metadata
        if (!saveMetadata()) {
            return "";
        }

        // Update parity if part of HA group
        if (!updateParity()) {
            std::cerr << "Warning: Failed to update parity" << std::endl;
        }

        // Sync with replica if necessary
        if (!syncWithReplica()) {
            std::cerr << "Warning: Failed to sync with replica" << std::endl;
        }

        return object_id;
    }
};

int main(int argc, char* argv[]) {
    // Check command usages 
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " [store-id] [file-path]" << std::endl;
        return 1;
    }

    try {
        int store_id = std::stoi(argv[1]);
        std::string file_path = argv[2];

        // Check if file exists
        if (!std::filesystem::exists(file_path)) {
            std::cerr << "File does not exist: " << file_path << std::endl;
            return 1;
        }

        StorePut store_put(store_id);
        std::string object_id = store_put.put(file_path);
        
        if (object_id.empty()) {
            std::cerr << "Failed to store file" << std::endl;
            return 1;
        }

        // Output the object ID to stdout
        std::cout << "Successfully put object id " << object_id 
                    << " into " << store_id << std::endl;
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}