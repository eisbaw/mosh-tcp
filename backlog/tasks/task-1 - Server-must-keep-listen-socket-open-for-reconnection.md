---
id: TASK-1
title: Server must keep listen socket open for reconnection
status: Done
assignee: []
created_date: '2026-03-24 09:22'
updated_date: '2026-03-24 09:45'
labels:
  - architecture
  - tcp
dependencies: []
references:
  - 'src/network/tcpconnection.cc:258-261'
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
After accepting one client, the server closes listen_fd (line 260-261 in tcpconnection.cc). A TCP disconnect kills the server permanently with no recovery path. This defeats mosh's core purpose of surviving network changes.

The server must keep the listen socket open and accept reconnections. The reconnecting client must prove it holds the session key (crypto authentication) before the server accepts the new TCP connection.

Without this fix, TCP mode is strictly worse than SSH for the core mosh use case.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Server keeps listen_fd open after first accept
- [ ] #2 Server accepts new TCP connections after client disconnect
- [ ] #3 Reconnecting client is authenticated via existing crypto session
- [ ] #4 Terminal state is preserved across TCP reconnection
- [ ] #5 Tested: kill client TCP connection, reconnect, verify session survives
<!-- AC:END -->
