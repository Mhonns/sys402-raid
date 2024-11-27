// hearty-store-list.cpp
#include <iostream>
#include <fstream>
#include <iomanip>
#include "hearty-store-common.hpp"

class StoreList {
private:
    // Helper function to get store status
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

    bool loadStoreMetadata(int store_id, StoreMetadata& metadata) {
        std::string metadata_path = BASE_PATH + "/store_" + std::to_string(store_id) + META_FILENAME;
        std::ifstream file(metadata_path, std::ios::binary);
        if (!file) return false;
        file.read(reinterpret_cast<char*>(&metadata), sizeof(StoreMetadata));
        return true;
    }

public:
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