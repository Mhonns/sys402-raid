/**
 * @file hearty-store-destroy.cpp
 * @author Nathadon Samairat
 * @brief Handles the destruction of stores and related metadata or files.
 * @version 0.1
 * @date 2024-11-27
 * 
 * @copyright Copyright (c) 2024
 */

#include <iostream>
#include <fstream>
#include "hearty-store-common.hpp"

class StoreDestroy {
private:
    /**
     * @brief Loads metadata for the specified store.
     * 
     * @param store_id ID of the store to load metadata for.
     * @param metadata Reference to a metadata structure to populate.
     * 
     * @return true if the metadata is successfully loaded; false otherwise.
     */
    bool loadStoreMetadata(int store_id, StoreMetadata& metadata) {
        std::ifstream file(utils::getMetadataPath(store_id), std::ios::binary);
        if (!file) return false;
        file.read(reinterpret_cast<char*>(&metadata), sizeof(StoreMetadata));
        return true;
    }

    /**
     * @brief Destroys the specified store, handling related stores or HA group data.
     * 
     * @param store_id ID of the store to destroy.
     * @param related_store Indicates whether this is a related store being handled.
     * 
     * @return true if the store is successfully destroyed; false otherwise.
     */
    bool destroyStore(int store_id, bool related_store) {
        StoreMetadata metadata;
        if (!loadStoreMetadata(store_id, metadata)) {
            std::cerr << "Failed to load store metadata" << std::endl;
            return false;
        }

        // Handle HA group
        if (metadata.ha_group_id != -1) {
            // Mark as destroyed but don't remove files
            metadata.is_destroyed = true;
            std::ofstream meta_file(utils::getMetadataPath(store_id), std::ios::binary);
            if (!meta_file.write(reinterpret_cast<char*>(&metadata), 
                               sizeof(StoreMetadata))) {
                std::cerr << "Failed to update metadata" << std::endl;
                return false;
            }

            // Get the ha group status
            HAGroupStatus status;
            std::string ha_path = utils::getHAPath(metadata.ha_group_id);
            std::string filename = ha_path + "/status.data";
            std::ifstream inFile(filename, std::ios::binary);
            inFile.read(reinterpret_cast<char*>(&status.group_id), sizeof(status.group_id));
            inFile.read(reinterpret_cast<char*>(&status.store_count), sizeof(status.store_count));
            inFile.read(reinterpret_cast<char*>(&status.destroyed_count), sizeof(status.destroyed_count));

            // Update the ha group status
            status.destroyed_count++;
            if (status.destroyed_count > 1) {
                // Delete ha group is more than one was destroyed
                for (int i = 0; i < status.store_count; i++) {
                    // Get metadata for all store in ha
                    int store_id; 
                    inFile.read(reinterpret_cast<char*>(&store_id), sizeof(store_id));
                    StoreMetadata target_metadata = metadata;
                    if (store_id != metadata.store_id) {
                        if (!loadStoreMetadata(store_id, target_metadata)) {
                            std::cerr << "Failed to load store metadata" << std::endl;
                            return false;
                        }
                    } 

                    // Update metadata
                    int temp_ha_group_id = target_metadata.ha_group_id;
                    target_metadata.ha_group_id = -1;
                    std::ofstream file(utils::getMetadataPath(target_metadata.store_id), std::ios::binary);
                    if (!file) {
                        std::cerr << "Failed to open metadata file for writing" << std::endl;
                        return false;
                    }
                    file.write(reinterpret_cast<char*>(&target_metadata), sizeof(StoreMetadata));
                    file.close();

                    // Destroy the store if any
                    if (target_metadata.is_destroyed) {
                        // Remove all files
                        try {
                            std::filesystem::remove_all(utils::getStorePath(store_id));
                        } catch (const std::filesystem::filesystem_error& e) {
                            std::cerr << "Failed to remove store files: " << e.what() << std::endl;
                            return false;
                        }   
                    }

                    // Remove all files
                    std::filesystem::remove_all(utils::getHAPath(temp_ha_group_id));
                }
            } else {
                // Write the update for ha status
                std::string ha_filepath = ha_path + "/status.data";
                std::ofstream outFile(ha_filepath, std::ios::binary);
                if (!outFile) {
                    std::cerr << "Error opening file for writing!" << std::endl;
                    return false;
                }
                outFile.write(reinterpret_cast<const char*>(&status.group_id), sizeof(status.group_id));
                outFile.write(reinterpret_cast<const char*>(&status.store_count), sizeof(status.store_count));
                outFile.write(reinterpret_cast<const char*>(&status.destroyed_count), sizeof(status.destroyed_count));
                
                // Read and write store ids
                for (int i = 0; i < status.store_count; i++) {  
                    int store_id;
                    inFile.read(reinterpret_cast<char*>(&store_id), sizeof(store_id));
                    outFile.write(reinterpret_cast<const char*>(&store_id), sizeof(store_id));
                }
                outFile.close();
            }
            inFile.close();
            return true;
        }

        // Handle replica
        if ((metadata.is_replica || metadata.replica_of != -1) && !related_store) {
            // Find and destroy the related store
            int related_id = metadata.replica_of;
            try {
                if (utils::storeExists(store_id)) {
                    destroyStore(related_id, true);
                }
            } catch (const std::exception& e) {
                std::cerr << "Error: Failed to destroy related store: " 
                         << e.what() << std::endl;
            }
        }

        // Remove all files
        try {
            std::filesystem::remove_all(utils::getStorePath(store_id));
        } catch (const std::filesystem::filesystem_error& e) {
            std::cerr << "Failed to remove store files: " << e.what() << std::endl;
            return false;
        }

        return true;
    }

public:
    /**
     * @brief Public method to initiate the destruction of a store.
     * 
     * @param store_id ID of the store to destroy.
     * @param related_store Indicates whether this is a related store being handled.
     * 
     * @return true if the store is successfully destroyed; false otherwise.
     */
    bool destroy(int store_id, bool related_store) {
        if (!utils::storeExists(store_id)) {
            std::cerr << "Store " << store_id << " does not exist" << std::endl;
            return false;
        }

        return destroyStore(store_id, related_store);
    }
};

int main(int argc, char* argv[]) {
    // Check command usages 
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " [store-id]" << std::endl;
        return 1;
    }

    try {
        int store_id = std::stoi(argv[1]);
        
        StoreDestroy destroyer;
        if (!destroyer.destroy(store_id, false)) {
            return 1;
        }

        std::cout << "Store " << store_id << " destroyed successfully" << std::endl;
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}