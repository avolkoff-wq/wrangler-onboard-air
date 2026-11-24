import asyncio, sys, struct
from bleak import BleakScanner, BleakClient
SVC="7a1f0001-5c0e-4b6a-9d2f-57a1e5a1c0de"; ST="7a1f0002-5c0e-4b6a-9d2f-57a1e5a1c0de"; IN="7a1f0003-5c0e-4b6a-9d2f-57a1e5a1c0de"
def dec(b):
    ver,scr,mp,pre,st,err,arm,fl=b[:8]; bar,est,tgt,stat,aux=struct.unpack_from('<hhhhh',b,8); run,ph,seq,adc=struct.unpack_from('<IIII',b,18)
    return f"v{ver} screen={scr} menu={mp} preset={pre} state={st} err={err} arm={arm} flags={fl:07b} bar={bar/1000:.3f} target={tgt/1000:.2f} seq={seq} adc_fail={adc}"
async def main():
    d = await BleakScanner.find_device_by_name("WranglerAir", timeout=12)
    if not d:
        ds = await BleakScanner.discover(timeout=6, service_uuids=[SVC]); print("by service:", ds); d = ds[0] if ds else None
    if not d: print("NOT FOUND"); return
    print("found", d.name)
    async with BleakClient(d) as c:
        print("connected, services:", [s.uuid for s in c.services if s.uuid.startswith("7a1f")])
        v = await c.read_gatt_char(ST); print("read", len(v), dec(v))
        n = []
        await c.start_notify(ST, lambda h, data: n.append(bytes(data)))
        await asyncio.sleep(6)
        print("notifies in 6 s:", len(n)); 
        if n: print("last", dec(n[-1]))
        codes = [int(x) for x in sys.argv[sys.argv.index("write")+1:]] if "write" in sys.argv else []
        paired = True
        if codes:
            try:
                await c.write_gatt_char(IN, bytes([codes[0]]), response=True); print("write", codes[0], "ok (already bonded)"); codes = codes[1:]
            except Exception as e:
                paired = False
                print("write refused:", str(e)[:80], "-> type the PIN on the Mac now, waiting 40 s on this link")
                await asyncio.sleep(40)
    if codes:   # a fresh link after the pairing: the bond must give an encrypted link without a PIN
        await asyncio.sleep(2)
        async with BleakClient(d) as c:
            n = []
            await c.start_notify(ST, lambda h, data: n.append(bytes(data)))
            await asyncio.sleep(1.5)
            if n: print("before", dec(n[-1]))
            for code in codes:
                try:
                    await c.write_gatt_char(IN, bytes([code]), response=True); print("write", code, "ok")
                except Exception as e: print("write", code, "refused:", type(e).__name__, str(e)[:100])
                await asyncio.sleep(1.0)
            await asyncio.sleep(1)
            if n: print("after ", dec(n[-1]))
asyncio.run(main())
