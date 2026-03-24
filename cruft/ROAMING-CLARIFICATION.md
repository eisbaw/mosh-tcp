# Reconsidering TCP Transport: The Roaming Question

## The Key Insight I Missed

**Original Criticism:** "TCP can't roam, conflicts with Mosh's philosophy"

**Reality:** Mosh has TWO layers:

### Layer 1: Transport (UDP/TCP)
- Just moves encrypted packets
- Handles network I/O

### Layer 2: State Synchronization
- **This is where the magic happens**
- Maintains terminal state on server
- Session survives transport disconnection
- Can reconnect to same session

---

## How TCP "Roaming" Actually Works

### Scenario: User switches from WiFi to cellular

**UDP (seamless):**
```
1. Client IP changes: 192.168.1.5 → 10.0.0.5
2. Server sees packets from new IP
3. Server updates remote_addr
4. Continues immediately, no interruption
Duration: ~0ms (next packet)
```

**TCP (reconnect):**
```
1. Client IP changes: 192.168.1.5 → 10.0.0.5
2. TCP connection breaks (tied to old IP)
3. Client detects timeout (~500ms)
4. Client reconnects to server from new IP: 10.0.0.5
5. State sync layer: "Oh, this session still exists!"
6. Resume from where we left off
Duration: ~1-2 seconds (reconnect time)
```

---

## The Critical Realization

**The terminal state lives on the SERVER, not in the connection!**

Just like:
- You can close mosh client, SSH back, run `mosh-client IP PORT`, reconnect to same session
- Server keeps state alive
- Session key unchanged
- Terminal state preserved

**TCP does the same thing, just automatically:**
- Connection drops
- Client auto-reconnects
- Same session key
- State preserved
- Resumes

---

## So TCP DOES Support Roaming!

**Just with different trade-offs:**

| Aspect | UDP Roaming | TCP Roaming |
|--------|-------------|-------------|
| Reconnect time | ~0ms (next packet) | ~1-2s (TCP handshake) |
| User experience | Seamless | Brief pause |
| Firewall friendly | ❌ Often blocked | ✅ Usually works |
| VPN friendly | ⚠️ Sometimes issues | ✅ Works |
| State preserved | ✅ Yes | ✅ Yes |
| Session survives | ✅ Yes | ✅ Yes |

---

## What I Got Wrong

**My Review Said:**
> "TCP cannot roam - conflicts with Mosh's philosophy"

**Should Have Said:**
> "TCP roams differently - brief reconnect vs seamless, but session survives"

**This is a FEATURE, not a bug!**
- Trade 1-2 seconds of reconnect time
- For ability to work through restrictive firewalls
- Session state still preserved (Mosh's real magic)

---

## The Real Mosh Philosophy

Looking at the Mosh website/papers, the core innovation is:

**NOT**: "UDP lets you roam"
**BUT**: "State synchronization lets you roam"

The transport is just a means to an end. UDP was chosen because:
1. Simpler for roaming (no reconnect)
2. Good enough for most use cases

But TCP is **still valid** for the use case:
- "My network blocks UDP"
- "I'm behind a corporate firewall"
- "My VPN doesn't pass UDP"

---

## Revised Assessment

### Does TCP Belong in Mosh?

**YES** - if the trade-offs are documented:

**Pros:**
- Works through restrictive networks
- Session state preserved (core Mosh feature)
- Automatic reconnection
- Still better than SSH (state sync, local echo, etc.)

**Cons:**
- 1-2 second pause when IP changes (vs 0ms for UDP)
- No transparent roaming (user notices brief disconnect)
- More complex code

---

## What Users Get

**Mosh with UDP:**
- "I can switch from WiFi to cellular with zero interruption"
- Perfect for mobile users, coffee shops, travel

**Mosh with TCP:**
- "I can use Mosh through my corporate VPN that blocks UDP"
- "Brief pause when switching networks, but session survives"
- Perfect for enterprise, restrictive networks

---

## Corrected Maintainer Questions

**Original Review:** "Why TCP when Mosh is for roaming?"

**Should Ask:**
1. ✅ Does TCP preserve session state? **YES**
2. ✅ Does reconnection work automatically? **YES**
3. ✅ Are trade-offs documented? **NEEDS WORK**
4. ✅ Is there a real use case? **YES - restricted networks**
5. ⚠️ Is the code quality good? **YES (after fixes)**
6. ⚠️ Cross-platform tested? **NO - needs work**

---

## My Mistake

I focused on "TCP connections break" without understanding:
- **Mosh sessions don't break** (state on server)
- **Auto-reconnect is the solution**
- **This is exactly what happens with UDP roaming** (state sync handles gaps)

The state synchronization protocol is **designed to handle packet loss and delays**. Whether that delay is:
- UDP: 100ms packet loss
- TCP: 2000ms reconnection

The protocol handles it the same way!

---

## Revised Verdict

**Original:** "TCP conflicts with Mosh philosophy - don't merge"

**Corrected:** "TCP is a valid transport option with different trade-offs"

**Remaining Issues:**
1. ✅ Philosophy fit: **ACTUALLY GOOD** (I was wrong)
2. ⚠️ Cross-platform testing: **STILL NEEDED**
3. ⚠️ Blocking usleep: **STILL NEEDS FIX**
4. ✅ Use case: **VALID** (restrictive networks)
5. ⚠️ Documentation: **NEEDS "roaming = reconnect" explanation**

---

## What Documentation Should Say

**Bad (my review):**
> "TCP transport doesn't support roaming"

**Good (accurate):**
> "TCP transport supports roaming with automatic reconnection. When your
> IP address changes, there will be a 1-2 second pause as the connection
> re-establishes, but your session state is fully preserved. This is
> different from UDP which switches instantly, but the end result is the
> same: your session survives network changes."

---

## Apology

You were right, I was wrong. TCP roaming IS possible in Mosh because:
- Session state lives on server
- Auto-reconnect works
- Brief pause is acceptable trade-off for firewall traversal

The implementation makes much more sense now. My critical review was too harsh on the philosophical fit.

**Revised score: 7/10** (was 4/10)
- Philosophy: ✅ GOOD (I was wrong)
- Use case: ✅ VALID
- Code quality: ✅ GOOD (after bug fixes)
- Still needs: cross-platform testing, remove blocking calls

I apologize for the overly critical assessment of the architectural fit. The TCP implementation is more aligned with Mosh's goals than I initially understood.
