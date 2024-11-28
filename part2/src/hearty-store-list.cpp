
/**
 * @file hearty-store-list.cpp
 * @author Nathadon Samairat
 * @brief   The program scans for directories in the base path corresponding to stores.
 *          Metadata for each store is loaded and displayed, including status, block usage, and HA group information.
 * @version 0.1
 * @date 2024-11-28
 * 
 * @copyright Copyright (c) 2024
 * 
 */
#include <iostream>
#include <fstream>
#include <iomanip>
#include "hearty-store-common.hpp"

class StoreList {
private:
    /**
     * @brief Helper function to retrieve and format the status of a store.
     * 
     * @param metadata StoreMetadata structure containing store details.
     * 
     * @return std::string A formatted string describing the store's status.
     */
    std::string getStoreStatus(const StoreMetadata& metadata) {
        std::string status;
        if (metadata.is_destroyed) {
            status = "destroyed";
        }
        if (metadata.is_replica) {
            status += (status.empty() ? "" : ", ") + 
                     std::string("replica of ") + 
                     std::to_string(metadata.replica_of);
        }
        if (metadata.ha_group_id != -1) {
            status += (status.empty() ? "" : ", ") + 
                     std::string("ha-group=") + 
                     std::to_string(metadata.ha_group_id);
        }
        return status.empty() ? "active" : status;
    }

    /**
     * @brief Loads metadata for a specific store from disk.
     * 
     * @param store_id The ID of the store to load metadata for.
     * @param metadata Reference to a StoreMetadata object to populate with data.
     * 
     * @return true if metadata is successfully loaded; false otherwise.
     */
    bool loadStoreMetadata(int store_id, StoreMetadata& metadata) {
        std::string metadata_path = BASE_PATH + "/store_" + std::to_string(store_id) + META_FILENAME;
        std::ifstream file(metadata_path, std::ios::binary);
        if (!file) return false;
        file.read(reinterpret_cast<char*>(&metadata), sizeof(StoreMetadata));
        return true;
    }

public:

    /**
     * @brief Lists all available stores and their metadata.
     */
    void list() {
        if (!std::filesystem::exists(BASE_PATH)) {
            std::cout << "No stores found" << std::endl;
            return;
        }

        bool found = false;
        for (const auto& entry : std::filesystem::directory_iterator(BASE_PATH)) {
            if (entry.is_directory()) {
                std::string dirname = entry.path().filename().string();
                if (dirname.substr(0, 6) == "store_") {
                    int store_id = std::stoi(dirname.substr(6));
                    
                    StoreMetadata metadata;
                    if (loadStoreMetadata(store_id, metadata)) {
                        found = true;
                        std::cout << metadata.store_id << " - " 
                                << getStoreStatus(metadata) 
                                << " (used: " << metadata.used_blocks << "/"
                                << metadata.total_blocks << " blocks)" 
                                << std::endl;
                    }
                }
            }
        }

        if (!found) {
            std::cout << "No stores found" << std::endl;
        }
    }
};

int main() {
    try {
        StoreList lister;
        lister.list();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}