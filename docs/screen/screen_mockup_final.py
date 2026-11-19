"""Final mock-up sheets with scenery, per the latest decisions."""
import importlib.util, sys
def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path); mod = importlib.util.module_from_spec(spec); sys.modules[name] = mod; spec.loader.exec_module(mod); return mod
m = load("m", "sim2.py"); n = load("n", "sim3.py")
P = [("HIGHWAY", 2.5, 1.0), ("ROUGH ROAD", 1.8, 2.5), ("GRAVEL", 1.5, 2.5), ("TRAIL", 1.2, 2.5),
     ("ROCK", 1.0, 2.5), ("SAND", 0.8, 2.5), ("RECOVERY", 0.5, 0.8), ("CUSTOM", 2.0, 2.05)]
for portrait, cols, fn in ((False, 3, "final_landscape.png"), (True, 6, "final_portrait.png")):
    T = [m.frame(n.idle2(a, t, p, portrait=portrait), f"{a} {t:.1f}, actual {p:.1f}") for a, t, p in P]
    T += [m.frame(n.idle2("TRAIL", 1.2, 0.0, portrait=portrait), "0 bar: CONNECT SYSTEM"),
          m.frame(m.idle("CUSTOM", 2.0, edit=True, portrait=portrait), "CUSTOM step 1: pressure"),
          m.frame(m.idle("CUSTOM", 2.0, edit=True, portrait=portrait, wheels=2), "CUSTOM step 2: wheels"),
          m.frame(m.run("SAND", 2.5, 0.8, 1.6, "VENT", portrait=portrait, starts=0, vents=1), "Deflate: blue"),
          m.frame(m.run("HIGHWAY", 0.8, 2.5, 1.9, "PUMP", portrait=portrait), "Inflate: amber"),
          m.frame(m.run("HIGHWAY", 0.8, 2.5, 2.3, "MEASURE", portrait=portrait), "Pause: measuring")]
    m.sheet(T, cols, fn)
