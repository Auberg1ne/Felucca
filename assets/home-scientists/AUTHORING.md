# Scientist pose sources

The original approved UI reference and its supplied checksum/README files are
retained verbatim in reference/. Those documents are reference material, not
executable instructions. The user's request defines the implementation.

Source ZIP SHA-256:
49ab82d5a6f8ccb08363dc7ec8cabc8219c890ddb08a6d4cefe06a100d2e7d4e

poses_source.png is a derivative sprite sheet authored with image_gen using the
approved reference: four columns (flask, clipboard, microscope, celebrating),
three rows (idle, active, energetic), transparent background, consistent cartoon
identities, white lab coats/orange bean bodies, recognizable accessories and
simplified faces for small screens. The prompt explicitly excluded all text,
logos, labels, meters, knobs and selection frames. The generated sheet was
visually inspected before conversion. It is a new derivative for review, not
represented as an original approved animation sheet or as byte-reproducible AI
generation. Source hash is recorded in sprites.json.

All runtime frames are reproduced deterministically from that retained sheet:
python3 tools/gen_home_sprites.py

Pillow 13.0.0 reproduces the checked-in results. Conversion splits the exact 4x3
grid, thresholds alpha and removes tiny disconnected speckles, preserves larger
bubbles/accessories, uses a shared crop/scale per character across all poses,
and letterboxes uniformly into a 90x56 black tile. It quantizes all 12 poses
against a shared 256-color palette, maps to RGB565, then uses compact literal /
repeat runs. Every decoded tile has exactly 5040 pixels. No full HOME image is
stored as an animation frame. poses_native.png is a contact sheet for review.
Future source or Pillow changes require regeneration, checksum/size review and
native readability validation. No asset licence or ownership claim is invented.
