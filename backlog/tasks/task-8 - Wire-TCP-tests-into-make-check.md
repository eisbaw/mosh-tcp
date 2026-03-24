---
id: TASK-8
title: Wire TCP tests into make check
status: Done
assignee: []
created_date: '2026-03-24 09:23'
updated_date: '2026-03-24 10:18'
labels:
  - testing
  - tcp
dependencies: []
references:
  - 'src/tests/Makefile.am:41-42'
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
test-tcp-basic, test-tcp-clientserver, and test-connection are in check_PROGRAMS (compiled) but NOT in the TESTS list in src/tests/Makefile.am. They never run during make check.

Also: the tests use hardcoded ports (60050-60052) and sleep() for synchronization, which is fragile for CI. Consider using port 0 (ephemeral) and pipe-based synchronization.

Additionally, window-resize.test is listed twice in displaytests (lines 37 and 39).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 TCP test programs added to TESTS list
- [ ] #2 Tests use ephemeral ports instead of hardcoded 60050-60052
- [ ] #3 Duplicate window-resize.test entry removed
- [ ] #4 make check runs and passes TCP tests
<!-- AC:END -->
