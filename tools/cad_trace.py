#!/usr/bin/env python3
"""Trace the front-view silhouettes of the owner's GBA CAD parts into assets/controller_cad.json.

Not part of the normal build (it needs the CAD repo and its cadquery environment); run it when
the CAD changes, then `python3 tools/gen_controller_svg.py`:

    ~/.local/bin/micromamba run -n cadquery python3 tools/cad_trace.py ../CAD

For each part: load lib/step/<part>.step (1:1 mm), move it to its place in the assembly
(lib/assembly_positions.py, translation only), tessellate it, project every triangle onto the
front plane (x right, y up; the front faces +z) and union them. That's the outline you see
looking straight at the console. For buttons only the top cap is traced (triangles within
`cap` mm of the part's highest point), because their base flanges sit hidden under the shell.
Pockets cut into the shell (speaker grille, START/SELECT recesses, top panel) aren't
silhouettes; gen_controller_svg.py draws those from the shell's own constants.

Output: coordinates in mm, CAD frame (origin at the shell centre, y up), simplified to 0.02 mm,
rounded to 0.01 mm; deterministic for a given CAD revision.
"""
import json
import os
import sys

PARTS = {   # output name: (step file, assembly key, cap depth in mm or None for the whole part)
    'body':        ('shell.front', 'shell', None),
    'shoulder-l':  ('shoulder_l', 'shoulder_l', None),
    'shoulder-r':  ('shoulder_r', 'shoulder_r', None),
    'trim-l':      ('edge_trim_l', 'edge_trim_l', None),
    'trim-r':      ('edge_trim_r', 'edge_trim_r', None),
    'lens':        ('screen_lens', 'screen_lens', None),
    'lcd':         ('screen_bezel', 'screen_bezel', None),
    'led':         ('power_led', 'power_led', None),
    'dpad':        ('dpad', 'dpad', 1.0),
    'btn-a':       ('button_a', 'button_a', 1.0),
    'btn-b':       ('button_b', 'button_b', 1.0),
    'btn-start':   ('start', 'start', 1.0),
    'btn-select':  ('select_button', 'select_button', 1.0),
}
MIN_AREA = 0.5   # mm^2: drops tessellation slivers and tiny tabs


def trace(cad):
    sys.path.insert(0, os.path.join(cad, 'lib'))
    import cadquery as cq
    from assembly_positions import POSITIONS
    from shapely.geometry import Polygon
    from shapely.ops import unary_union

    out = {}
    for name, (step, key, cap) in PARTS.items():
        pos = POSITIONS[key]['position']
        shape = cq.importers.importStep(os.path.join(cad, 'lib', 'step', step + '.step')).val()
        shape = shape.translate(cq.Vector(*pos))
        verts, tris = shape.tessellate(0.02, 0.1)
        zmax = shape.BoundingBox().zmax
        tri_polys = []
        for t in tris:
            if cap is not None and min(verts[i].z for i in t) < zmax - cap:
                continue
            p = Polygon([(verts[i].x, verts[i].y) for i in t])
            if p.area > 1e-9:
                tri_polys.append(p.buffer(0.002, join_style=2))
        u = unary_union(tri_polys).buffer(-0.002, join_style=2).simplify(0.02)
        geoms = [g for g in getattr(u, 'geoms', [u]) if g.area >= MIN_AREA]
        if len(geoms) != 1:
            raise SystemExit(f'cad_trace: {name}: expected one outline, got {len(geoms)}')
        g = geoms[0]
        out[name] = {
            'step': f'lib/step/{step}.step',
            'outline': [[round(x, 2), round(y, 2)] for x, y in g.exterior.coords[:-1]],
        }
    return out


def main():
    if len(sys.argv) != 2:
        raise SystemExit(__doc__)
    cad = os.path.abspath(sys.argv[1])
    data = {'about': 'Front-view outlines traced from the owner\'s GBA CAD (tools/cad_trace.py); '
                     'mm, origin at the shell centre, y up.',
            'parts': trace(cad)}
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    path = os.path.join(root, 'assets', 'controller_cad.json')
    with open(path, 'w') as f:
        json.dump(data, f, indent=1, sort_keys=True)
        f.write('\n')
    print('wrote', os.path.relpath(path, root), f'({len(data["parts"])} parts)')


if __name__ == '__main__':
    main()
