# phecomp

The window compositor of Phorepoc OS, as a render stage of the NGA window server. One real screen, one compositor, any number
of windows: each top-level application (window group) becomes one compositor surface, and the compositor alone decides where it
goes, how big it is, and which one has the focus. Classic applications stay unmodified; a fixed-size one is scaled into its window
and its pointer input is mapped back.

Status: **v0.** By default (`PHECOMP=1`) the trace stage: a pass-through stage that sits first in the chain and logs what the window server tells a
compositor:

- the window tree (`MWsWindowTreeObserver`): window groups as they are created, named, restacked and released, and their top-level
  windows' extents and visibility;
- the draw brackets (`MWsDrawAnnotationObserver`): which window group each redraw belongs to, summed once a second.

Everything else is passed to the next stage, so the screen is exactly what the standard chain makes of it.

## Using it

An NGA image with `PHECOMP=1` at ROM time (`phe image rom -O nga=1 -O phecomp=1`). The window server's ini file then has

```
PLUGINS PHECOMP FLICKERBUFFER DISPLAY EXTENDEDFADER
RENDERSTAGES phecomp flickerbuffer display
[PHECOMP]
ID 0xE7017C01
```

It must be the first stage: the window server asks the first stage of the chain for the two observers. Its log lines start with
`PH: [phecomp]`.

## Compose mode (`PHECOMP=2`)

The wsini also gets `CHANGETRACKING` and `PHECOMPTILE <uid> <uid>` (default `10005902 10005907`, Calculator and Notes). The
stage then:

- hands the window server its own `MWsGraphicsContext` (a forwarder, `phecompfwd.inc`) whose target switches at every window
  redraw bracket: each window group draws into its own off-screen target (a square RSgImage of the screen's longer side, with
  alpha, made at the group's first redraw, freed with the group); drawing outside a bracket (root window, sprites) goes to a
  "rest" target. `CHANGETRACKING` makes the window server draw every window whole, covered or not;
- composes in `End()`: the rest, then every group back to front (the order from `SiblingOrderChanged`) through the next stage's
  `MWsDrawableSourceProvider`; when both `PHECOMPTILE` apps run, those two are drawn side by side, scaled to half the width, the
  front one framed as focused.

The off-screen target and the DirectGDI graphics context are the 2010 standard plug-in's (`rendertarget.cpp`,
`directgdigcwrapper.cpp`, `utils.cpp`, EPL-1.0), compiled from the Symbian source tree, not copied.

Not yet: pointer mapping (the window server still hit-tests its own full-screen layout), click-to-focus, drag; a group's stale
pixels after a window inside it hides or moves.

## Next

Pointer mapping before the window server's hit test; click-to-focus and drag; MWsScene elements instead of the per-frame
copy; then the window manager (`phewm`: placement, stacking, decorations).

UIDs: DLL 0xE7017C00, implementation 0xE7017C01. Licence: BSD-2-Clause (the three 2010 plug-in sources it builds: EPL-1.0).
