/**
 * @file hearty-store-replicate.cpp
 * @author Nathadon Samairat
 * @brief 
 * @version 0.1
 * @date 2024-11-27
 * 
 * @copyright Copyright (c) 2024
 * 
 */
#include <iostream>
#include <fstream>
#include <random>
#include "hearty-store-common.hpp"

class StoreReplicate {
private:
    /**
     * @brief Generates a unique ID for a new replica store.
     * 
     * @return A new unique store ID.
     */
    int generateNewStoreId() {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(1000, 9999);
        
        // Random select store id if matched with existing then do it again
        int new_id;
        do {
            new_id = dis(gen);
        } while (utils::storeExists(new_id));
        
        return new_id;
    }

    /**
     * @brief Copies data from the source store to the replica store.
     * 
     * @param source_id ID of the source store.
     * @param replica_id ID of the replica store.
     * 
     * @return true if the data copy is successful; false otherwise.
     */
    bool copyStoreData(int source_id, int replica_id) {
        std::ifstream src(utils::getDataPath(source_id), std::ios::binary);
        std::ofstream dst(utils::getDataPath(replica_id), std::ios::binary);
        
        if (!src || !dst) {
            std::cerr << "Failed to open data files" << std::endl;
            return false;
        }

        // Copy in chunks to handle large files efficiently
        constexpr size_t CHUNK_SIZE = 8192;
        std::vector<char> buffer(CHUNK_SIZE);
        
        while (src) {
            src.read(buffer.data(), buffer.size());
            std::streamsize bytes_read = src.gcount();
            if (bytes_read > 0) {
                dst.write(buffer.data(), bytes_read);
                if (!dst) {
                    std::cerr << "Failed to write data" << std::endl;
                    return false;
                }
            }
        }

        return true;
    }

    /**
     * @brief Updates the metadata of the source store to link it to the replica.
     * 
     * @param source_id ID of the source store.
     * @param replica_id ID of the replica store.
     * 
     * @return true if the metadata update is successful; false otherwise.
     */
    bool updateSourceMetadata(int source_id, int replica_id) {
        StoreMetadata metadata;
        std::fstream file(utils::getMetadataPath(source_id), 
                         std::ios::binary | std::ios::in | std::ios::out);
        
        if (!file) return false;
        
        file.read(reinterpret_cast<char*>(&metadata), sizeof(StoreMetadata));
        if (metadata.is_replica || metadata.replica_of != -1) {
            std::cerr << "Store is already part of a replica pair" << std::endl;
            return false;
        }
        
        metadata.replica_of = replica_id;
        
        file.seekp(0);
        file.write(reinterpret_cast<char*>(&metadata), sizeof(StoreMetadata));
        
        return true;
    }

    /**
     * @brief Creates metadata for the replica store.
     * 
     * @param source_id ID of the source store.
     * @param replica_id ID of the replica store.
     * 
     * @return true if the metadata creation is successful; false otherwise.
     */
    bool createReplicaMetadata(int source_id, int replica_id) {
        // First read source metadata
        StoreMetadata source_metadata;
        {
            std::ifstream src(utils::getMetadataPath(source_id), std::ios::binary);
            if (!src) return false;
            src.read(reinterpret_cast<char*>(&source_metadata), sizeof(StoreMetadata));
        }

        // Create and initialize replica metadata
        StoreMetadata replica_metadata = source_metadata;
        replica_metadata.store_id = replica_id;
        replica_metadata.is_replica = true;
        replica_metadata.replica_of = source_id;

        // Write replica metadata
        std::ofstream dst(utils::getMetadataPath(replica_id), std::ios::binary);
        if (!dst) return false;
        
        dst.write(reinterpret_cast<char*>(&replica_metadata), sizeof(StoreMetadata));

        // Copy block metadata
        std::ifstream src(utils::getMetadataPath(source_id), std::ios::binary);
        src.seekg(sizeof(StoreMetadata));  // Skip the store metadata we already handled

        char buffer[1024];
        while (src) {
            src.read(buffer, sizeof(buffer));
            std::streamsize bytes_read = src.gcount();
            if (bytes_read > 0) {
                dst.write(buffer, bytes_read);
            }
        }

        return true;
    }

    /**
     * @brief Creates directories for the replica store.
     * 
     * @param replica_id ID of the replica store.
     * 
     * @return true if the directories are created successfully; false otherwise.
     */
    bool createReplicaDirectories(int replica_id) {
        try {
            std::filesystem::create_directories(utils::getStorePath(replica_id));
            return true;
        } catch (const std::filesystem::filesystem_error& e) {
            std::cerr << "Failed to create directories: " << e.what() << std::endl;
            return false;
        }
    }

public:
    /**
     * @brief Creates a replica for the specified source store.
     * 
     * @param source_id ID of the source store.
     * 
     * @return The ID of the newly created replica store, or -1 on failure.
     */
    int replicate(int source_id) {
        // Verify source store exists
        std::string store_path = BASE_PATH + STORE_DIR + std::to_string(source_id);
        if (!std::filesystem::exists(store_path)) {
            std::cerr << "Source store " << source_id << " does not exist" << std::endl;
            return -1;
        }

        // Generate new store ID for replica
        int replica_id = generateNewStoreId();

        // Create directories for replica
        if (!createReplicaDirectories(replica_id)) {
            return -1;
        }

        // Copy store data
        if (!copyStoreData(source_id, replica_id)) {
            std::filesystem::remove_all(utils::getStorePath(replica_id));
            return -1;
        }

        // Create replica metadata
        if (!createReplicaMetadata(source_id, replica_id)) {
            std::filesystem::remove_all(utils::getStorePath(replica_id));
            return -1;
        }

        // Update source metadata
        if (!updateSourceMetadata(source_id, replica_id)) {
            std::filesystem::remove_all(utils::getStorePath(replica_id));
            return -1;
        }

        return replica_id;
    }
};

int main(int argc, char* argv[]) {
    // Check command usages 
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " [store-id]" << std::endl;
        return 1;
    }

    try {
        int source_id = std::stoi(argv[1]);
        
        StoreReplicate replicator;
        int replica_id = replicator.replicate(source_id);
        
        if (replica_id == -1) {
            std::cerr << "Failed to create replica" << std::endl;
            return 1;
        }

        // Output the replica store ID
        std::cout << replica_id << std::endl;
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}