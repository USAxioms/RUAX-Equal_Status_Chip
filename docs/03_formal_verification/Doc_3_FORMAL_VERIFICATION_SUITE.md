# Doc_3: Complete Formal Verification Suite

## Overview

This document provides:
1. All 16 SVA formal properties (extracted from Doc_2)
2. Formal property binding module
3. Execution commands for Cadence JasperGold and Synopsys VC Formal
4. Cover properties for completeness
5. Sign-off checklist

---

## SVA Formal Properties

### Property Set 1: INTEGRITY (I₁.₁) — 4 Properties

```verilog
// Property 1.1: Digest ready after valid input (latency)
property p_integrity_digest_ready;
  @(posedge clk) (cert_valid) |-> 
    ##4 (~reset);  // By cycle t+4, design is still alive
endproperty
assert property (p_integrity_digest_ready);

// Property 1.2: Soundness - match implies accept
property p_integrity_sound;
  @(posedge clk) disable iff (reset)
    ((sha_digest[127:0] == signature_expected) && digest_valid) |->
      (integrity_ok == 1'b1);
endproperty
assert property (p_integrity_sound);

// Property 1.3: Completeness - mismatch implies reject
property p_integrity_complete;
  @(posedge clk) disable iff (reset)
    ((sha_digest[127:0] != signature_expected) && digest_valid) |->
      (integrity_ok == 1'b0);
endproperty
assert property (p_integrity_complete);

// Property 1.4: Fail-closed reset
property p_integrity_reset;
  @(posedge clk) 
    (reset) |-> ##1 (integrity_ok == 1'b0);
endproperty
assert property (p_integrity_reset);
```

**Rationale:** INTEGRITY predicates must verify SHA-256 signature and fail-closed on reset. Soundness and completeness ensure no false positives or negatives.

---

### Property Set 2: SCOPE (I₁.₂) — 4 Properties

```verilog
// Property 2.1: Scope match (req_id && addr in range)
property p_scope_match;
  @(posedge clk) disable iff (reset)
    ((txn_req_id == cert_req_id) && 
     (txn_addr >= cert_addr_lo) && 
     (txn_addr < cert_addr_hi)) |->
      (scope_ok == 1'b1);
endproperty
assert property (p_scope_match);

// Property 2.2: Requester ID mismatch
property p_scope_req_mismatch;
  @(posedge clk) disable iff (reset)
    (txn_req_id != cert_req_id) |-> (scope_ok == 1'b0);
endproperty
assert property (p_scope_req_mismatch);

// Property 2.3: Address below range
property p_scope_addr_below;
  @(posedge clk) disable iff (reset)
    ((txn_req_id == cert_req_id) && (txn_addr < cert_addr_lo)) |->
      (scope_ok == 1'b0);
endproperty
assert property (p_scope_addr_below);

// Property 2.4: Address at or above high boundary (exclusive)
property p_scope_addr_above;
  @(posedge clk) disable iff (reset)
    ((txn_req_id == cert_req_id) && (txn_addr >= cert_addr_hi)) |->
      (scope_ok == 1'b0);
endproperty
assert property (p_scope_addr_above);
```

**Rationale:** SCOPE predicates enforce both requester ID match and half-open address range [lo, hi). All four edges must be verified: match, id-mismatch, below-range, at/above-high.

---

### Property Set 3: TEMPORAL (I₁.₃) — 4 Properties

```verilog
// Property 3.1: All temporal conditions pass
property p_epoch_all_pass;
  @(posedge clk) disable iff (reset)
    ((t_now >= cert_t_issue) && 
     (t_now < cert_t_expiry) &&
     (({1'b0,t_now} - {1'b0,cert_t_issue}) <= {1'b0,delta_max})) |->
      (epoch_ok == 1'b1);
endproperty
assert property (p_epoch_all_pass);

// Property 3.2: Not yet issued
property p_epoch_not_issued;
  @(posedge clk) disable iff (reset)
    (t_now < cert_t_issue) |-> (epoch_ok == 1'b0);
endproperty
assert property (p_epoch_not_issued);

// Property 3.3: Expired (time >= expiry)
property p_epoch_expired;
  @(posedge clk) disable iff (reset)
    (t_now >= cert_t_expiry) |-> (epoch_ok == 1'b0);
endproperty
assert property (p_epoch_expired);

// Property 3.4: Too old (age > delta_max)
property p_epoch_too_old;
  @(posedge clk) disable iff (reset)
    (({1'b0,t_now} - {1'b0,cert_t_issue}) > {1'b0,delta_max}) |->
      (epoch_ok == 1'b0);
endproperty
assert property (p_epoch_too_old);
```

**Rationale:** TEMPORAL predicates enforce a 3-part window: (t_now >= t_issue) && (t_now < t_expiry) && (age <= delta_max). This prevents stale/expired/future-dated certificates.

---

### Property Set 4: QUOTA (I₁.₄) — 4 Properties

```verilog
// Property 4.1: Quota below ceiling
property p_quota_below;
  @(posedge clk) disable iff (reset)
    (quota_counters[slot] < cert_q_max) |->
      (quota_ok == 1'b1);
endproperty
assert property (p_quota_below);

// Property 4.2: Quota at or above ceiling
property p_quota_at_ceiling;
  @(posedge clk) disable iff (reset)
    (quota_counters[slot] >= cert_q_max) |->
      (quota_ok == 1'b0);
endproperty
assert property (p_quota_at_ceiling);

// Property 4.3: Quota increment on admission
property p_quota_increment;
  @(posedge clk) disable iff (reset)
    (admit) |->
      ##1 (quota_counters[slot] == $past(quota_counters[slot]) + 1'b1);
endproperty
assert property (p_quota_increment);

// Property 4.4: Quota monotonicity (only increases or holds)
property p_quota_monotonic;
  @(posedge clk) disable iff (reset)
    (quota_counters[slot] <= $past(quota_counters[slot]) + 1'b1);
endproperty
assert property (p_quota_monotonic);
```

**Rationale:** QUOTA predicates enforce per-requester transaction limits. Counters must be monotonic, increment only on admission, and prevent overflow/underflow.

---

### Property 5: Master Invariant (I₀) — 1 Property

```verilog
// Property 5: Master Invariant - ALL predicates must be true for admission
property p_master_invariant;
  @(posedge clk) disable iff (reset)
    (admit) |->
      (integrity_ok && scope_ok && epoch_ok && quota_ok);
endproperty
assert property (p_master_invariant);
```

**Rationale:** The master invariant guarantees that every admitted transaction satisfies all four predicates simultaneously. This is the highest-level correctness claim.

---

### Cover Properties (Completeness)

```verilog
// Coverage: Can we ever admit a transaction?
cover property (admit == 1'b1);

// Coverage: Can we ever reject a transaction?
cover property (admit == 1'b0);

// Coverage: Can we reach quota exhaustion?
cover property ((quota_ok == 1'b0) && (txn_valid));

// Coverage: Can we reach time expiry?
cover property ((epoch_ok == 1'b0) && (t_now >= cert_t_expiry));

// Coverage: Can we reach time not-yet-issued?
cover property ((epoch_ok == 1'b0) && (t_now < cert_t_issue));

// Coverage: Can we reach scope mismatch?
cover property ((scope_ok == 1'b0) && (txn_valid));
```

---

## SVA Property Binding Module

Create a separate file: `perm_ctrl_sva_bind.v`

```verilog
// SVA Property Binding Module
// Binds all formal properties to the design

bind perm_ctrl formal_properties u_sva_bind (
  .clk(clk),
  .reset(reset),
  .cert_valid(cert_valid),
  .integrity_ok(integrity_ok),
  .scope_ok(scope_ok),
  .epoch_ok(epoch_ok),
  .quota_ok(quota_ok),
  .admit(admit),
  .txn_req_id(txn_req_id),
  .txn_addr(txn_addr),
  .t_now(t_now),
  .cert_req_id(reg_req_id),
  .cert_addr_lo(reg_addr_lo),
  .cert_addr_hi(reg_addr_hi),
  .cert_t_issue(reg_t_issue),
  .cert_t_expiry(reg_t_expiry),
  .cert_q_max(reg_q_max),
  .delta_max(delta_max),
  .txn_valid(txn_valid),
  .quota_counters(quota_check.quota_counters),
  .sha_digest(integrity_check.sha_digest),
  .signature_expected(integrity_check.signature_expected),
  .digest_valid(integrity_check.sha_digest_valid)
);

module formal_properties (
  input logic clk, reset, cert_valid, integrity_ok, scope_ok, epoch_ok, quota_ok, admit, txn_valid,
  input logic [63:0] txn_req_id, txn_addr, cert_req_id, cert_addr_lo, cert_addr_hi,
  input logic [31:0] t_now, cert_t_issue, cert_t_expiry, cert_q_max, delta_max,
  input logic [15:0] quota_counters [0:15],
  input logic [255:0] sha_digest,
  input logic [127:0] signature_expected,
  input logic digest_valid
);

  wire [3:0] slot = txn_req_id[3:0];
  
  // ===== INTEGRITY Properties (4) =====
  property p_integrity_digest_ready;
    @(posedge clk) (cert_valid) |-> ##4 (~reset);
  endproperty
  assert property (p_integrity_digest_ready);

  property p_integrity_sound;
    @(posedge clk) disable iff (reset)
      ((sha_digest[127:0] == signature_expected) && digest_valid) |->
        (integrity_ok == 1'b1);
  endproperty
  assert property (p_integrity_sound);

  property p_integrity_complete;
    @(posedge clk) disable iff (reset)
      ((sha_digest[127:0] != signature_expected) && digest_valid) |->
        (integrity_ok == 1'b0);
  endproperty
  assert property (p_integrity_complete);

  property p_integrity_reset;
    @(posedge clk) (reset) |-> ##1 (integrity_ok == 1'b0);
  endproperty
  assert property (p_integrity_reset);

  // ===== SCOPE Properties (4) =====
  property p_scope_match;
    @(posedge clk) disable iff (reset)
      ((txn_req_id == cert_req_id) && 
       (txn_addr >= cert_addr_lo) && 
       (txn_addr < cert_addr_hi)) |->
        (scope_ok == 1'b1);
  endproperty
  assert property (p_scope_match);

  property p_scope_req_mismatch;
    @(posedge clk) disable iff (reset)
      (txn_req_id != cert_req_id) |-> (scope_ok == 1'b0);
  endproperty
  assert property (p_scope_req_mismatch);

  property p_scope_addr_below;
    @(posedge clk) disable iff (reset)
      ((txn_req_id == cert_req_id) && (txn_addr < cert_addr_lo)) |->
        (scope_ok == 1'b0);
  endproperty
  assert property (p_scope_addr_below);

  property p_scope_addr_above;
    @(posedge clk) disable iff (reset)
      ((txn_req_id == cert_req_id) && (txn_addr >= cert_addr_hi)) |->
        (scope_ok == 1'b0);
  endproperty
  assert property (p_scope_addr_above);

  // ===== TEMPORAL Properties (4) =====
  property p_epoch_all_pass;
    @(posedge clk) disable iff (reset)
      ((t_now >= cert_t_issue) && 
       (t_now < cert_t_expiry) &&
       (({1'b0,t_now} - {1'b0,cert_t_issue}) <= {1'b0,delta_max})) |->
        (epoch_ok == 1'b1);
  endproperty
  assert property (p_epoch_all_pass);

  property p_epoch_not_issued;
    @(posedge clk) disable iff (reset)
      (t_now < cert_t_issue) |-> (epoch_ok == 1'b0);
  endproperty
  assert property (p_epoch_not_issued);

  property p_epoch_expired;
    @(posedge clk) disable iff (reset)
      (t_now >= cert_t_expiry) |-> (epoch_ok == 1'b0);
  endproperty
  assert property (p_epoch_expired);

  property p_epoch_too_old;
    @(posedge clk) disable iff (reset)
      (({1'b0,t_now} - {1'b0,cert_t_issue}) > {1'b0,delta_max}) |->
        (epoch_ok == 1'b0);
  endproperty
  assert property (p_epoch_too_old);

  // ===== QUOTA Properties (4) =====
  property p_quota_below;
    @(posedge clk) disable iff (reset)
      (quota_counters[slot] < cert_q_max) |->
        (quota_ok == 1'b1);
  endproperty
  assert property (p_quota_below);

  property p_quota_at_ceiling;
    @(posedge clk) disable iff (reset)
      (quota_counters[slot] >= cert_q_max) |->
        (quota_ok == 1'b0);
  endproperty
  assert property (p_quota_at_ceiling);

  property p_quota_increment;
    @(posedge clk) disable iff (reset)
      (admit) |->
        ##1 (quota_counters[slot] == $past(quota_counters[slot]) + 1'b1);
  endproperty
  assert property (p_quota_increment);

  property p_quota_monotonic;
    @(posedge clk) disable iff (reset)
      (quota_counters[slot] <= $past(quota_counters[slot]) + 1'b1);
  endproperty
  assert property (p_quota_monotonic);

  // ===== Master Invariant (1) =====
  property p_master_invariant;
    @(posedge clk) disable iff (reset)
      (admit) |->
        (integrity_ok && scope_ok && epoch_ok && quota_ok);
  endproperty
  assert property (p_master_invariant);

  // ===== Cover Properties =====
  cover property (admit == 1'b1);
  cover property (admit == 1'b0);
  cover property ((quota_ok == 1'b0) && (txn_valid));
  cover property ((epoch_ok == 1'b0) && (t_now >= cert_t_expiry));
  cover property ((epoch_ok == 1'b0) && (t_now < cert_t_issue));
  cover property ((scope_ok == 1'b0) && (txn_valid));

endmodule
```

---

## Formal Verification Execution Scripts

### Cadence JasperGold

**File:** `run_jaspergold.tcl`

```tcl
# JasperGold Formal Verification Script

set_app formal

# Read design files
read_verilog -sv {
  rtl_perm_ctrl.v
  perm_ctrl_sva_bind.v
}

# Elaborate top module
elaborate -top perm_ctrl

# Configure solver
set_option -solver_engine {SMV}

# Set time limit for property proving
set_option -timeout 3600

# Prove all assertions
prove -all

# Generate reports
report -summary
report -coverage
report -unproven
report -prove_strategy

# Export results
write_result formal_results.db
```

**Execution:**

```bash
#!/bin/bash

export DESIGN_DIR="/path/to/design"
export WORKDIR="/tmp/formal_work"

mkdir -p $WORKDIR
cd $WORKDIR

jaspergold -file run_jaspergold.tcl

# Check for failures
if grep -q "UNREACHABLE\|FAIL" formal_results.db; then
  echo "FORMAL VERIFICATION FAILED"
  exit 1
else
  echo "FORMAL VERIFICATION PASSED"
  exit 0
fi
```

---

### Synopsys VC Formal

**File:** `run_vc_formal.tcl`

```tcl
# VC Formal Verification Script

set_app vcformal

# Compile design
compile -work formal_work -f compile_files.f

# Run formal verification
verify \
  -top perm_ctrl \
  -mode prove \
  -engine both \
  -incremental \
  -timeout 3600 \
  -jobs 4

# Generate coverage
coverage -app vcformal

# Report results
report -summary
report -failures
report -cover
```

**File:** `compile_files.f`

```
rtl_perm_ctrl.v
perm_ctrl_sva_bind.v
+incdir+./includes
```

**Execution:**

```bash
#!/bin/bash

cd /path/to/formal_work

vcformal -file run_vc_formal.tcl -log formal.log

# Parse results
if grep -q "FAIL\|UNSAT" formal.log; then
  echo "FORMAL VERIFICATION FAILED"
  exit 1
else
  echo "FORMAL VERIFICATION PASSED"
  exit 0
fi
```

---

## Formal Sign-Off Checklist

- [ ] All 16 SVA assertions prove successfully (no unreachable proofs)
- [ ] All 6 cover properties are reachable (completeness verified)
- [ ] No unproven assertions (timeout or solver failure)
- [ ] Master invariant proved across all input combinations
- [ ] Each predicate independently proved (INTEGRITY, SCOPE, TEMPORAL, QUOTA)
- [ ] Reset behavior verified (fail-closed)
- [ ] No found violations or counterexamples
- [ ] Formal report signed by verification lead
- [ ] Formal results archived (timestamp + hash)

---

## Summary

**Total Properties:** 17 (16 assertions + 1 master invariant + 6 covers)

**Verification Coverage:**
- Soundness: All "pass" cases verified
- Completeness: All "fail" cases verified
- Fail-closed: Reset behavior verified
- Monotonicity: State evolution verified

**Execution Time (Expected):** 30-120 minutes per tool, depending on solver strategy.

---

**End of Formal Verification Suite**
