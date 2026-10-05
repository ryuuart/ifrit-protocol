# Y2K materials

A desktop hyperportal with folded silver lettering, brushed aluminium
housing, smoked glass insets and coloured optical gel controls. The
classic Aqua specimen keeps its broad highlight and luminous lower edge
beside the deeper gel treatment.

The interface references are Apple's [Aqua introduction](https://www.apple.com/newsroom/2000/01/05Apple-Unveils-Mac-OS-X/)
and the [Mac OS X 10.0.4 appearance and QuickTime screenshots](https://guidebookgallery.org/screenshots/macosx100/).
The chrome states a metallic surface with a shaped normal field; the page
carries no light, so it paints its silver flat. It is an authored material study, not a reconstruction of one
operating system screen.

The material recipes are shared with the chrome lettering, text paints
and surface components sketches:

- `Metal.h` supplies the silver, brushed housing and smoked inset. Fold
  depth and roughness set the silver's surface normal and polish.
- `ChromeType.h` adds a cast shadow, bevel and keyline to the silver
  face. Its sunset finish remains a separate painted horizon.
- `Aqua.h` supplies the opaque gel body, lower caustic, narrow rim and
  inset highlight lens. Body and lens share the same options; the orb
  uses a narrower lens.
- `Gloss.h` shapes a coverage-based light band around a silhouette.

The material fields are static. The desktop ground and page backplate
are cached independently of the live ticker. Only the entrance poses
and marquee move.
