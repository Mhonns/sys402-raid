#include <string>
#include <cstring>
#include <vector>

const size_t BLOCK_SIZE = 1024 * 1024;              // 1MB
const size_t NUM_BLOCKS = 1024;                     // 1024 blocks
const std::string BASE_PATH = "/tmp";               // Default path to storage
const std::string DATA_FILENAME = "/data.bin";      // Actual data file name
const std::string META_FILENAME = "/metadata.bin";  // Meta data file name
const std::string STORE_DIR = "/store_";            // Default path to storage
const std::string PARITY_FILENAME = "/parity.bin";   // parity filename

struct BlockMetadata {
    bool is_used;           // Is this block currently storing an object
    std::string object_id;  // Unique identifier for the object in this block
    size_t data_size;       // Actual size of data in the block
    time_t timestamp;       // Last modification time will be used for object ID
};

struct StoreMetadata {
    int store_id;
    size_t total_blocks;     // Always 1024
    size_t block_size;       // Always 1MB
    size_t used_blocks;      // Number of blocks currently in use
    bool is_replica;         // Is this a replica store
    int replica_of;          // If is_replica, stores original store_id
    int ha_group_id;         // ID of the HA group if part of one
    bool is_destroyed;       // If store is in destroyed state
};

// Utility functions
namespace utils {
    inline std::string getStorePath(int store_id) {
        return BASE_PATH + "/store_" + std::to_string(store_id);
    }

    inline std::string getDataPath(int store_id) {
        return getStorePath(store_id) + "/data.bin";
    }

    inline std::string getMetadataPath(int store_id) {
        return getStorePath(store_id) + "/metadata.bin";
    }

    // Checks if a store exists
    inline bool storeExists(int store_id) {
        return std::filesystem::exists(getStorePath(store_id));
    }
}