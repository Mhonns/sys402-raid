/**
 * @file hearty-store-get.cpp
 * @author Nathadon Samairat
 * @brief This program provides functionality to retrieve objects 
 *        from a distributed storage system. It handles metadata loading, 
 *        block reconstruction using parity, and direct block reading.
 * @version 0.1
 * @date 2024-11-27
 * 
 * @copyright Copyright (c) 2024
 */
#include <iostream>
#include <fstream>
#include <algorithm>
#include "hearty-store-common.hpp"

class StoreGet {
private:
    int store_id;
    StoreMetadata store_metadata;
    std::vector<BlockMetadata> block_metadata;

    /**
     * @brief Load metadata for the store, including store and block metadata.
     * 
     * @return true     - Metadata loaded successfully.
     * @return false    - Failed to load metadata.
     */
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

    /**
     * @brief Locate the block containing the specified object ID.
     * 
     * @param object_id     - Target object ID to find in the store.
     * @return int          - Index of the block if found; -1 if not found.
     */
    int findBlockByObjectId(const std::string& object_id) {
        for (size_t i = 0; i < NUM_BLOCKS; i++) {
            if (block_metadata[i].is_used && block_metadata[i].object_id == object_id) {
                return i;
            }
        }
        return -1;
    }

    /**
     * @brief Reconstruct a block's data using parity and data from surviving stores.
     * 
     * @param block_num     - Block number to reconstruct.
     * @param out           - Output stream to write the reconstructed data.
     * @return true         - Successfully reconstructed the block.
     * @return false        - Failed to reconstruct the block.
     */
    bool reconstructFromParity(int block_num, std::ostream& out) {
        if (store_metadata.ha_group_id == -1) {
            return false;
        }

        // Read HA group status
        std::string ha_status_path = utils::getHAPath(store_metadata.ha_group_id);
        std::ifstream status_file(ha_status_path, std::ios::binary);
        if (!status_file) {
            return false;
        }

        HAGroupStatus ha_status;
        status_file.read(reinterpret_cast<char*>(&ha_status), sizeof(HAGroupStatus));

        // Prepare buffers
        std::vector<char> data_buffer(BLOCK_SIZE, 0);
        std::vector<char> block_buffer(BLOCK_SIZE);

        // Read parity block
        std::string parity_path = ha_status_path + PARITY_FILENAME;
        std::ifstream parity_file(parity_path, std::ios::binary);
        if (!parity_file) {
            return false;
        }

        parity_file.seekg(block_num * BLOCK_SIZE);
        parity_file.read(data_buffer.data(), BLOCK_SIZE);

        // XOR with blocks from surviving stores
        for (int store_id : ha_status.store_ids) {
            if (store_id == store_metadata.store_id) continue; // Skip current store

            // Check if store is active
            std::string store_meta_path = utils::getMetadataPath(store_id);
            std::ifstream store_meta(store_meta_path, std::ios::binary);
            if (!store_meta) continue;

            StoreMetadata other_meta;
            store_meta.read(reinterpret_cast<char*>(&other_meta), sizeof(StoreMetadata));
            if (other_meta.is_destroyed) continue;

            // Read block from this store
            std::ifstream store_file(utils::getDataPath(store_id), std::ios::binary);
            if (!store_file) continue;

            store_file.seekg(block_num * BLOCK_SIZE);
            store_file.read(block_buffer.data(), BLOCK_SIZE);

            // XOR into data buffer
            for (size_t i = 0; i < BLOCK_SIZE; i++) {
                data_buffer[i] ^= block_buffer[i];
            }
        }

        // Write reconstructed data
        out.write(data_buffer.data(), BLOCK_SIZE);

        return true;
    }

    /**
     * @brief Read a block's data from the store and output it.
     * 
     * @param block_num     - Block number to read.
     * @param out           - Output stream to write the block's data.
     * @return true         - Successfully read the block.
     * @return false        - Failed to read the block.
     */
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
    
    /**
     * @brief Retrieve an object by its ID from the store or reconstruct it if necessary.
     * 
     * @param object_id     - ID of the object to retrieve.
     * @param out           - Output stream to write the object's data.
     * @return true         - Successfully retrieved the object.
     * @return false        - Failed to retrieve the object.
     */
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