package air.wrangler

import android.Manifest
import android.app.Activity
import android.bluetooth.BluetoothAdapter
import android.content.Intent
import android.content.pm.PackageManager
import android.graphics.Color
import android.graphics.drawable.GradientDrawable
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.os.VibrationEffect
import android.os.Vibrator
import android.os.VibratorManager
import android.view.Gravity
import android.view.MotionEvent
import android.view.View
import android.view.ViewGroup.LayoutParams.MATCH_PARENT
import android.view.ViewGroup.LayoutParams.WRAP_CONTENT
import android.widget.Button
import android.widget.LinearLayout
import android.widget.TextView

/**
 * One screen: the DinMeter screen (same as on the device) and the knob as buttons.
 *  <  >   one knob step (or swipe across the screen)
 *  PRESS  tap = press; held 0.5-2 s = medium press; held 2 s = hold (menu / back), like the knob button
 * Everything else (menus, confirmations, protections) is in the DinMeter.
 */
class MainActivity : Activity(), Ble.Listener {
    private lateinit var ble: Ble
    private lateinit var screen: ScreenView
    private lateinit var status: TextView
    private lateinit var info: TextView
    private lateinit var press: Button
    private val main = Handler(Looper.getMainLooper())
    private val vib by lazy { (getSystemService(VIBRATOR_MANAGER_SERVICE) as VibratorManager).defaultVibrator }

    override fun onCreate(b: Bundle?) {
        super.onCreate(b)
        window.statusBarColor = Color.BLACK; window.navigationBarColor = Color.BLACK
        val dp = resources.displayMetrics.density
        val root = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL; setBackgroundColor(Color.BLACK); setPadding((12 * dp).toInt(), (12 * dp).toInt(), (12 * dp).toInt(), (16 * dp).toInt()) }
        status = TextView(this).apply { setTextColor(Color.LTGRAY); textSize = 14f; text = "Starting ..." }
        info = TextView(this).apply { setTextColor(Color.GRAY); textSize = 12f }
        screen = ScreenView(this) { turn(it) }
        root.addView(status, LinearLayout.LayoutParams(MATCH_PARENT, WRAP_CONTENT))
        root.addView(info, LinearLayout.LayoutParams(MATCH_PARENT, WRAP_CONTENT))
        root.addView(screen, LinearLayout.LayoutParams(MATCH_PARENT, 0, 1f).apply { topMargin = (8 * dp).toInt(); bottomMargin = (12 * dp).toInt() })

        val row = LinearLayout(this).apply { orientation = LinearLayout.HORIZONTAL; gravity = Gravity.CENTER }
        fun btn(t: String, color: Int) = Button(this).apply {
            text = t; textSize = 22f; setTextColor(Color.WHITE); isAllCaps = false
            background = GradientDrawable().apply { cornerRadius = 18 * dp; setColor(color) }
        }
        val left = btn("<", 0xFF2A2A2A.toInt()).apply { setOnClickListener { turn(-1) } }
        val right = btn(">", 0xFF2A2A2A.toInt()).apply { setOnClickListener { turn(1) } }
        press = btn("PRESS", 0xFF1F5E1F.toInt())
        pressTouch()
        val h = (96 * dp).toInt(); val m = (6 * dp).toInt()
        row.addView(left, LinearLayout.LayoutParams(0, h, 1f).apply { marginEnd = m })
        row.addView(press, LinearLayout.LayoutParams(0, h, 1.6f).apply { marginStart = m; marginEnd = m })
        row.addView(right, LinearLayout.LayoutParams(0, h, 1f).apply { marginStart = m })
        root.addView(row, LinearLayout.LayoutParams(MATCH_PARENT, WRAP_CONTENT))
        setContentView(root)

        ble = Ble(this, this)
    }

    override fun onResume() {
        super.onResume()
        val need = arrayOf(Manifest.permission.BLUETOOTH_SCAN, Manifest.permission.BLUETOOTH_CONNECT)
        if (need.any { checkSelfPermission(it) != PackageManager.PERMISSION_GRANTED }) requestPermissions(need, 1)
        else startBle()
    }

    // the link stays up while the app is open: the system PIN dialog and a dark screen pause the activity,
    // and dropping the link there broke the pairing (bench 24.11). Closed only when the app is closed.
    override fun onDestroy() { super.onDestroy(); ble.stop() }

    override fun onRequestPermissionsResult(code: Int, perms: Array<out String>, res: IntArray) {
        if (res.isNotEmpty() && res.all { it == PackageManager.PERMISSION_GRANTED }) startBle()
        else onStatus("Bluetooth permission is needed to reach the DinMeter")
    }

    @Suppress("MissingPermission")
    private fun startBle() {
        val a = (getSystemService(BLUETOOTH_SERVICE) as android.bluetooth.BluetoothManager).adapter
        if (!a.isEnabled) { startActivity(Intent(BluetoothAdapter.ACTION_REQUEST_ENABLE)); return }
        ble.start()
    }

    // ---------- knob ----------
    private fun buzz(ms: Long) = vib.vibrate(VibrationEffect.createOneShot(ms, VibrationEffect.DEFAULT_AMPLITUDE))
    private fun turn(d: Int) { ble.send(if (d < 0) Ble.LEFT else Ble.RIGHT); buzz(10) }

    private var downAt = 0L
    private var holdSent = false
    private val holdCheck = object : Runnable {
        override fun run() {
            if (downAt == 0L) return
            val held = System.currentTimeMillis() - downAt
            if (held >= 2000 && !holdSent) { holdSent = true; ble.send(Ble.HOLD); buzz(60); press.text = "PRESS" ; return }
            if (held >= 300) press.text = "HOLD " + "|".repeat(((held - 300) / 170).toInt().coerceAtMost(10))
            main.postDelayed(this, 50)
        }
    }

    private fun pressTouch() = press.setOnTouchListener { v: View, e: MotionEvent ->
        when (e.actionMasked) {
            MotionEvent.ACTION_DOWN -> { downAt = System.currentTimeMillis(); holdSent = false; main.post(holdCheck); v.isPressed = true }
            MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> {
                val held = System.currentTimeMillis() - downAt
                downAt = 0; v.isPressed = false; press.text = "PRESS"
                if (e.actionMasked == MotionEvent.ACTION_UP && !holdSent) {
                    if (held < 500) { ble.send(Ble.PRESS); buzz(20) } else { ble.send(Ble.MID); buzz(30) }
                }
            }
        }
        true
    }

    // ---------- Ble.Listener ----------
    override fun onStatus(text: String) = runOnUiThread { status.text = text; screen.offline = !ble.connected }
    override fun onState(s: Ble.State) = runOnUiThread {
        screen.offline = false
        val out = when { s.k1 -> "  COMPRESSOR ON"; s.valve -> "  VALVE OPEN"; else -> "" }
        info.text = "line %.2f bar   target %.2f%s".format(s.bar, s.target, out)
    }
    override fun onFrame(f: Frame) = runOnUiThread { screen.offline = false; screen.frame = f }
    override fun onNeedPairing() = runOnUiThread {
        status.text = "Pair first: on the DinMeter open BLUETOOTH > PAIR A PHONE and type its PIN here"
    }
}
