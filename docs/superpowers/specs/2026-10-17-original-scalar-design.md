# Original CT scalar native port

The user's current priority is the original RT 8.02 camera/control architecture.
First-person face hiding is optional and must not gate this port. Keep the
existing Explore runtime stable while implementing the original native path.

This increment ports the original scalar decisions and arithmetic to C:
`HEAD_Y_WRITE` view policy without its model operations, OTS/FP coordinate
expressions, pitch multiplier, focus zoom/unzoom, guest-house water clamp and
firing-origin selection. Use original variable names and source line provenance.
The original's default `fpaimflag=0` must retain OTS while aiming. Wall-distance
multipliers change coordinates, not the view-policy flag.

Expose individual axis expressions. Do not invent a frame wrapper: CAM_WRITES
uses the previous pitch/aim multipliers for X/Y and updates them during Z;
TP_INTERACTABLE_COORDS uses the existing multipliers for all axes. Its object,
popup and scene gates are not yet ported, so these expressions alone cannot
claim to fix the A-button prompt. Preserve x87 float-store boundaries using
64-bit-significand long double and explicit float results; forbid fast math.
The expected floating-point environment is x87 extended precision, round-nearest.
Other precision/rounding modes need runtime validation before hook activation.

This increment is source/build-only: no game memory hooks, no new installer,
no runtime/FPS claims. Full CAM dispatch, weapon-block transitions, collision,
auto-disable producers, object identity/lifetime and instruction-preserving
hook integration remain required. Historical AOB matches are useful mapping
evidence, not proof of callable object semantics. No face-hiding dependency.

Validate numeric vectors, state transitions and an optional independent oracle
extracted from the pinned, private CT. Compile the core for host tests and
Windows x64; integrate verification into the existing offline suite. Publish
source and current scope to `work` under the standing user authorization.
