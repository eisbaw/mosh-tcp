---
id: TASK-9
title: Reduce TransportSender send interval to 1ms for TCP
status: Done
assignee: []
created_date: '2026-03-24 14:32'
updated_date: '2026-03-24 14:47'
labels:
  - performance
  - tcp
dependencies: []
references:
  - src/network/transportsender-impl.h
  - src/network/transportsender.h
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
TransportSender deliberately batches outgoing data on a timer (send_interval() based on SRTT, typically 20-100ms between sends). This adds noticeable latency compared to SSH which sends every keystroke immediately with TCP_NODELAY.

For TCP transport, the kernel already handles congestion control and batching. The application-level send delay is pure overhead.

Fix: When using TCP transport, set send_interval minimum to 1ms (effectively immediate). The existing SRTT-based interval makes sense for UDP (unreliable, needs app-level pacing) but not for TCP.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 TransportSender uses 1ms minimum send interval when connection is TCP
- [ ] #2 UDP send interval behavior unchanged
- [ ] #3 Keypress-to-display latency measurably improved over TCP
<!-- AC:END -->
