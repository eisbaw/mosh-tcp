---
id: TASK-6
title: 'Remove dead code: read_fully()'
status: Done
assignee: []
created_date: '2026-03-24 09:23'
updated_date: '2026-03-24 10:06'
labels:
  - code-quality
  - tcp
dependencies: []
references:
  - 'src/network/tcpconnection.cc:519-557'
  - 'src/network/tcpconnection.h:145'
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
read_fully() is declared and defined in tcpconnection.cc but never called. recv_one() uses raw read() with its own EAGAIN handling. Dead code should be removed.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 read_fully() removed from tcpconnection.h and tcpconnection.cc
- [ ] #2 Build succeeds after removal
<!-- AC:END -->
