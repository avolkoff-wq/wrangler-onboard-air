"""Print files for the own snap-lid box: WA_BOX=cust|cust_adc python3 export_cust_stl.py -> box_<tag>_body.stl, box_<tag>_lid.stl.
Print: PETG or ASA, 0.2 mm layers, 4 walls; body open side up, lid upside down (plate on the bed)."""
import os
os.environ.setdefault("WA_BOX", "cust")
import cadquery as cq
import layout_box as L
cq.exporters.export(L.box, f"box_{L.TAG}_body.stl", tolerance=0.05, angularTolerance=0.2)
cq.exporters.export(L.lid, f"box_{L.TAG}_lid.stl", tolerance=0.05, angularTolerance=0.2)
print("ok", L.TAG, L.W, L.L, L.HB + L.HL)
