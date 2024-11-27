#include <iostream>
#include <fstream>
#include "hearty-store-common.hpp"

class StoreDestroy {
private:
    bool loadStoreMetadata(int store_id, StoreMetadata& metadata) {
        std::ifstream file(utils::getMetadataPath(store_id), std::ios::binary);
        if (!file) return false;
        file.read(reinterpret_cast<char*>(&metadata), sizeof(StoreMetadata));
        return true;
    }

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