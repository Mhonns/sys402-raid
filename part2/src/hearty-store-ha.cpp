/**
 * @file hearty-store-ha.cpp
 * @author Nathadon Samairat
 * @brief Manages the creation of High Availability (HA) groups for stores, 
 *          including metadata management, parity file creation, and store validation.
 * @version 0.1
 * @date 2024-11-28
 * 
 * @copyright Copyright (c) 2024
 * 
 */
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <set>
#include "hearty-store-common.hpp"

std::vector<std::vector<int>> ha_group_counts(NUM_BLOCKS, std::vector<int>(NUM_BLOCKS, 0));

class StoreHA {
private:
    /**
     * @brief Loads the metadata for a given store.
     * 
     * @param store_id ID of the store.
     * @param metadata Reference to the metadata object to load.
     * 
     * @return true if metadata is successfully loaded; false otherwise.
     */
    bool loadStoreMetadata(int store_id, StoreMetadata& metadata) {
        std::ifstream file(utils::getMetadataPath(store_id), std::ios::binary);
        if (!file) return false;
        file.read(reinterpret_cast<char*>(&metadata), sizeof(StoreMetadata));
        return true;
    }

    /**
     * @brief Saves the metadata for a given store.
     * 
     * @param store_id ID of the store.
     * @param metadata Reference to the metadata object to save.
     * 
     * @return true if metadata is successfully saved; false otherwise.
     */
    bool saveStoreMetadata(int store_id, const StoreMetadata& metadata) {
        std::ofstream file(utils::getMetadataPath(store_id), std::ios::binary | std::ios::trunc);
        if (!file) return false;
        file.write(reinterpret_cast<const char*>(&metadata), sizeof(StoreMetadata));
        return true;
    }

    /**
     * @brief Creates a parity file for an HA group.
     * 
     * @param parity_path Path to the parity file.
     * 
     * @return true if the parity file is successfully created; false otherwise.
     */
    bool createParityFile(const std::string& parity_path) {
        std::ofstream parity(parity_path, std::ios::binary);
        if (!parity) return false;

        // Initialize parity file with zeros
        std::vector<char> zeros(BLOCK_SIZE, 0);
        for (size_t i = 0; i < NUM_BLOCKS; i++) {
            if (!parity.write(zeros.data(), BLOCK_SIZE)) {
                return false;
            }
        }
        return true;
    }

    /**
     * @brief Updates the parity file for the given stores in an HA group.
     * 
     * @param store_ids List of store IDs to include in the parity computation.
     * 
     * @return true if the parity is successfully updated; false otherwise.
     */
    bool updateParity(const std::vector<int>& store_ids) {
        std::string parity_path = BASE_PATH + "/ha_group_" + 
                                 std::to_string(store_ids[0]) + PARITY_FILENAME;

        // Buffers for reading blocks and computing parity
        std::vector<char> parity_buffer(BLOCK_SIZE, 0);
        std::vector<char> block_buffer(BLOCK_SIZE);

        // Process each block
        for (size_t block = 0; block < NUM_BLOCKS; block++) {
            // Reset parity buffer
            std::fill(parity_buffer.begin(), parity_buffer.end(), 0);

            // XOR all blocks from all stores
            for (int store_id : store_ids) {
                std::ifstream store_file(utils::getDataPath(store_id), std::ios::binary);
                if (!store_file) return false;

                // Seek to current block
                store_file.seekg(block * BLOCK_SIZE);
                store_file.read(block_buffer.data(), BLOCK_SIZE);

                // XOR into parity buffer
                for (size_t i = 0; i < BLOCK_SIZE; i++) {
                    parity_buffer[i] ^= block_buffer[i];
                }
            }

            // Write parity block
            std::fstream parity_file(parity_path, 
                                   std::ios::binary | std::ios::in | std::ios::out);
            if (!parity_file) return false;

            parity_file.seekp(block * BLOCK_SIZE);
            parity_file.write(parity_buffer.data(), BLOCK_SIZE);
        }

        return true;
    }

    /**
     * @brief Validates the given stores for inclusion in an HA group.
     * 
     * @param store_ids List of store IDs to validate.
     * 
     * @return true if all stores are valid; false otherwise.
     */
    bool validateStores(const std::vector<int>& store_ids) {
        std::set<int> unique_ids(store_ids.begin(), store_ids.end());
        
        // Check for duplicates
        if (unique_ids.size() != store_ids.size()) {
            std::cerr << "Duplicate store IDs are not allowed" << std::endl;
            return false;
        }

        for (int store_id : store_ids) {
            if (!utils::storeExists(store_id)) {
                std::cerr << "Store " << store_id << " does not exist" << std::endl;
                return false;
            }

            StoreMetadata metadata;
            if (!loadStoreMetadata(store_id, metadata)) {
                std::cerr << "Failed to load metadata for store " << store_id << std::endl;
                return false;
            }

            if (metadata.ha_group_id != -1) {
                std::cerr << "Store " << store_id << " is already part of HA group " 
                         << metadata.ha_group_id << std::endl;
                return false;
            }

            if (metadata.is_replica || metadata.replica_of != -1) {
                std::cerr << "Store " << store_id << " is part of a replica pair" << std::endl;
                return false;
            }
        }

        return true;
    }

public:
    /**
     * @brief Creates a High Availability (HA) group from the given stores.
     * 
     * @param store_ids List of store IDs to include in the HA group.
     * 
     * @return true if the HA group is successfully created; false otherwise.
     */
    bool createHAGroup(const std::vector<int>& store_ids) {
        
        // Validate all stores
        if (!validateStores(store_ids)) {
            return false;
        }

        // Create parity directory structure
        std::string ha_path = utils::getHAPath(store_ids[0]);
        if (!std::filesystem::exists(ha_path)) {
            try {
                std::filesystem::create_directories(ha_path);
            } catch (const std::filesystem::filesystem_error& e) {
                std::cerr << "Failed to create store directory: " << e.what() << std::endl;
                return false;
            }
        }

        // Create parity file
        std::string full_parity_path = ha_path + PARITY_FILENAME;
        if (!createParityFile(full_parity_path)) {
            std::cerr << "Failed to create parity file" << std::endl;
            return false;
        }

        // Calculate initial parity
        if (!updateParity(store_ids)) {
            std::cerr << "Failed to calculate initial parity" << std::endl;
            std::filesystem::remove(ha_path);
            return false;
        }

        // Update metadata for all stores
        for (int store_id : store_ids) {
            StoreMetadata metadata;
            if (!loadStoreMetadata(store_id, metadata)) {
                continue;  // Skip if can't load metadata
            }

            metadata.ha_group_id = store_ids[0];  // Use first store's ID as group ID
            if (!saveStoreMetadata(store_id, metadata)) {
                std::cerr << "Warning: Failed to update metadata for store " 
                            << store_id << std::endl;
            }
        }
        
        // Update global ha status
        HAGroupStatus status;
        status.group_id = store_ids[0];
        status.store_count = store_ids.size();
        status.destroyed_count = 0;
        std::string ha_filepath = ha_path + "/status.data";
        std::ofstream outFile(ha_filepath, std::ios::binary);
        if (!outFile) {
            std::cerr << "Error opening file for writing!" << std::endl;
            return false;
        }
        outFile.write(reinterpret_cast<const char*>(&status.group_id), sizeof(status.group_id));
        outFile.write(reinterpret_cast<const char*>(&status.store_count), sizeof(status.store_count));
        outFile.write(reinterpret_cast<const char*>(&status.destroyed_count), sizeof(status.destroyed_count));
        for (int store_id : store_ids) {
            outFile.write(reinterpret_cast<const char*>(&store_id), sizeof(store_id));
        }
        outFile.close();

        return true;
    }
};

int main(int argc, char* argv[]) {
    // Check command usages 
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " [store-id1] [store-id2] ..." << std::endl;
        return 1;
    }

    try {
        std::vector<int> store_ids;
        for (int i = 1; i < argc; i++) {
            store_ids.push_back(std::stoi(argv[i]));
        }

        StoreHA ha_manager;
        if (!ha_manager.createHAGroup(store_ids)) {
            return 1;
        }

        std::cout << "Successfully created HA group with ID " << store_ids[0] << std::endl;
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}