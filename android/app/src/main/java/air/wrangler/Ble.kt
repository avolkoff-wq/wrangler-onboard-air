package air.wrangler

import android.annotation.SuppressLint
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattCallback
import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothGattDescriptor
import android.bluetooth.BluetoothManager
import android.bluetooth.BluetoothProfile
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanFilter
import android.bluetooth.le.ScanResult
import android.bluetooth.le.ScanSettings
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.os.Build
import android.os.Handler
import android.os.Looper
import android.os.ParcelUuid
import java.util.ArrayDeque
import java.util.UUID

/**
 * BLE link to the DinMeter (firmware ble.h). The DinMeter holds all state: this class only
 *  - reads STATE (36 bytes) and SCREEN (display list frames, dlist.h) notifications,
 *  - writes INPUT (1 byte: 1 left, 2 right, 3 press, 4 medium press, 5 hold), which needs a bonded, encrypted link.
 * GATT operations go through one queue (Android runs one at a time). Reconnects by itself.
 */
@SuppressLint("MissingPermission")   // MainActivity asks for BLUETOOTH_SCAN / BLUETOOTH_CONNECT before start()
class Ble(private val ctx: Context, private val listener: Listener) {
    interface Listener {
        fun onStatus(text: String)
        fun onState(s: State)
        fun onFrame(f: Frame)
        fun onNeedPairing()
        fun onIcon(key: Int, strokes: List<Frame.IconItem>)
        fun onMenu(m: MenuList)
    }

    data class State(val screen: Int, val menuPos: Int, val preset: Int, val state: Int, val err: Int, val armStage: Int,
                     val k1: Boolean, val valve: Boolean, val noAir: Boolean, val inBand: Boolean, val offTarget: Boolean,
                     val fake: Boolean, val demo: Boolean, val bar: Float, val est: Float, val target: Float, val staticBar: Float,
                     val aux1: Float, val runMs: Long, val phaseMs: Long, val seq: Long, val adcFails: Long, val p0: Float)

    companion object {
        val SVC: UUID = UUID.fromString("7a1f0001-5c0e-4b6a-9d2f-57a1e5a1c0de")
        val STATE: UUID = UUID.fromString("7a1f0002-5c0e-4b6a-9d2f-57a1e5a1c0de")
        val INPUT: UUID = UUID.fromString("7a1f0003-5c0e-4b6a-9d2f-57a1e5a1c0de")
        val SCREEN: UUID = UUID.fromString("7a1f0004-5c0e-4b6a-9d2f-57a1e5a1c0de")
        val CCCD: UUID = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb")
        const val LEFT = 1; const val RIGHT = 2; const val PRESS = 3; const val MID = 4; const val HOLD = 5; const val BACK = 6; const val ICONS = 9
    }

    private val main = Handler(Looper.getMainLooper())
    private val adapter = (ctx.getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager).adapter
    private var gatt: BluetoothGatt? = null
    private var device: BluetoothDevice? = null
    private var scanning = false
    private var running = false
    private var ready = false
    private val ops = ArrayDeque<() -> Unit>()
    private var busy = false
    private var pendingInput: Int? = null
    private val chunks = HashMap<Int, Array<ByteArray?>>()   // frame id -> chunks

    val connected get() = ready

    fun start() {
        if (running) return
        running = true
        ctx.registerReceiver(bondReceiver, IntentFilter(BluetoothDevice.ACTION_BOND_STATE_CHANGED))
        scan()
    }

    fun stop() {
        running = false
        try { ctx.unregisterReceiver(bondReceiver) } catch (_: Exception) {}
        stopScan()
        main.removeCallbacksAndMessages(null)
        gatt?.disconnect(); gatt?.close(); gatt = null
        ready = false; ops.clear(); busy = false; chunks.clear()
    }

    /** one knob / button event to the DinMeter */
    fun send(code: Int) {
        val g = gatt ?: return
        if (!ready) return
        val c = g.getService(SVC)?.getCharacteristic(INPUT) ?: return
        enqueue { write(g, c, byteArrayOf(code.toByte())) ; pendingInput = code }
    }

    // ---------- scan ----------
    private val scanCb = object : ScanCallback() {
        override fun onScanResult(callbackType: Int, result: ScanResult) {
            if (!scanning) return
            stopScan()
            connect(result.device)
        }
        override fun onScanFailed(errorCode: Int) { status("Scan failed ($errorCode), retrying"); retryLater() }
    }

    private fun scan() {
        if (!running) return
        if (!adapter.isEnabled) { status("Bluetooth is off"); retryLater(); return }
        val bonded = adapter.bondedDevices.firstOrNull { it.name == "WranglerAir" }
        if (bonded != null && device == null) device = bonded
        status("Searching for WranglerAir ...")
        scanning = true
        adapter.bluetoothLeScanner?.startScan(
            listOf(ScanFilter.Builder().setServiceUuid(ParcelUuid(SVC)).build()),
            ScanSettings.Builder().setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY).build(), scanCb)
        main.postDelayed({ if (scanning) { stopScan(); status("Not found. Is BT ON in the DinMeter menu BLUETOOTH?"); retryLater() } }, 15000)
    }

    private fun stopScan() {
        if (scanning) { scanning = false; try { adapter.bluetoothLeScanner?.stopScan(scanCb) } catch (_: Exception) {} }
    }

    private fun retryLater() { main.postDelayed({ if (running && gatt == null) scan() }, 3000) }

    // ---------- GATT ----------
    private fun connect(d: BluetoothDevice) {
        device = d
        status("Connecting ...")
        gatt = d.connectGatt(ctx, false, gattCb, BluetoothDevice.TRANSPORT_LE)
    }

    private val gattCb = object : BluetoothGattCallback() {
        override fun onConnectionStateChange(g: BluetoothGatt, st: Int, newState: Int) = main.post {
            if (newState == BluetoothProfile.STATE_CONNECTED) {
                status("Connected, reading the screen ...")
                g.requestConnectionPriority(BluetoothGatt.CONNECTION_PRIORITY_HIGH)
                g.requestMtu(517)
            } else {
                ready = false; ops.clear(); busy = false; chunks.clear()
                g.close(); if (gatt == g) gatt = null
                status("Disconnected, reconnecting ...")
                retryLater()
            }
        }.let { }

        override fun onMtuChanged(g: BluetoothGatt, mtu: Int, st: Int) = main.post { g.discoverServices() }.let { }

        override fun onServicesDiscovered(g: BluetoothGatt, st: Int) = main.post {
            val svc = g.getService(SVC)
            if (svc == null) { status("No Wrangler service on this device"); g.disconnect(); return@post }
            for (id in listOf(STATE, SCREEN)) {
                val c = svc.getCharacteristic(id) ?: continue
                enqueue { subscribe(g, c) }
            }
            svc.getCharacteristic(STATE)?.let { c -> enqueue { g.readCharacteristic(c) } }
            enqueue { ready = true; status(if (device?.bondState == BluetoothDevice.BOND_BONDED) "Connected" else "Connected (watch only until paired)"); done() }
        }.let { }

        override fun onDescriptorWrite(g: BluetoothGatt, d: BluetoothGattDescriptor, st: Int) = main.post { done() }.let { }

        override fun onCharacteristicWrite(g: BluetoothGatt, c: BluetoothGattCharacteristic, st: Int) = main.post {
            if (c.uuid == INPUT && (st == 5 || st == 15 || st == 137)) {   // insufficient authentication / encryption
                listener.onNeedPairing()
                if (device?.bondState != BluetoothDevice.BOND_BONDING) device?.createBond()
            } else pendingInput = null
            done()
        }.let { }

        override fun onCharacteristicRead(g: BluetoothGatt, c: BluetoothGattCharacteristic, value: ByteArray, st: Int) =
            main.post { handle(c.uuid, value); done() }.let { }

        @Deprecated("API < 33") override fun onCharacteristicRead(g: BluetoothGatt, c: BluetoothGattCharacteristic, st: Int) {
            if (Build.VERSION.SDK_INT < 33) { @Suppress("DEPRECATION") val v = c.value ?: ByteArray(0); main.post { handle(c.uuid, v); done() } }
        }

        override fun onCharacteristicChanged(g: BluetoothGatt, c: BluetoothGattCharacteristic, value: ByteArray) =
            main.post { handle(c.uuid, value) }.let { }

        @Deprecated("API < 33") override fun onCharacteristicChanged(g: BluetoothGatt, c: BluetoothGattCharacteristic) {
            if (Build.VERSION.SDK_INT < 33) { @Suppress("DEPRECATION") val v = c.value ?: ByteArray(0); main.post { handle(c.uuid, v) } }
        }
    }

    private val bondReceiver = object : BroadcastReceiver() {
        override fun onReceive(c: Context, i: Intent) {
            val d: BluetoothDevice? = if (Build.VERSION.SDK_INT >= 33) i.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE, BluetoothDevice::class.java)
                                      else @Suppress("DEPRECATION") i.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE)
            if (d?.address != device?.address) return
            when (i.getIntExtra(BluetoothDevice.EXTRA_BOND_STATE, -1)) {
                BluetoothDevice.BOND_BONDED -> { status("Paired"); pendingInput?.let { send(it) } }
                BluetoothDevice.BOND_NONE -> status("Pairing failed. Open PAIR A PHONE on the DinMeter and try again")
            }
        }
    }

    private fun subscribe(g: BluetoothGatt, c: BluetoothGattCharacteristic) {
        g.setCharacteristicNotification(c, true)
        val d = c.getDescriptor(CCCD) ?: run { done(); return }
        val v = BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE
        if (Build.VERSION.SDK_INT >= 33) g.writeDescriptor(d, v)
        else { @Suppress("DEPRECATION") d.value = v; @Suppress("DEPRECATION") g.writeDescriptor(d) }
    }

    private fun write(g: BluetoothGatt, c: BluetoothGattCharacteristic, v: ByteArray) {
        val t = BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT
        if (Build.VERSION.SDK_INT >= 33) g.writeCharacteristic(c, v, t)
        else { @Suppress("DEPRECATION") run { c.writeType = t; c.value = v; g.writeCharacteristic(c) } }
    }

    private fun enqueue(op: () -> Unit) { ops.add(op); if (!busy) next() }
    private fun next() { val op = ops.poll() ?: run { busy = false; return }; busy = true; op() }
    private fun done() { busy = false; next() }

    // ---------- data ----------
    private fun handle(id: UUID, v: ByteArray) {
        when (id) {
            STATE -> if (v.size >= 34) listener.onState(decodeState(v))
            SCREEN -> if (v.size >= 3) {
                val fid = v[0].toInt() and 0xFF; val idx = v[1].toInt() and 0xFF; val cnt = v[2].toInt() and 0xFF
                if (cnt == 0 || idx >= cnt) return
                val arr = chunks.getOrPut(fid) { arrayOfNulls(cnt) }
                if (arr.size != cnt) return
                arr[idx] = v.copyOfRange(3, v.size)
                if (arr.all { it != null }) {
                    chunks.keys.filter { it != fid }.forEach { chunks.remove(it) }
                    chunks.remove(fid)
                    val all = arr.fold(ByteArray(0)) { a, b -> a + b!! }
                    if (all.isNotEmpty() && all[0] == 'M'.code.toByte()) MenuList.decode(all)?.let { listener.onMenu(it) }
                    else if (all.isNotEmpty() && all[0] == 'I'.code.toByte()) Frame.decodeIcon(all)?.let { listener.onIcon(it.first, it.second) }
                    else Frame.decode(all)?.let { listener.onFrame(it) }
                }
                if (chunks.size > 4) chunks.clear()
            }
        }
    }

    private fun decodeState(b: ByteArray): State {
        fun s16(i: Int) = ((b[i].toInt() and 0xFF) or (b[i + 1].toInt() shl 8)).toShort().toInt()
        fun u32(i: Int) = (0 until 4).fold(0L) { a, k -> a or ((b[i + k].toLong() and 0xFF) shl (8 * k)) }
        val fl = b[7].toInt()
        fun u8(i: Int) = b[i].toInt() and 0xFF
        return State(u8(1), u8(2), u8(3), u8(4), u8(5), u8(6),
            fl and 1 != 0, fl and 2 != 0, fl and 4 != 0, fl and 8 != 0, fl and 16 != 0, fl and 32 != 0, fl and 64 != 0,
            s16(8) / 1000f, s16(10) / 1000f, s16(12) / 1000f, s16(14) / 1000f, s16(16) / 100f,
            u32(18), u32(22), u32(26), u32(30), if (b.size >= 36) s16(34) / 1000f else 0f)
    }

    private fun status(t: String) = listener.onStatus(t)
}
