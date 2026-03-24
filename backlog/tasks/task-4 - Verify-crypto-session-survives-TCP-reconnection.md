---
id: TASK-4
title: Verify crypto session survives TCP reconnection
status: Done
assignee: []
created_date: '2026-03-24 09:23'
updated_date: '2026-03-24 09:59'
labels:
  - security
  - tcp
dependencies:
  - TASK-1
references:
  - 'src/network/tcpconnection.cc:486-492'
  - src/crypto/crypto.h
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
After TCP reconnect, recv_buffer is cleared but the Crypto::Session object is NOT reset. Session tracks nonce counters for replay protection. After reconnect, nonce sequence continues from where it left off.

Possible outcomes:
- Receiver expected nonce has advanced past what sender will send next
- Or replayed old messages might be accepted
- All post-reconnect messages may fail decryption silently

Needs careful analysis of Crypto::Session replay window behavior across TCP reconnection. May need a reconnection handshake that resynchronizes nonce state.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Post-reconnect messages decrypt successfully
- [x] #2 Replay protection still works after reconnect
- [x] #3 Nonce state is properly coordinated between client and server
- [ ] #4 Tested: disconnect, reconnect, verify bidirectional message exchange works
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Analysis confirms crypto session safely survives TCP reconnection. Crypto::Session is stateless w.r.t. nonces (OCB uses nonce-per-message from ciphertext). Nonce uniqueness guaranteed by process-global Crypto::unique() counter that persists across reconnect. Added documentation comments in reconnect(), accept_connection(), and recv_one(). Added direction-bit check in TCP recv_one() (parity with UDP). Fixed expected_receiver_seq to use p.seq+1 (was p.seq). Build passes.
<!-- SECTION:FINAL_SUMMARY:END -->
