---
id: TASK-10
title: Process all buffered TCP messages per event loop iteration
status: Done
assignee: []
created_date: '2026-03-24 14:33'
updated_date: '2026-03-24 14:49'
labels:
  - performance
  - tcp
dependencies: []
references:
  - src/network/networktransport-impl.h
  - src/network/tcpconnection.cc
  - src/frontend/stmclient.cc
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Transport::recv() calls connection->recv() which returns one message or empty. Even if 5 complete messages are sitting in recv_buffer, only one is processed per event loop iteration. Each iteration goes through the full select() -> timer check -> tick cycle before handling the next message.

This adds unnecessary latency proportional to the number of buffered messages times the select timeout.

Fix: Either make Transport::recv() loop internally until recv_buffer is drained of complete messages, or make TCPConnection::recv() / recv_one() drain all complete messages in one call. The caller (STMClient) should also loop on network.recv() while data is available.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 All complete messages in recv_buffer processed in single event loop iteration
- [ ] #2 No extra select() round-trips for already-buffered data
- [ ] #3 UDP behavior unchanged (one datagram per recv is correct for UDP)
<!-- AC:END -->
