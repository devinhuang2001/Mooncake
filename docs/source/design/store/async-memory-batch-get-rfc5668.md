---
orphan: true
---

# Async memory-only batch get (RFC #5668): implementation slice

This is an **experimental, incomplete** first slice of
[Issue #5668](https://github.com/kvcache-ai/Mooncake/issues/5668).
It is not yet ready for production or merge.

## API implemented in this branch

```python
op = store.batch_get_into_multi_buffers_async(
    ["key-a", "key-b"],
    [[ptr_a0, ptr_a1], [ptr_b]],
    [[size_a0, size_a1], [size_b]],
)
if op.poll() is None:
    # Submit another independent layer while this one is in flight.
    pass
result = op.wait()  # One byte count or negative error per key.
```

The C++ Store submits its memory-replica transfers through the existing
`TransferSubmitter` rather than dispatching the blocking BatchGet in
an executor. The handle keeps the underlying C++ Store Client alive until
its transfers are drained and checked. Its destructor calls `wait()`.

## Deliberately restricted initial scope

* RealClient only; `DummyClient` is not supported.
* RDMA protocol and MEMORY replicas only. No SSD, DFS, NoF, GPU IPC,
  hybrid transport, or TENT-specific claims.
* Every destination slice must be registered, writable, and within a
  single registered region. The caller must keep allocations registered
  and alive until terminal completion.
* Duplicate keys are rejected; they cannot safely be represented by the
  existing key-indexed batch map without explicit duplicate handling.
* Admission currently happens after metadata lookup and transport submit,
  not in a separate bounded dispatcher.
* No global in-flight operation/byte credits, timeout-taking wait,
  wait-any completion queue, drain-on-explicit-close coordination, or
  connector/graph barrier yet. These are required before removing the
  draft status.

## Review and validation checklist

- [ ] Compile Store, integration bindings, and `async_memory_batch_get_test`
- [ ] Verify registered-buffer direct RDMA path on real hardware
- [ ] Deterministically block first completion; prove second layer is
      submitted before first completion
- [ ] Compare bytes against synchronous `batch_get_into_multi_buffers`,
      including discontiguous slices and key order
- [ ] Validate failures, checksum, lease expiry, zero/invalid pointers,
      mixed MEMORY/DISK routing, timeout and partial DMA drain
- [ ] Verify dropping handle and shutting down the Python client while
      reads are in flight; prohibit memory unregister until DMA is drained
- [ ] Implement bounded per-client operation and byte admission, including
      rejection with no submitted writes and subsequent credit release
- [ ] Implement wait timeout and terminal-state guarantees
- [ ] Compare against bounded concurrent blocking reads, not only serial
      synchronous calls; measure P50/P99, CPU and device gaps

See upstream's earlier closed prototype
[#1861](https://github.com/kvcache-ai/Mooncake/pull/1861) for previous
two-phase work. This version deliberately avoids copying that patch,
which predates the current Store lifecycle and duplicate-fanout changes.
