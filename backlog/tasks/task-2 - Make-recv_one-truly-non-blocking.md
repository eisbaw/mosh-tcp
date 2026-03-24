---
id: TASK-2
title: Make recv_one() truly non-blocking
status: Done
assignee: []
created_date: '2026-03-24 09:22'
updated_date: '2026-03-24 09:48'
labels:
  - architecture
  - tcp
dependencies: []
references:
  - 'src/network/tcpconnection.cc:700-784'
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
recv_one() contains a while(true) loop that calls read() then poll() with tcp_timeout on EAGAIN. Once a 4-byte length prefix is buffered, it blocks until the full message body arrives — up to 500ms per iteration.

This is called from Transport::recv() in the main event loop. A 500ms block means frozen keyboard input, delayed display updates, and stalled signal handling.

Fix: recv_one() must attempt a single non-blocking read(), append to buffer, check if a complete message is available, and return immediately (empty string if incomplete). The select/poll loop in STMClient::main() handles waiting for readability.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 recv_one() never calls poll() or blocks internally
- [ ] #2 Single read() call per recv_one() invocation
- [ ] #3 Returns empty string when message is incomplete
- [ ] #4 Main event loop remains responsive during partial TCP reads
- [ ] #5 Tested: slow sender does not freeze terminal UI
<!-- AC:END -->
