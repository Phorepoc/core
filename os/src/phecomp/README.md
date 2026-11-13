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
  redraw bracket: each window group draws into its own off-screen target (one RSgImage of the current screen size, ARGB
  premultiplied, with a DirectGDI context); drawing outside a bracket (root window, sprites) goes to a "rest" target.
  `CHANGETRACKING` makes the window server draw every window whole, covered or not;
- composes in `End()`: the rest, then every group back to front (the order from `SiblingOrderChanged`) through the next stage's
  `MWsDrawableSourceProvider`; when both `PHECOMPTILE` apps run, those two are drawn above the rest, side by side at half
  width, the front one framed as focused, each with a darker outer band as its drag handle;
- memory: front to back, the groups down to the first named one that covers the screen keep their targets; the ones behind it
  free theirs and draw into a 1x1 sink (tiles never), and repaint (`MWsWindow::Invalidate`) once they are in front again. A
  group that shows no window frees its target; a target of the wrong size (the UI turned) is re-made;
- stale pixels: when a top-level window hides, moves or goes, its old place is cleared in the group's target and the group's
  other windows repaint it; a hidden nested window has its parent repaint the place;
- pointer (tiling only): the window server's pointer pre-processing (core `toolchain/shims/wserv_board/pointer_nga.cpp`) asks
  the first stage for `MPhePointerFilter` (type 0xE7017C02). A press in the focused tile goes to its app, mapped to the app's
  full-screen coordinates (moves and the release follow the press); a press in the other tile only focuses it (click-to-focus:
  a window server client thread of the stage brings that group to the front); a press on a tile's band drags the tile;
  everything else is dropped. The digitiser's coordinates are portrait: a landscape mode is handled as the portrait one turned.

The DirectGDI graphics context is the 2010 standard plug-in's (`directgdigcwrapper.cpp`, `utils.cpp`, EPL-1.0), compiled
from the Symbian source tree, not copied.

Seen as a side effect: windows drawn with an alpha channel over others (the Notes editor's lined page and its text, the
middle softkey clock) come out right in compose mode, while the standard NGA chain without `CHANGETRACKING` draws them white
or black.

## Next

MWsScene elements instead of the per-frame copy; z-order for overlays (the task switcher draws under the tiles); then the
window manager (`phewm`: placement, stacking, decorations).

UIDs: DLL 0xE7017C00, implementation 0xE7017C01. Licence: BSD-2-Clause (the three 2010 plug-in sources it builds: EPL-1.0).
