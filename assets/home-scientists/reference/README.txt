COFFEE GRAINS HOME SCREEN ART REFERENCE

The included original 1254x1254 image is a VISUAL REFERENCE, not a ready-to-use 240x240 UI bitmap or animation spritesheet.
The 240x240 preview illustrates readability limits only.

Implement four independently renderable character sprites (CH1 flask, CH2 clipboard, CH3 microscope, CH4 cheering) with idle/active/energetic poses, while keeping a compact four-knob top row.
Do not bake text, knob values, meters or selection frames into sprite images.
Do not infer real knob parameter mappings from example labels/values in artwork.
Do not modify existing audio output, routing or navigation.
Use existing HOME meter API from bb fbd03 (commit bbfbd03).
Selected-channel frame must follow actual selected part and remain independent of audio level.
Check actual target memory budget; browser-only fallback if hardware fit unverified.
