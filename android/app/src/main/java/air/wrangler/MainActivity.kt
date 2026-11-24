package air.wrangler

import android.Manifest
import android.app.Activity
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothManager
import android.content.Intent
import android.content.pm.PackageManager
import android.content.res.Configuration
import android.graphics.Color
import android.graphics.drawable.GradientDrawable
import android.os.Bundle
import android.view.Gravity
import android.view.View
import android.view.ViewGroup.LayoutParams.MATCH_PARENT
import android.view.ViewGroup.LayoutParams.WRAP_CONTENT
import android.widget.FrameLayout
import android.widget.LinearLayout
import android.widget.TextView

/** The BLE link lives across rotations (the activity is rebuilt, the link is not). */
object Link {
    var ble: Ble? = null
    var state: Ble.State? = null
    var frame: Frame? = null
    var menu: MenuList? = null
    var status = "Starting ..."
    val icons = HashMap<Int, List<Frame.IconItem>>()
    var ui: Ble.Listener? = null
}

/**
 * Main screen, home or DinMeter screen, and the knob.
 *  - DinMeter on its main screen (idle, run, result, error): HomeView, drawn natively for the phone.
 *  - any other DinMeter screen (menus, editors, calibration, LEARN, PIN): the DinMeter screen itself (ScreenView).
 *  - the knob: KnobView (turn the ring, the centre is the button). Portrait: knob below; landscape: knob on the right.
 */
class MainActivity : Activity(), Ble.Listener {
    private lateinit var home: HomeView
    private lateinit var mirror: ScreenView
    private lateinit var menuView: MenuView
    private lateinit var knob: KnobView
    private lateinit var status: TextView
    private lateinit var dot: View
    private lateinit var pillView: LinearLayout

    override fun onCreate(b: Bundle?) {
        super.onCreate(b)
        window.statusBarColor = Color.BLACK; window.navigationBarColor = Color.BLACK
        val dp = resources.displayMetrics.density
        val land = resources.configuration.orientation == Configuration.ORIENTATION_LANDSCAPE

        home = HomeView(this) { send(if (it < 0) Ble.LEFT else Ble.RIGHT) }
        mirror = ScreenView(this) { send(if (it < 0) Ble.LEFT else Ble.RIGHT) }
        menuView = MenuView(this) { send(it) }
        knob = KnobView(this, { send(if (it < 0) Ble.LEFT else Ble.RIGHT) }, { send(it) })

        // status pill: dot + text
        dot = View(this).apply { background = GradientDrawable().apply { shape = GradientDrawable.OVAL; setColor(0xFF636366.toInt()) } }
        status = TextView(this).apply { setTextColor(0xFFEBEBF5.toInt()); textSize = 12f; text = Link.status }
        pillView = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL; gravity = Gravity.CENTER_VERTICAL
            setPadding((14 * dp).toInt(), (8 * dp).toInt(), (16 * dp).toInt(), (8 * dp).toInt())
            background = GradientDrawable().apply { cornerRadius = 20 * dp; setColor(0xFF1C1C1E.toInt()) }
            addView(dot, LinearLayout.LayoutParams((8 * dp).toInt(), (8 * dp).toInt()).apply { marginEnd = (8 * dp).toInt() })
            addView(status)
        }
        val stage = FrameLayout(this).apply {
            addView(home, FrameLayout.LayoutParams(MATCH_PARENT, MATCH_PARENT))
            addView(mirror, FrameLayout.LayoutParams(MATCH_PARENT, MATCH_PARENT).apply {
                val m = (12 * dp).toInt(); setMargins(m, m, m, m)
            })
            addView(menuView, FrameLayout.LayoutParams(MATCH_PARENT, MATCH_PARENT))
        }
        val left = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            addView(pillView, LinearLayout.LayoutParams(WRAP_CONTENT, WRAP_CONTENT).apply { gravity = Gravity.START; topMargin = (8 * dp).toInt(); marginStart = (28 * dp).toInt() })   // where the card's capsule sits
            addView(stage, LinearLayout.LayoutParams(MATCH_PARENT, 0, 1f))
        }
        val root = LinearLayout(this).apply {
            orientation = if (land) LinearLayout.HORIZONTAL else LinearLayout.VERTICAL
            setBackgroundColor(Color.BLACK)
            if (land) {
                addView(left, LinearLayout.LayoutParams(0, MATCH_PARENT, 1.4f))
                addView(knob, LinearLayout.LayoutParams(0, MATCH_PARENT, 1f).apply { val m = (16 * dp).toInt(); setMargins(m, m, m, m) })
            } else {
                addView(left, LinearLayout.LayoutParams(MATCH_PARENT, 0, 1.4f))
                addView(knob, LinearLayout.LayoutParams(MATCH_PARENT, 0, 1f).apply { val m = (6 * dp).toInt(); setMargins(m, 0, m, (10 * dp).toInt()) })   // a larger panel, centred in what is left
            }
        }
        setContentView(root)
        Link.ui = this
        if (Link.ble == null) Link.ble = Ble(applicationContext, object : Ble.Listener {
            override fun onStatus(text: String) { Link.status = text; Link.ui?.onStatus(text) }
            override fun onState(s: Ble.State) { Link.state = s; Link.ui?.onState(s) }
            override fun onFrame(f: Frame) { Link.frame = f; Link.ui?.onFrame(f) }
            override fun onNeedPairing() { Link.ui?.onNeedPairing() }
            override fun onIcon(key: Int, strokes: List<Frame.IconItem>) { Link.icons[key] = strokes; Link.ui?.onIcon(key, strokes) }
            override fun onMenu(m: MenuList) { Link.menu = m; Link.ui?.onMenu(m) }
        })
        Link.icons.forEach { (k, v) -> mirror.putIcon(k, v) }
        Link.state?.let { onState(it) }; Link.frame?.let { onFrame(it) }; Link.menu?.let { menuView.menu = it }
        refresh()
    }

    override fun onResume() {
        super.onResume()
        val need = arrayOf(Manifest.permission.BLUETOOTH_SCAN, Manifest.permission.BLUETOOTH_CONNECT)
        if (need.any { checkSelfPermission(it) != PackageManager.PERMISSION_GRANTED }) requestPermissions(need, 1)
        else startBle()
    }

    // the link stays up while the app is open (the PIN dialog and a dark screen pause the activity); closed with the app
    override fun onDestroy() {
        super.onDestroy()
        if (Link.ui === this) Link.ui = null
        if (isFinishing) { Link.ble?.stop(); Link.ble = null; Sounds.release() }
    }

    override fun onRequestPermissionsResult(code: Int, perms: Array<out String>, res: IntArray) {
        if (res.isNotEmpty() && res.all { it == PackageManager.PERMISSION_GRANTED }) startBle()
        else onStatus("Bluetooth permission is needed to reach the DinMeter")
    }

    @Suppress("MissingPermission")
    private fun startBle() {
        val a = (getSystemService(BLUETOOTH_SERVICE) as BluetoothManager).adapter
        if (!a.isEnabled) { startActivity(Intent(BluetoothAdapter.ACTION_REQUEST_ENABLE)); return }
        Link.ble?.start()
    }

    private fun send(code: Int) { Link.ble?.send(code) }

    // which view, knob label and colour
    private fun refresh() {
        val s = Link.state
        val online = Link.ble?.connected == true
        val onMain = s == null || s.screen == 0 || s.screen == 7   // 7: the CUSTOM editor, drawn natively too
        val onMenu = s != null && s.screen == 1 && Link.menu != null
        home.visibility = if (onMain) View.VISIBLE else View.GONE
        menuView.visibility = if (onMenu) View.VISIBLE else View.GONE
        mirror.visibility = if (onMain || onMenu) View.GONE else View.VISIBLE
        home.online = online; mirror.offline = !online
        knob.enabledLink = online
        val running = s != null && s.state in 1..6
        knob.label = when {
            s == null -> "PRESS"
            s.screen == 7 -> if (s.customStep == 0) "NEXT" else "OK"
            onMenu -> "SELECT"
            !onMain -> hintVerb() ?: "SELECT"   // service screens: what the DinMeter's own hint says a press does
            running -> "STOP"
            s.state == 7 || s.state == 8 -> "OK"
            s.armStage > 0 -> "START"
            else -> "START"
        }
        knob.accent = when {            // pointer and lit code: white idle, amber armed / running, red on an error
            s == null -> 0xFF636366.toInt()
            s.state == 8 || s.armStage == 2 -> HomeView.RED
            running || s.armStage == 1 -> 0xFFFFB020.toInt()
            else -> 0xFFF5F5F7.toInt()
        }
        (dot.background as GradientDrawable).setColor(if (online) HomeView.GREEN else 0xFF636366.toInt())
        // the link capsule moves into the scene card on the main screen (advisor); the top pill stays for search / errors
        pillView.visibility = if (online && onMain) View.GONE else View.VISIBLE
        knob.preset = if (onMain && s != null) s.preset else -1
        knob.busy = s != null && (s.state in 1..6 || s.armStage > 0)
        // the preset icon of the main screen: the ICON item of the newest frame
        val key = Link.frame?.items?.firstOrNull { it.kind == Frame.ICON }?.raw
        if (key != null) {
            val ic = Link.icons[key]
            if (ic != null) home.iconStrokes = ic
            else if (online && System.currentTimeMillis() - iconAsked > 2500) { iconAsked = System.currentTimeMillis(); send(Ble.ICONS) }   // lost on the way: ask again
        }
    }
    private var iconAsked = 0L
    /** "PRESS: BACK  HOLD: ..." in the mirrored screen's hint -> BACK */
    private fun hintVerb(): String? {
        val t = Link.frame?.items?.firstOrNull { it.kind == Frame.TEXT && it.text.uppercase().startsWith("PRESS:") } ?: return null
        val v = t.text.uppercase().substringAfter(":").trim().substringBefore("  ").trim()
        return v.take(9).ifEmpty { null }
    }

    // the phone back key: on a DinMeter menu or service screen one level up there; on the main screen to the background
    // (the link stays, the app is not closed)
    @Deprecated("system back")
    override fun onBackPressed() {
        val s = Link.state
        if (s != null && s.screen != 0 && Link.ble?.connected == true) { send(Ble.BACK); knob.performHapticFeedback(android.view.HapticFeedbackConstants.CONFIRM) }
        else moveTaskToBack(true)
    }

    // ---------- Ble.Listener (UI thread) ----------
    override fun onStatus(text: String) = runOnUiThread { status.text = text; if (Link.ble?.connected != true) sndSeq = -1; refresh() }
    private var sndSeq = -1
    override fun onState(s: Ble.State) = runOnUiThread {
        // the DinMeter's sounds on the phone too: play what it just started (not the one that was current when we connected)
        if (s.sndSeq >= 0) { if (sndSeq >= 0 && s.sndSeq != sndSeq) Sounds.play(s.sndId); sndSeq = s.sndSeq }
        home.state = s; refresh()
    }
    override fun onFrame(f: Frame) = runOnUiThread { mirror.frame = f; refresh() }
    override fun onIcon(key: Int, strokes: List<Frame.IconItem>) = runOnUiThread { mirror.putIcon(key, strokes); refresh() }
    override fun onMenu(m: MenuList) = runOnUiThread { menuView.menu = m; refresh() }
    override fun onNeedPairing() = runOnUiThread {
        status.text = "Pair first: DinMeter menu CONNECT > BLUETOOTH > PAIR A PHONE"
    }
}
