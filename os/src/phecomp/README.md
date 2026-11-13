# phecomp

The window compositor of Phorepoc OS, as a render stage of the NGA window server. One real screen, one compositor, any number
of windows: each top-level application (window group) becomes one compositor surface, and the compositor alone decides where it
goes, how big it is, and which one has the focus. Classic applications stay unmodified; a fixed-size one is scaled into its window
and its pointer input is mapped back.

Status: **v0, the trace stage.** A pass-through stage that sits first in the chain and logs what the window server tells a
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

## Next

Per-window-group offscreen surfaces, composed as scaled elements; click-to-focus, drag-to-move and pointer mapping; then the
window manager (`phewm`: placement, stacking, decorations).

UIDs: DLL 0xE7017C00, implementation 0xE7017C01. Licence: BSD-2-Clause.
