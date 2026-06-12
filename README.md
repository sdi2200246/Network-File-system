objects.c – Core Data Structures & Monitor Logic

This file implements all the low-level "object-oriented" logic that supports the manager-worker-client model in your NFS system. It includes creation, destruction, and thread-safe access to shared structures like file sync requests, job monitors, and live client counters.

### Request Management

#### `Request create_init_req(...)`
- Allocates a new `Request` object containing:
  - Source/target IPs and ports
  - Source and destination file paths
  - Optional `Pair_monitor` to track sync completion
- Assigns a custom destructor (`Destroy_request`) for cleanup.

#### `int equal_request(...)`
- Compares a `Request` with a given directory, address, and port.
- Used for finding or canceling matching jobs.

#### `void Destroy_request(...)`
- Frees memory associated with a `Request`.

---

###  Sync Pair Management

#### `Sync_info_pair create_init_syncpair(...)`
- Creates a `Sync_info_pair`, which represents an active sync session.
- Stores info about the source and target directories, IPs, ports, and a status flag.
- Used to:
  - Avoid duplicate syncs.
  - Detect when a directory is already being synced.

#### `Sync_info_pair find_pair(...)`
- Searches a vector of pairs for one matching a given source directory.

#### `int sync_info_pair_equals(...)`
- Compares the fields of a pair to a source directory and address/port.

#### `Destroy_syncpair(...)`
- Deallocates memory used by a pair.

---

### Request Monitor (`requests_monitor`)

This monitor acts as a **producer-consumer buffer** shared by all worker threads.

#### `requests_monitor_initialization()`
- Creates the monitor with:
  - A mutex (`lock`) for safe queue access.
  - A condition variable (`empty`) to block consumers when the queue is empty.
  - A dynamic vector to store `Request` objects.
  - A `stop` flag to signal termination.

#### `add_request(...)`
- Adds a `Request` to the queue.
- Broadcasts to wake any blocked worker threads.

#### `get_request(...)`
- Called by worker threads.
- Blocks if queue is empty until a `Request` arrives or shutdown is signaled.

#### `remove_requests(...)`
- Removes queued requests matching a specific source (for `cancel` command).
- Also notifies the associated `Pair_monitor` of how many were removed.

#### `destroy_requests_monitor(...)`
- Destroys all internal fields, including queue and synchronization primitives.

---

### Pair Monitor (`Pair_monitor`)

Each pair monitor tracks the **progress of a single sync operation**, especially for live `add` commands triggered from a client.

#### `pair_monitor_initialization(...)`
- Allocates and sets up:
  - `count` and `files_no`: progress tracking.
  - `lock`, `ready`, and `done` condition variables.

#### `increase_count(...)`
- Atomically increments the number of completed files.
- Signals `done` if all files in the sync are finished.

#### `early_destroy_pair_monitor(...)`
- Decreases expected file count if some jobs are removed early (via cancel).

#### `destroy_pair_monitor(...)`
- Waits until all expected files are transferred, then cleans up.

---
###  Client Management

#### `Client create_init_client(...)`
- Factory function for a new client structure:
  - Stores client socket and original request message.

#### `Clients_monitor_initialization(...)`
- Tracks the number of currently connected clients.
- Used by the manager to decide when to shut down.

#### `increase_clients(...)` / `decrease_clients(...)`
- Atomically updates client count.
- `decrease_clients()` signals a condition variable if count hits zero.

#### `destroy_Clients_monitor(...)`
- Waits for all clients to disconnect before safely destroying the monitor.

---

##  Thread Safety Model

This file provides the full monitor logic using:
- **Mutex locks** to ensure consistent state.
- **Condition variables** to allow blocking and signaling between threads.
- **Shared counters** to manage resource lifecycles (e.g., `files_no`, `count`, `monitor->stop`, `monitor->count`).

Every shared structure in the system is protected by its corresponding monitor:
| Structure             | Protected by         |
|----------------------|----------------------|
| `Request` queue       | `requests_monitor`    |
| Client connections    | `clients_monitor`     |
| Sync progress         | `Pair_monitor`        |
| Sync pair tracking    | `sync_info_lock` (mutex in main) |

---

##  Summary

This file contains **all object factories and monitors** that coordinate concurrent execution. It is a crucial backbone enabling:
- Thread-safe job scheduling
- Client connection management
- Synchronized multi-threaded file transfers
- Accurate cancellation, shutdown, and progress monitoring

All higher-level behavior in the manager (`nfs_manager.c`) and workers (`nfsmanager_utils.c`) ultimately depends on these abstractions being solid and thread-safe.

====================================================================================================================================================================================================


## clients_threads.c – Core Client Request Handler

This module implements the core functionality of the NFS client. It defines how a passive client responds to file service requests from the manager or other components. The client supports three main operations: `LIST`, `PULL`, and `PUSH`.

### Supported Operations

#### 1. `LIST`
- **Function**: `List()`
- Opens the requested directory and reads all non-hidden filenames.
- Aggregates them into a single message with newline separators, ending with a `.`.
- Sends a header (`OK <size>`) followed by the file list.

#### 2. `PULL`
- **Function**: `Pull()`
- Opens the specified file in read-only mode.
- Reads it incrementally in `MAX_DATA_CAPACITY`-sized chunks.
- Dynamically reallocates a buffer to hold the data.
- Sends a header (`OK <filesize>`) followed by the full file content in a single stream.

#### 3. `PUSH`
- **Function**: `Push()`
- The file is sent in stages:
  - First stage (`chunksize == -1`) opens and truncates the target file.
  - Middle stages (`chunksize > 0`) write chunks to the open file descriptor.
  - Final stage (`chunksize == 0`) closes the file.
- Each stage sends an `OK 0` header to acknowledge progress.

---

###  Protocol Parsing

- **Function**: `read_request()` parses the raw client request into:
  - Operation (e.g., `PULL`, `PUSH`)
  - File name
  - Optional chunk size (for `PUSH`)

- **Function**: `read_Push_request_chunk_size()` parses a `PUSH` request to extract just the chunk size field from the header.

---

###  Main Thread Handler

- **Function**: `handle_client()`
- Entry point for each client connection.
- Reads the incoming request, identifies the operation, and dispatches it.
- Handles an entire series of `PUSH` requests (multi-chunk uploads).
- Ensures cleanup of open file descriptors and memory, regardless of success or failure.
- Reports errors using a `Vector Errors` object and sends a standardized error header (`ERROR <len> <msg>`).

---

### Error Handling

All functions push detailed error messages into a shared `Errors` vector. Before returning from any major operation, the handler checks if the vector is empty. If errors are found:
- A response with the format `ERROR <length> <message>` is sent.
- Cleanup routines (closing files/sockets, freeing memory) are still guaranteed.
---

=========================================================================================================================


## nfs_client.c – Passive Client Entry Point

This file implements the main server logic for a passive NFS client. It initializes a listening socket on a given port, accepts incoming connections, and dispatches them to new threads for processing via `clients_threads.c`.

###  Key Responsibilities

- Parses command-line arguments to determine which port to bind.
- Initializes a TCP socket and binds it to the provided port.
- Enters an infinite loop to `accept()` incoming connections.
- Allocates a new memory slot for each socket (`stream2`) to avoid race conditions.
- Spawns a new thread (`pthread_create`) for each accepted connection using `handle_client()`.
- Detaches the thread to allow autonomous cleanup upon termination.

### Thread Safety

To avoid concurrency bugs:
- Each accepted socket is passed as a pointer to a dynamically allocated `int`, so simultaneous threads don’t overwrite each other’s descriptors.
- Threads are immediately detached (`pthread_detach`) to eliminate the need for explicit `join()` calls.

### Error Handling

- If socket creation or binding fails, the program prints a fatal error and exits.
- If `accept()` fails for a specific connection, it logs the issue but continues accepting others.
- If `pthread_create` fails, the error message is printed, but the server stays online.

### Example Usage

```sh
./nfs_client -p 8000

================================================================================================================================


nfs_manager.c – Central Coordinator (Manager)

This file serves as the **entry point for the NFS Manager**, which acts as the orchestrator in the entire system. It manages client consoles, reads sync tasks from configuration, and dispatches sync jobs to a pool of worker threads.

###  Core Responsibilities

- Initializes the socket and accepts incoming connections from console interfaces.
- Spawns a pool of worker threads (`grand_sync_request()`) to perform background file sync tasks.
- Reads a configuration file of synchronization pairs at startup and queues initial jobs.
- Handles live client requests (`add`, `cancel`, `shutdown`) and spawns a thread per console.
- Tracks active clients and ensures graceful shutdown only when all threads finish.

###  Threading Architecture

- Each incoming console request (via TCP) is processed by a thread executing `handle_clients()`.
- A fixed-size worker thread pool performs the file synchronization jobs (via `grand_sync_request()`).
- Shared structures like `requests_monitor` and `clients_monitor` are protected using mutexes and condition variables.

---
=================================================================================================================================================================

nfsmanager_utils.c – Utilities, Handlers, and Worker Threads

This file implements the **entire backend logic** used by the manager. It contains:
- System initialization
- File sync task queuing and processing
- Request parsing and response generation
- Logging
- All low-level socket-based message handling for `PUSH` and `PULL`

-Worker Thread Logic

#### `void* grand_sync_request(void *buf_capacity)`
- The thread pulls a `Request` from the global `requests_monitor`.
- Connects to the source and target clients.
- Sends a `PULL` request to download the file.
- If successful, performs a `PUSH` to upload it to the destination.
- Logs success/failure using `update_log_file()`.
- Notifies the associated `Pair_monitor` (if any) to track job completion.

###  Job Lifecycle

1. **Queueing a Job**  
   - From the config or live client, `add_requests()` is called.
   - It:
     - Connects to the source to retrieve a file list via `Read_List()`.
     - For each file, creates a `Request` with source/target data.
     - Pushes it into the shared `requests_monitor`.

2. **Executing a Job**  
   - A worker thread picks up the `Request`.
   - Connects to the remote client using TCP.
   - Sends a `PULL` followed by a series of `PUSH` messages.
   - Cleans up resources and logs the outcome.

###  PUSH/PULL Messaging

- Messages follow a basic custom protocol:
  - `PULL <filepath>` → gets `OK <size>` and the file.
  - `PUSH <filepath> <chunk_size> <data>` → chunked uploads.
  - `PUSH <filepath> 0` → signals end of upload.

- Headers are parsed using `read_header()` and error responses via `read_Error()`.

###  Configuration and Live Commands

#### `Read_config_file(char *filename)`
- Reads config line by line (`/src@ip:port /dst@ip:port`).
- Schedules sync jobs at startup by calling `add_requests()`.

#### `handle_clients()`
- Parses incoming console command into `add` or `cancel`.
- `add`:
  - Starts a sync by calling `handle_add_request()` → `add_requests()`.
  - Uses `Pair_monitor` to track per-client completion.
- `cancel`:
  - Removes queued jobs via `remove_requests()` if the pair exists.

###  Logging

#### `update_log_file()`
- Thread-safe logging to both log file and, optionally, back to the client.
- Provides complete trace:
  - Timestamp
  - Source and target IPs/ports
  - File
  - Operation type (PULLED, PUSHED)
  - Status (SUCCESS/ERROR)
  - Thread ID

### Thread Synchronization

- All shared data structures are guarded:
  - `log_lock`: Prevents race conditions on logging.
  - `sync_info_lock`: Controls access to current sync pairs.
- `clients_monitor`:
  - Tracks how many consoles are connected.
  - Ensures clean shutdown after all threads finish.

---

## Sync Flow Summary

1. **Initialization**  
   `nfs_manager.c` calls `system_boot()` → initializes socket, monitors, log file.

2. **Startup Sync (from config)**  
   Reads a config file → generates `Request`s → queues them. ****NOTE***  Configration file must end just an empty line 

3. **Incoming Command (via Console)**  
   Console sends `add /dir@ip:port ...` → parsed by `handle_clients()`  
   → `handle_add_request()` → queues `Request`s.

4. **Worker Thread Executes**  
   `grand_sync_request()` picks up request, does PULL then PUSH, logs the result.

5. **Console Feedback & Logging**  
   If the request was initiated via console, the manager notifies the console client when all transfers finish.

6. **Shutdown**  
   If `shutdown` is received, manager:
   - Waits for all consoles to exit.
   - Waits for workers to complete jobs.
   - Releases all memory and file handles.

---

## Summary

Together, `nfs_manager.c` and `nfsmanager_utils.c` form the **heart of the NFS system**:

- **Scalable**: Supports multiple syncs concurrently, using a bounded thread pool.
- **Synchronized**: Mutexes and condition variables ensure safe multithreading.
- **Reliable**: Handles partial failures, restarts, job cancellation, and graceful shutdown.
- **Extensible**: Structured cleanly to allow additional operations or UI layers in the future.

This is the component that enables **central coordination and orchestration** of all network-based file syncs.

=============================================================================================================================================================================================================
