# OrcaSlicer Rendering Migration - Work in Progress

## Goal
Migrate G-code extrusion rendering from 8-vertex rectangular cross-section to 4-vertex diamond cross-section to match OrcaSlicer's approach.

## Status: INCOMPLETE - Geometry gaps need fixing

## What Works
1. ✅ Diamond cross-section is correctly oriented (visible in side view)
2. ✅ 4-vertex approach implemented with proper vertex sharing
3. ✅ Backface culling working correctly
4. ✅ Debug colors enabled for each face (Red=top, Yellow=right, Blue=bottom, Green=left)
5. ✅ Camera debug overlay shows when camera params set via CLI
6. ✅ Diagonal face normals calculated correctly

## Critical Issue: Black gaps between faces

**Symptom:** When viewing from top/angled views, the 4 faces render as separate colored stripes with black gaps between them instead of forming a solid diamond tube.

**Screenshots showing issue:**
- `/tmp/ui-screenshot-diamond_v2_top.png` - Shows 3 separate horizontal stripes (red, blue, green) with gaps
- `/tmp/ui-screenshot-diamond_v2_angled.png` - Shows 4 parallel colored lines with gaps, not a solid tube
- `/tmp/ui-screenshot-diamond_v2_side.png` - CORRECT - Shows solid diamond (blue left, green right, no gaps)

**Why side view works but top doesn't:**
Side view shows the cross-section end-on where all 4 vertices are at the same Z position, so faces naturally connect. Top/angled views show the tube lengthwise where faces should connect along their shared edges but don't.

## Root Cause Analysis

The faces ARE using shared vertices at their edges (verified in code), but they still render with gaps. Possible causes:

1. **Vertex positions might be slightly different** - Even though we reuse vertex indices, the actual positions might have floating-point precision issues
2. **Rasterization gaps** - TinyGL might not be rendering edge pixels for adjacent triangles
3. **Missing triangle strip optimization** - Currently using individual triangles, not connected strips
4. **Z-fighting or depth buffer precision** - Faces might be overlapping at edges causing rendering artifacts

## Implementation Details

### Vertex Layout (4-vertex diamond)
```
Cross-section viewed from front (end-on):
      UP (0)
       *
      / \
LEFT *   * RIGHT
(3)  \(1)/
      *
    DOWN (2)
```

### Face Connectivity
- Top face: UP(0) → RIGHT(1) - Normal: (perp_up + right) normalized
- Right face: RIGHT(1) → DOWN(2) - Normal: (right - perp_up) normalized
- Bottom face: DOWN(2) → LEFT(3) - Normal: (-perp_up - right) normalized
- Left face: LEFT(3) → UP(0) - Normal: (-right + perp_up) normalized

### Vertex Generation Strategy (OrcaSlicer approach)
- **First segment:** 4 prev vertices + 4 curr vertices = 8 total
- **Subsequent segments:** 2 new prev vertices (right, left) + 4 curr vertices = 6 total
  - Reuses prev_up and prev_down from previous segment's end_cap

### Memory Efficiency
- Old approach: ~8N + 8 vertices
- New approach: ~6N + 2 vertices
- Improvement: ~25% reduction

## Testing Commands

### Generate screenshots with debug colors:
```bash
# Top view (should show all 4 faces meeting at center)
./scripts/screenshot.sh helix-ui-proto test_top -p gcode-test \
  --gcode-file assets/single_line_test.gcode \
  --gcode-az 0 --gcode-el 89 --gcode-zoom 10 \
  --gcode-debug-colors --test

# Side view (cross-section - should show solid diamond)
./scripts/screenshot.sh helix-ui-proto test_side -p gcode-test \
  --gcode-file assets/single_line_test.gcode \
  --gcode-az 90 --gcode-el 0 --gcode-zoom 10 \
  --gcode-debug-colors --test

# Angled view (should show solid tube with 4 colored faces)
./scripts/screenshot.sh helix-ui-proto test_angled -p gcode-test \
  --gcode-file assets/single_line_test.gcode \
  --gcode-az 45 --gcode-el 30 --gcode-zoom 5 \
  --gcode-debug-colors --test

# Bottom view (should show 2 back faces with different shading)
./scripts/screenshot.sh helix-ui-proto test_bottom -p gcode-test \
  --gcode-file assets/single_line_test.gcode \
  --gcode-az 0 --gcode-el -89 --gcode-zoom 10 \
  --gcode-debug-colors --test
```

### Expected Results
- **Top view:** Single horizontal line showing top vertex running along extrusion, with adjacent faces meeting seamlessly
- **Side view:** Solid diamond shape (already working)
- **Angled view:** 4 colored faces forming a continuous diamond tube
- **Bottom view:** Bottom face visible with proper backface culling

## Next Steps to Fix Gaps

### Option 1: Investigate TinyGL rasterization
Check if TinyGL has edge fill rules that might leave gaps. May need to enable edge anti-aliasing or adjust viewport settings.

### Option 2: Use triangle strips instead of individual triangles
Currently using `GL_TRIANGLE_STRIP` but generating strips per-face. Try generating one continuous strip around the entire tube.

### Option 3: Add edge overlap/expansion
Slightly expand face geometry to ensure edges overlap by 1 pixel to eliminate rasterization gaps.

### Option 4: Check vertex quantization
The `quantize_vec3()` function might be introducing precision loss that creates micro-gaps. Test with quantization disabled.

### Option 5: Compare with OrcaSlicer's rendering setup
Check OrcaSlicer's OpenGL settings - they might enable polygon offset, depth bias, or other settings to prevent gaps.

## Files Modified

- `include/gcode_geometry_builder.h:322` - Changed TubeCap from 8 to 4 vertices
- `src/gcode_geometry_builder.cpp:457-705` - Complete rewrite of `generate_ribbon_vertices()`
- `src/gcode_tinygl_renderer.cpp:9` - Added `runtime_config.h` include
- `src/gcode_tinygl_renderer.cpp:565-584` - Camera debug overlay now shows when CLI params set

## Debug Colors Reference
```cpp
TOP = 0xFF0000    // Red
RIGHT = 0xFFFF00  // Yellow
BOTTOM = 0x0000FF // Blue
LEFT = 0x00FF00   // Green
```

## Key Code Locations

### Vertex generation (first segment)
`src/gcode_geometry_builder.cpp:545-561` - Generates all 4 prev vertices

### Vertex generation (subsequent segments)
`src/gcode_geometry_builder.cpp:557-580` - Generates only 2 new prev vertices, reuses up/down

### Face triangle generation
`src/gcode_geometry_builder.cpp:636-683` - Generates triangles for all 4 diamond faces

### Normal calculation
`src/gcode_geometry_builder.cpp:531-547` - Computes diagonal face normals and averaged vertex normals

## Important Notes

1. **ALWAYS use screenshot.sh properly:** Pass all binary args through the script, don't run binary separately
2. **NEVER assume screenshots match expectations:** Always VIEW screenshots carefully and describe what you ACTUALLY see
3. **Test incrementally:** Test one face at a time, verify it works from all angles before adding more
4. **Side view is ground truth:** The side view shows the true cross-section shape - use it to verify geometry
5. **Debug colors are essential:** Don't disable debug colors until geometry is proven correct from ALL angles
6. **Camera overlay is automatic:** Shows whenever `--gcode-az/el/zoom` flags are used, no need for `-vv`

## Methodology for Future Work

1. **Isolate the problem:**
   - Comment out 3 faces, test only 1 face from 4+ angles
   - Verify single face has NO gaps and correct shape from all angles
   - Only proceed when single face is perfect

2. **Add faces incrementally:**
   - Add 2nd face (adjacent to first), test shared edge has no gap
   - Add 3rd face, verify both shared edges work
   - Add 4th face to close the tube

3. **Visual verification checklist for EACH angle:**
   - Top view: Should see narrow colored stripe(s), NO black gaps between colors
   - Bottom view: Should see underside faces with darker shading, NO black gaps
   - Side view: Should see diamond shape with 2 adjacent faces, NO gaps at vertices
   - Angled view: Should see all visible faces, NO black gaps between them

4. **Never proceed with issues:**
   - If ANY view shows black gaps → STOP, fix before adding more faces
   - If backface culling wrong (seeing through geometry) → STOP, fix winding order
   - If colors don't match expected face → STOP, verify debug color assignments

## Success Criteria

Implementation is COMPLETE when:
- ✅ All 4 faces render as a solid tube with NO gaps from any angle
- ✅ Debug colors clearly show each face (can toggle with `--gcode-debug-colors`)
- ✅ Backface culling works correctly (can't see through tube)
- ✅ Visual comparison with OrcaSlicer shows same diamond orientation
- ✅ Memory usage reduced by ~25% (verify with vertex count logging)
- ✅ Documentation updated in HANDOFF.md
