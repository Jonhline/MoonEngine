### Brake lamp material lighting (14/10/2026)

C (PC GPU dependency): removed the global lamp==2 -> unlit override in
render3d. Callback 0 (00453698; MDLS dispatch 00455708) selects released/
pressed geometry and textures; material kind remains independently decoded.
The previous override was explicitly A and lit all triangles in the pressed
branch, including non-emissive lens areas. Authored kinds 0/5 still use their
existing unlit path; other materials retain scene light/specular/reflection.
This supersedes the earlier approximate full-brake-branch unlit treatment.

brake_material compares full framebuffer pixels of a lit material in normal
and pressed branches (identical), versus an authored unlit control (different).
Negative changes one output byte and fails. brake_material_focus loads both
real race Focus assets: focus_rs_02/fdeu0005 has 8 affected triangles;
focus_st170_jp_03/fdeu0007 has 66. Complete car pixels match an independently
selected pressed stream with original material kinds retained. car_brake_branch
and negative preserve the MIRA join regression: 5/5 focused tests passed.
Evidence: build/brake-material-final-tests.log and Testing/Temporary/LastTest.log.
Full PS2 light/texture/glow appearance remains A pending original image
comparison; these tests do not establish whole-car PS2 raster equivalence.
No pedal, physics or material texture values changed. Local executable updated.
