---
id: TASK-3
title: Make send() non-blocking with write queue
status: Done
assignee: []
created_date: '2026-03-24 09:22'
updated_date: '2026-03-24 09:55'
labels:
  - architecture
  - tcp
dependencies: []
references:
  - 'src/network/tcpconnection.cc:559-598'
  - 'src/network/tcpconnection.cc:642-697'
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
send() calls write_fully() which loops with poll() internally. A congested TCP buffer or slow receiver blocks the entire event loop during send.

Fix: send() should queue encrypted data into a write buffer and drain what it can with a single non-blocking write(). Remaining data is flushed when the fd becomes writable in the main select/poll loop.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 send() never calls poll() or blocks internally
- [ ] #2 Write buffer queues outgoing data
- [ ] #3 Partial writes are completed on next writable event
- [ ] #4 Main event loop remains responsive during TCP congestion
<!-- AC:END -->
