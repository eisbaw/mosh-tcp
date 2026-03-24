---
id: TASK-7
title: Handle SIGPIPE robustly on Linux
status: Done
assignee: []
created_date: '2026-03-24 09:23'
updated_date: '2026-03-24 10:09'
labels:
  - portability
  - tcp
dependencies: []
references:
  - 'src/network/tcpconnection.cc:374-380'
  - 'src/network/tcpconnection.cc:559-598'
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
SO_NOSIGPIPE only works on macOS/BSD (guarded by #ifdef). On Linux, write() to a broken TCP socket sends SIGPIPE which terminates the process by default. The client blocks SIGPIPE via Select, but this is fragile and the server path needs verification.

Fix: Add global signal(SIGPIPE, SIG_IGN) early in both mosh-client and mosh-server main(), or use send() with MSG_NOSIGNAL instead of write() in write_fully(). The latter is more surgical but requires changing write() to send().
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 SIGPIPE does not kill mosh-server on Linux when TCP client disconnects
- [ ] #2 SIGPIPE does not kill mosh-client on Linux when TCP server disconnects
- [ ] #3 Existing UDP behavior unaffected
<!-- AC:END -->
