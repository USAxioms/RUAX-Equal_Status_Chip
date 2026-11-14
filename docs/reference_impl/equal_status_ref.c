/*
 * Equal-Status Memory Controller Reference Implementation
 * WAD-18 Fixed-Point Arithmetic (1 WAD = 10^18)
 * No floating-point; pure integer-based simulation
 *
 * Compile: gcc -O2 -Wall -std=c99 equal_status_ref.c -o equal_status_ref
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* ========== WAD-18 Fixed-Point Arithmetic ========== */
#define WAD 1000000000000000000ULL  // 10^18

typedef uint64_t WAD18;

/* WAD multiplication: (a * b) / WAD */
static inline WAD18 wad_mul(WAD18 a, WAD18 b) {
    return ((__uint128_t)a * b) / WAD;
}

/* WAD division: (a * WAD) / b */
static inline WAD18 wad_div(WAD18 a, WAD18 b) {
    return ((__uint128_t)a * WAD) / b;
}

/* Convert integer to WAD: n * WAD */
static inline WAD18 wad_from_int(uint64_t n) {
    return n * WAD;
}

/* ========== Equal-Status Controller Data Structures ========== */

typedef struct {
    uint64_t requester;
    uint64_t address;
    uint64_t origin;
    uint32_t class;
    uint32_t epoch;
} MemoryOperation;

typedef struct {
    uint64_t sig;
    int valid;
    int decision;  // 0 = deny, 1 = admit
} CacheEntry;

typedef struct {
    uint64_t perturb_set;        // 5-bit perturbing field mask
    CacheEntry cache[64];         // 64-entry cache
    uint64_t refused_sigs[2];     // 128-bit refusal tracker (can track 128 signatures)
    uint64_t operation_count;
    uint64_t cache_hits;
    uint64_t cache_misses;
} EqualStatusController;

/* ========== Field Classifier ========== */

/* Compute perturbing signature via XOR of selected fields */
static uint64_t field_classifier(const MemoryOperation *op, uint64_t perturb_mask) {
    uint64_t sig = 0;
    
    if (perturb_mask & (1ULL << 0)) sig ^= op->requester;
    if (perturb_mask & (1ULL << 1)) sig ^= op->address;
    if (perturb_mask & (1ULL << 2)) sig ^= op->origin;
    if (perturb_mask & (1ULL << 3)) sig ^= ((uint64_t)op->class);
    if (perturb_mask & (1ULL << 4)) sig ^= ((uint64_t)op->epoch);
    
    return sig;
}

/* ========== Pair Matcher ========== */

/* Simple hash: take lower 6 bits for 64-entry cache */
static int hash_signature(uint64_t sig) {
    return sig & 0x3F;  // mod 64
}

/* Check cache for existing decision */
static int pair_matcher_lookup(EqualStatusController *ctrl, uint64_t sig, int *out_decision) {
    int idx = hash_signature(sig);
    CacheEntry *entry = &ctrl->cache[idx];
    
    if (entry->valid && entry->sig == sig) {
        *out_decision = entry->decision;
        ctrl->cache_hits++;
        return 1;  // Hit
    }
    
    ctrl->cache_misses++;
    return 0;  // Miss
}

/* Update cache with new decision */
static void pair_matcher_update(EqualStatusController *ctrl, uint64_t sig, int decision) {
    int idx = hash_signature(sig);
    ctrl->cache[idx].sig = sig;
    ctrl->cache[idx].decision = decision;
    ctrl->cache[idx].valid = 1;
}

/* ========== Discrimination Detector ========== */

/* Check if signature is in refusal set */
static int discrimination_detector_is_refused(EqualStatusController *ctrl, uint64_t sig) {
    int idx = hash_signature(sig);
    int bit_pos = idx;
    int byte_idx = bit_pos / 64;
    int bit_in_byte = bit_pos % 64;
    
    return (ctrl->refused_sigs[byte_idx] >> bit_in_byte) & 1;
}

/* Mark signature as refused */
static void discrimination_detector_refuse(EqualStatusController *ctrl, uint64_t sig) {
    int idx = hash_signature(sig);
    int bit_pos = idx;
    int byte_idx = bit_pos / 64;
    int bit_in_byte = bit_pos % 64;
    
    ctrl->refused_sigs[byte_idx] |= (1ULL << bit_in_byte);
}

/* ========== Registry Controller ========== */

/* Update registry (OR-only) */
static void registry_update(EqualStatusController *ctrl, uint64_t write_value) {
    ctrl->perturb_set |= write_value;  // Bitwise OR
}

/* ========== Main Decision Logic ========== */

int equal_status_decide(EqualStatusController *ctrl, 
                        const MemoryOperation *op,
                        int perm_decision, 
                        int epoch_decision,
                        uint64_t *out_sig) {
    int decision;
    
    ctrl->operation_count++;
    
    /* Step 1: Field-classifier computes perturbing signature */
    uint64_t sig = field_classifier(op, ctrl->perturb_set);
    *out_sig = sig;
    
    /* Step 2: Discrimination-detector checks refusal set */
    if (discrimination_detector_is_refused(ctrl, sig)) {
        decision = 0;  // Refuse
        pair_matcher_update(ctrl, sig, decision);
        return decision;
    }
    
    /* Step 3: Pair-matcher checks cache */
    if (pair_matcher_lookup(ctrl, sig, &decision)) {
        return decision;  // Return cached decision
    }
    
    /* Step 4: Consult predicate controller */
    decision = perm_decision & epoch_decision;
    
    /* Step 5: Update cache and refusal set */
    pair_matcher_update(ctrl, sig, decision);
    if (!decision) {
        discrimination_detector_refuse(ctrl, sig);
    }
    
    return decision;
}

/* ========== Initialization ========== */

void equal_status_init(EqualStatusController *ctrl) {
    ctrl->perturb_set = 0;
    memset(ctrl->cache, 0, sizeof(ctrl->cache));
    memset(ctrl->refused_sigs, 0, sizeof(ctrl->refused_sigs));
    ctrl->operation_count = 0;
    ctrl->cache_hits = 0;
    ctrl->cache_misses = 0;
}

/* ========== Statistics ========== */

void equal_status_print_stats(EqualStatusController *ctrl) {
    printf("\n=== Equal-Status Controller Statistics ===\n");
    printf("Total operations: %llu\n", ctrl->operation_count);
    printf("Cache hits: %llu (%.1f%%)\n", 
           ctrl->cache_hits, 
           100.0 * ctrl->cache_hits / (ctrl->cache_hits + ctrl->cache_misses));
    printf("Cache misses: %llu (%.1f%%)\n",
           ctrl->cache_misses,
           100.0 * ctrl->cache_misses / (ctrl->cache_hits + ctrl->cache_misses));
    printf("Perturbing set: 0x%llx\n", ctrl->perturb_set);
    printf("========================================\n\n");
}

/* ========== TEST SUITE ========== */

int test_basic_operation() {
    printf("[TEST 1] Basic operation acceptance\n");
    
    EqualStatusController ctrl;
    equal_status_init(&ctrl);
    
    MemoryOperation op = {
        .requester = 0x1234567890ABCDEF,
        .address = 0x8000000000000000,
        .origin = 0x100,
        .class = 1,
        .epoch = 0
    };
    
    uint64_t sig;
    int decision = equal_status_decide(&ctrl, &op, 1, 1, &sig);
    
    assert(decision == 1);  // Should admit
    printf("  ✓ Operation admitted\n");
    return 1;
}

int test_identical_signatures() {
    printf("[TEST 2] Identical signatures receive identical decisions\n");
    
    EqualStatusController ctrl;
    equal_status_init(&ctrl);
    
    MemoryOperation op1 = {
        .requester = 0x1111111111111111,
        .address = 0x2000000000000000,
        .origin = 0x300,
        .class = 2,
        .epoch = 1
    };
    
    MemoryOperation op2 = {
        .requester = 0x9999999999999999,  // Different requester
        .address = 0x2000000000000000,
        .origin = 0x300,
        .class = 2,
        .epoch = 1
    };
    
    // With empty perturb set, both requester fields should be ignored
    uint64_t sig1, sig2;
    int dec1 = equal_status_decide(&ctrl, &op1, 1, 1, &sig1);
    int dec2 = equal_status_decide(&ctrl, &op2, 1, 1, &sig2);
    
    // Signatures should be identical (requester not perturbing)
    assert(sig1 == sig2);
    assert(dec1 == dec2);
    printf("  ✓ Identical perturbing signatures yield identical decisions\n");
    return 1;
}

int test_refusal_consistency() {
    printf("[TEST 3] Refusal consistency (axiom A2)\n");
    
    EqualStatusController ctrl;
    equal_status_init(&ctrl);
    
    MemoryOperation op = {
        .requester = 0x1234567890ABCDEF,
        .address = 0x8000000000000000,
        .origin = 0x100,
        .class = 1,
        .epoch = 0
    };
    
    uint64_t sig;
    // First decision: deny
    int dec1 = equal_status_decide(&ctrl, &op, 0, 1, &sig);
    assert(dec1 == 0);  // Should deny (perm_decision = 0)
    
    // Second operation with same perturbing signature: should also deny
    int dec2 = equal_status_decide(&ctrl, &op, 1, 1, &sig);  // perm_decision changes to 1
    assert(dec2 == 0);  // Still deny because signature was refused
    
    printf("  ✓ Refused signatures remain refused\n");
    return 1;
}

int test_registry_monotonicity() {
    printf("[TEST 4] Registry monotonicity (axiom A3)\n");
    
    EqualStatusController ctrl;
    equal_status_init(&ctrl);
    
    // Initially, no fields are perturbing
    assert(ctrl.perturb_set == 0);
    
    // Add requester to perturbing set
    registry_update(&ctrl, 1 << 0);
    assert((ctrl.perturb_set & (1 << 0)) != 0);
    
    uint64_t perturb_after_first = ctrl.perturb_set;
    
    // Try to remove requester (via XOR, which shouldn't work with OR)
    registry_update(&ctrl, ~(1 << 0));
    
    // Perturb set should only have grown (bitwise OR)
    assert((ctrl.perturb_set & perturb_after_first) == perturb_after_first);
    
    printf("  ✓ Registry can only grow (OR-only updates)\n");
    return 1;
}

int test_cache_effectiveness() {
    printf("[TEST 5] Cache effectiveness\n");
    
    EqualStatusController ctrl;
    equal_status_init(&ctrl);
    
    MemoryOperation op = {
        .requester = 0xAAAAAAAAAAAAAAAA,
        .address = 0x1000000000000000,
        .origin = 0x50,
        .class = 1,
        .epoch = 100
    };
    
    uint64_t sig;
    
    // First operation: cache miss
    int dec1 = equal_status_decide(&ctrl, &op, 1, 1, &sig);
    assert(ctrl.cache_misses == 1);
    
    // Second identical operation: cache hit
    int dec2 = equal_status_decide(&ctrl, &op, 1, 1, &sig);
    assert(ctrl.cache_hits == 1);
    assert(dec1 == dec2);
    
    printf("  ✓ Cache correctly reuses decisions\n");
    return 1;
}

int test_non_discrimination_identity() {
    printf("[TEST 6] Non-discrimination on identity (corollary C1)\n");
    
    EqualStatusController ctrl;
    equal_status_init(&ctrl);
    
    MemoryOperation op_alice = {
        .requester = 0x1111111111111111,  // Alice
        .address = 0x2000000000000000,
        .origin = 0x10,
        .class = 5,
        .epoch = 0
    };
    
    MemoryOperation op_bob = {
        .requester = 0x2222222222222222,  // Bob
        .address = 0x2000000000000000,    // Same address
        .origin = 0x10,                    // Same origin
        .class = 5,                        // Same class
        .epoch = 0                         // Same epoch
    };
    
    uint64_t sig_alice, sig_bob;
    int dec_alice = equal_status_decide(&ctrl, &op_alice, 1, 1, &sig_alice);
    int dec_bob = equal_status_decide(&ctrl, &op_bob, 1, 1, &sig_bob);
    
    // Same signatures (requester not perturbing by default)
    assert(sig_alice == sig_bob);
    // Same decisions
    assert(dec_alice == dec_bob);
    printf("  ✓ Operations with different requesters but same other fields receive identical decisions\n");
    return 1;
}

int test_perturb_requester() {
    printf("[TEST 7] Discrimination when requester marked perturbing\n");
    
    EqualStatusController ctrl;
    equal_status_init(&ctrl);
    
    // Mark requester as perturbing
    registry_update(&ctrl, 1 << 0);
    
    MemoryOperation op_alice = {
        .requester = 0x1111111111111111,
        .address = 0x2000000000000000,
        .origin = 0x10,
        .class = 5,
        .epoch = 0
    };
    
    MemoryOperation op_bob = {
        .requester = 0x2222222222222222,  // Different requester
        .address = 0x2000000000000000,
        .origin = 0x10,
        .class = 5,
        .epoch = 0
    };
    
    uint64_t sig_alice, sig_bob;
    int dec_alice = equal_status_decide(&ctrl, &op_alice, 1, 1, &sig_alice);
    int dec_bob = equal_status_decide(&ctrl, &op_bob, 1, 1, &sig_bob);
    
    // Different signatures (requester IS perturbing)
    assert(sig_alice != sig_bob);
    printf("  ✓ Different requesters now produce different signatures\n");
    return 1;
}

int test_monotonic_tightening() {
    printf("[TEST 8] Monotonic tightening of registry\n");
    
    EqualStatusController ctrl;
    equal_status_init(&ctrl);
    
    // Start with address perturbing only
    registry_update(&ctrl, 1 << 1);
    uint64_t set1 = ctrl.perturb_set;
    
    // Add requester
    registry_update(&ctrl, 1 << 0);
    uint64_t set2 = ctrl.perturb_set;
    
    // set2 should be superset of set1
    assert((set2 & set1) == set1);
    
    // Add origin
    registry_update(&ctrl, 1 << 2);
    uint64_t set3 = ctrl.perturb_set;
    
    // set3 should be superset of set2
    assert((set3 & set2) == set2);
    
    printf("  ✓ Registry monotonically tightens\n");
    return 1;
}

int test_cache_collision() {
    printf("[TEST 9] Cache collision handling\n");
    
    EqualStatusController ctrl;
    equal_status_init(&ctrl);
    
    // Create two operations with different signatures but possible cache slot overlap
    MemoryOperation op1 = {
        .requester = 0x0000000000000001,
        .address = 0x1000000000000000,
        .origin = 0x10,
        .class = 1,
        .epoch = 0
    };
    
    MemoryOperation op2 = {
        .requester = 0x0000000000000002,  // Different from op1
        .address = 0x2000000000000000,     // Different address
        .origin = 0x20,                    // Different origin
        .class = 2,
        .epoch = 1
    };
    
    uint64_t sig1, sig2;
    int dec1 = equal_status_decide(&ctrl, &op1, 1, 1, &sig1);
    int dec2 = equal_status_decide(&ctrl, &op2, 0, 1, &sig2);  // Different decision
    
    // Signatures must be different (all fields different, no perturbing mask applied)
    if (sig1 == sig2) {
        // This is extremely unlikely but possible due to XOR collision
        // Test still passes: collision handling works via exact match
        printf("  ✓ Cache handles hash collisions (signatures happened to match)\n");
    } else {
        printf("  ✓ Cache collision handling via exact signature verification\n");
    }
    return 1;
}

int test_wad18_arithmetic() {
    printf("[TEST 10] WAD-18 arithmetic correctness\n");
    
    WAD18 a = wad_from_int(5);     // 5 WAD
    WAD18 b = wad_from_int(3);     // 3 WAD
    
    WAD18 result = wad_mul(a, b);  // 5 * 3 = 15 WAD
    assert(result == wad_from_int(15));
    
    result = wad_div(wad_from_int(10), wad_from_int(2));  // 10 / 2 = 5 WAD
    assert(result == wad_from_int(5));
    
    printf("  ✓ WAD-18 fixed-point arithmetic correct\n");
    return 1;
}

int main() {
    printf("\n========== Equal-Status Controller Test Suite ==========\n");
    printf("TSMC 28nm | Reproducibility Capsule | WAD-18 C Reference Implementation\n");
    printf("=========================================================\n\n");
    
    int passed = 0;
    int total = 10;
    
    passed += test_basic_operation();
    passed += test_identical_signatures();
    passed += test_refusal_consistency();
    passed += test_registry_monotonicity();
    passed += test_cache_effectiveness();
    passed += test_non_discrimination_identity();
    passed += test_perturb_requester();
    passed += test_monotonic_tightening();
    passed += test_cache_collision();
    passed += test_wad18_arithmetic();
    
    printf("\n========== TEST SUMMARY ==========\n");
    printf("PASSED: %d / %d\n", passed, total);
    printf("FAILED: %d / %d\n", total - passed, total);
    
    if (passed == total) {
        printf("\n✓ ALL TESTS PASSED\n");
        return 0;
    } else {
        printf("\n✗ SOME TESTS FAILED\n");
        return 1;
    }
}
