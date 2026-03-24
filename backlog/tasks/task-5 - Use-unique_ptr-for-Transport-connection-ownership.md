---
id: TASK-5
title: Use unique_ptr for Transport connection ownership
status: Done
assignee: []
created_date: '2026-03-24 09:23'
updated_date: '2026-03-24 10:04'
labels:
  - code-quality
  - tcp
dependencies: []
references:
  - 'src/network/networktransport.h:78'
  - 'src/network/networktransport-impl.h:80-84'
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Transport class owns ConnectionInterface* via raw delete in destructor (networktransport-impl.h:83). Factory methods allocate with new then pass to Transport constructor. If the Transport constructor throws (e.g. during sender construction), the connection leaks.

Fix: Use std::unique_ptr<ConnectionInterface> for the connection member. Factory methods should use unique_ptr to hold the connection until ownership is safely transferred.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 connection member is std::unique_ptr<ConnectionInterface>
- [ ] #2 Factory methods use unique_ptr for exception safety
- [ ] #3 No raw delete in Transport destructor
- [ ] #4 Build passes with -Weffc++
<!-- AC:END -->
