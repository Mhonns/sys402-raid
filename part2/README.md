# Hearty Store

A high-availability object store implementation supporting replication and parity-based redundancy.

## Build Instructions

```bash
make all
```

The executables will be generated in the `bin/` directory.

## Components

- `hearty-store-init`: Initialize a new store instance
- `hearty-store-put`: Store objects in a store instance
- `hearty-store-get`: Retrieve objects from a store instance
- `hearty-store-list`: List all store instances
- `hearty-store-destroy`: Remove a store instance
- `hearty-store-replicate`: Create a replica of a store instance
- `hearty-store-ha`: Create high-availability group from multiple stores

## Usage

### Initialize Store
```bash
./bin/hearty-store-init [store-id]
```

### Store Object
```bash
./bin/hearty-store-put [store-id] [file-path]
# Returns unique object identifier
```

### Retrieve Object
```bash
./bin/hearty-store-get [store-id] [object-id]
# Output written to stdout, can be redirected:
./bin/hearty-store-get [store-id] [object-id] > output.file
```

### List Stores
```bash
./bin/hearty-store-list
```

### Create Replica
```bash
./bin/hearty-store-replicate [store-id]
# Returns replica store ID
```

### Create HA Group
```bash
./bin/hearty-store-ha [store-id1] [store-id2] ...
```

### Destroy Store
```bash
./bin/hearty-store-destroy [store-id]
```

## Design

- Each store consists of 1024 1MB blocks
- One object per block (objects > 1MB rejected)
- Supports store replication with automatic sync
- High-availability groups with parity-based redundancy
- Degraded operations when store in HA group fails

## Testing

Run test cases:
```bash
./testcase.sh
```