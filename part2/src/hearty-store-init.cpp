#include <iostream>
#include <fstream>
#include <vector>
#include <filesystem>
#include <string>
#include <cstring>
#include "hearty-store-common.hpp"

class StoreInitializer {
private:
    std::string base_path;
    StoreMetadata store_metadata;
    std::vector<BlockMetadata> block_metadata;

    bool createDataFile(const std::string& path) {
        std::ofstream file(path, std::ios::binary);
        if (!file) {
            std::cerr << "Failed to create data file" << std::endl;
            return false;
        }

        // Initialize with zeros
        std::vector<char> zeros(BLOCK_SIZE, 0);
        for (size_t i = 0; i < NUM_BLOCKS; i++) {
            if (!file.write(zeros.data(), BLOCK_SIZE)) {
                std::cerr << "Failed to initialize block " << i << std::endl;
                return false;
            }
        }
        return true;
    }

    bool createMetadataFile(const std::string& path) {
        std::ofstream file(path, std::ios::binary);
        if (!file) {
            std::cerr << "Failed to create metadata file" << std::endl;
            return false;
        }

        // Write store metadata
        file.write(reinterpret_cast<char*>(&store_metadata), sizeof(StoreMetadata));

        // Write block metadata
        for (const auto& block : block_metadata) {
            file.write(reinterpret_cast<const char*>(&block), sizeof(BlockMetadata));
        }

        return true;
    }

    bool initializeMetadata(int store_id) {
        // Initialize store metadata
        store_metadata.store_id = store_id;
        store_metadata.total_blocks = NUM_BLOCKS;
        store_metadata.block_size = BLOCK_SIZE;
        store_metadata.used_blocks = 0;
        store_metadata.is_replica = false;
        store_metadata.replica_of = -1;
        store_metadata.ha_group_id = -1;
        store_metadata.is_destroyed = false;

        // Initialize block metadata
        block_metadata.resize(NUM_BLOCKS);
        for (auto& block : block_metadata) {
            block.is_used = false;
            block.data_size = 0;
            block.timestamp = 0;
        }

        return true;
    }

public:
    StoreInitializer() {}

    bool initialize(int store_id) {
        // Check if store already exists
        std::string store_path = BASE_PATH + STORE_DIR + std::to_string(store_id);
        if (utils::storeExists(store_id)) {
            std::cerr << "Store " << store_id << " already exists" << std::endl;
            return false;
        }

        // Create store directory structure
        if (!std::filesystem::exists(store_path)) {
            try {
                std::filesystem::create_directories(store_path);
            } catch (const std::filesystem::filesystem_error& e) {
                std::cerr << "Failed to create store directory: " << e.what() << std::endl;
                return false;
            }
        }

        // Initialize metadata structures
        if (!initializeMetadata(store_id)) {
            return false;
        }

        // Create data and metadata files
        std::string data_file = store_path + DATA_FILENAME;
        std::string metadata_file = store_path + META_FILENAME;

        if (!createDataFile(data_file)) {
            return false;
        }

        if (!createMetadataFile(metadata_file)) {
            // Cleanup data file if metadata creation fails
            std::filesystem::remove(data_file);
            return false;
        }

        return true;
    }
};

int main(int argc, char* argv[]) {
    // Check command usages 
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " [store-id]" << std::endl;
        return 1;
    }

    // Check input format
    try {
        int store_id = std::stoi(argv[1]);
        if (store_id < 0) {
            std::cerr << "Store ID must be non-negative" << std::endl;
            return 1;
        }

        // Check initialization
        StoreInitializer initializer;
        if (!initializer.initialize(store_id)) {
            std::cerr << "Failed to initialize store " << store_id << std::endl;
            return 1;
        }

        std::cout << "Successfully initialized store " << store_id << std::endl;
        return 0;

    } catch (const std::invalid_argument&) {
        std::cerr << "Invalid store ID format" << std::endl;
        return 1;
    }
}