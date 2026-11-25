"""knob24_3d.html: the GLB from make_box.py in a three.js viewer (drag to turn). Run after make_box.py."""
import base64, pathlib
glb = base64.b64encode(pathlib.Path("knob24.glb").read_bytes()).decode()
html = """<!doctype html><html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Knob24 Box</title>
<style>:root{--bg:#f2efe8;--fg:#24261f;--mut:#6b6e62}@media (prefers-color-scheme:dark){:root{--bg:#15160f;--fg:#e9e6dc;--mut:#9a9c8f}}
html,body{margin:0;height:100%;background:var(--bg);color:var(--fg);font:14px system-ui,sans-serif}#c{position:fixed;inset:0;width:100%;height:100%;display:block}
#t{position:fixed;left:16px;top:12px;right:16px;pointer-events:none}#t b{font-size:16px}#t div{color:var(--mut);margin-top:4px}</style></head><body>
<div id="t"><b>Knob24: 2.4" TFT + EC11 over the ESP32 2-relay board</b><div>__SIZE__. Lid lowered to show the relay board. Display board sizes are estimates, check with a caliper.</div></div>
<canvas id="c"></canvas>
<script src="https://cdnjs.cloudflare.com/ajax/libs/three.js/r128/three.min.js"></script>
<script src="https://cdn.jsdelivr.net/npm/three@0.128.0/examples/js/loaders/GLTFLoader.js"></script>
<script src="https://cdn.jsdelivr.net/npm/three@0.128.0/examples/js/controls/OrbitControls.js"></script>
<script>
const GLB="__GLB__";
const cv=document.getElementById('c'),R=new THREE.WebGLRenderer({canvas:cv,antialias:true,alpha:true,preserveDrawingBuffer:true});
R.setPixelRatio(devicePixelRatio);window.S=new THREE.Scene();window.C=new THREE.PerspectiveCamera(30,1,1,5000);
S.add(new THREE.HemisphereLight(0xffffff,0x444433,0.9));const L=new THREE.DirectionalLight(0xffffff,0.8);L.position.set(150,300,250);S.add(L);
window.O=new THREE.OrbitControls(C,cv);O.enableDamping=true;
const b=Uint8Array.from(atob(GLB),c=>c.charCodeAt(0)).buffer;
new THREE.GLTFLoader().parse(b,'',g=>{const m=g.scene;S.add(m);m.traverse(o=>{if(o.isMesh){o.material.metalness=0;o.material.roughness=0.7;if(/display_pcb|relay_pcb|relay1|relay2|esp32/.test(o.name)){}}});
const bb=new THREE.Box3().setFromObject(m),c=bb.getCenter(new THREE.Vector3()),s=bb.getSize(new THREE.Vector3()).length();
O.target.copy(c);C.position.set(c.x+s*1.1,c.y+s*1.3,c.z+s*2.0);C.lookAt(c);window.__ready=1});
function fit(){const w=innerWidth,h=innerHeight;R.setSize(w,h,false);C.aspect=w/h;C.updateProjectionMatrix()}addEventListener('resize',fit);fit();
(function a(){requestAnimationFrame(a);O.update();R.render(S,C)})();
</script></body></html>"""
import re
size = "100.4 x 56.4 x 36.7 mm"
pathlib.Path("knob24_3d.html").write_text(html.replace("__GLB__", glb).replace("__SIZE__", size))
