#include "SharedContext.hpp"

#include <signal.h>
#include <errno.h>

bool isProcessAlive(pid_t pid) {
    if (pid < 1) {
        return false;
    }

    if (kill(pid, 0) == 0) {
        return true;  // Process exists
    } else {
        if (errno == ESRCH) {
            return false;  // Process does not exist
        }
        // Handle other errors like EPERM
        return false;
    }
}

key_t SharedContext::get_shared_memory_key() {
    return ftok("/tmp", PROJECT_ID);
    
}

void SharedContext::bind_shared_memory() {
    int segment_id;
    key_t key = get_shared_memory_key();
    
    console->info("=== Shared Memory Binding Debug ===");
    console->info("Key: 0x{:08X}", key);
    console->info("Our struct size: {} bytes", sizeof(SharedMemoryBuffer));

    // Use size 0 to attach to existing segment regardless of size
    segment_id = shmget(key, 0, S_IRUSR | S_IWUSR);
    console->info("shmget returned segment_id: {}", segment_id);
    
    if (segment_id == -1) {
        console->error("shmget failed: {} (errno: {})", strerror(errno), errno);
        state_ = 0;
        return;
    }

    shared_memory = (SharedMemoryBuffer*)shmat(segment_id, NULL, 0);
    console->info("shmat returned address: {}", (void*)shared_memory);
    
    if (shared_memory == (void*) -1) {
        console->error("shmat failed: {} (errno: {})", strerror(errno), errno);
        shared_memory = nullptr;
        state_ = 0;
        return;
    }

    // Get actual segment info
    struct shmid_ds buf;
    if (shmctl(segment_id, IPC_STAT, &buf) == 0) {
        console->info("Actual segment size: {} bytes", buf.shm_segsz);
        console->info("Our struct size: {} bytes", sizeof(SharedMemoryBuffer));
        if (buf.shm_segsz < sizeof(SharedMemoryBuffer)) {
            console->warn("Segment is smaller than our struct! This may cause issues.");
        }
    }

    console->info("Successfully attached to shared memory");
    console->info("Initial procid: {}", shared_memory->procid);
    console->info("Initial frame: {}", shared_memory->frame);
    
    state_ = STATE_VALID;
    console->info("=== End Binding Debug ===");
}


void SharedContext::threadTask(){
    console->info("SharedContext thread started!");
    
    while(!abortThread_){
        uint8_t old_state = state_;
        const SharedMemoryBuffer* context = get_context();
        
        // Check 1: Null reference
        if(context == nullptr){
            console->error("get_context() returned nullptr!");
            state_ |= STATE_NULL_REF;
        } else {
            state_ &= ~STATE_NULL_REF;
        }

        // Check 2: Process alive
        if(context != nullptr) {
            bool processAlive = isProcessAlive(context->procid);
            if(!processAlive){
                if(context->procid > 0) {
                    // console->warn("Producer process {} is not alive", context->procid);
                } else {
                    // console->warn("Invalid procid in shared memory: {}", context->procid);
                }
                state_ &= ~STATE_VALID;
            } else {
                state_ |= STATE_VALID;
            }
        }

        // Check 3: Timeout
        if(context != nullptr) {
            uint64_t currentTs = getTs();
            uint64_t age = currentTs - context->ts;
            
            if(age > 1000 && !timedOut_){
                console->warn("Connection timed out! Frame age: {}ms, current: {}, frame: {}", 
                             age, currentTs, context->ts);
                state_ |= STATE_TIMEOUT;
                timedOut_ = true;
            } else if(age <= 1000) {
                state_ &= ~STATE_TIMEOUT;
                if(timedOut_) {
                    console->info("Connection restored! Frame age: {}ms", age);
                    timedOut_ = false;
                }
            }
        }

        // Log state changes
        if(old_state != state_) {
            console->info("State changed: 0x{:02X} -> 0x{:02X} (VALID:{}, NULL_REF:{}, TIMEOUT:{})", 
                         old_state, state_,
                         (state_ & STATE_VALID) ? 1 : 0,
                         (state_ & STATE_NULL_REF) ? 1 : 0,
                         (state_ & STATE_TIMEOUT) ? 1 : 0);
        }

        // Process ID tracking
        if(context != nullptr && timedOut_ && (lastProcId_ != context->procid)){
            console->info("Process ID changed from {} to {}, clearing timeout", 
                         lastProcId_, context->procid);
            timedOut_ = false;
        }

        if(context != nullptr) {
            lastProcId_ = context->procid;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    console->info("SharedContext thread ending");
}
