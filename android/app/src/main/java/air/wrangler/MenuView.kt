package air.wrangler

import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.RectF
import android.graphics.Typeface
import android.view.HapticFeedbackConstants
import android.view.MotionEvent
import android.view.View
import kotlin.math.abs
import kotlin.math.max
import kotlin.math.min

/** One DinMeter menu level (firmware ui_screens.h menuBlob). */
class MenuList(val group: Int, val cursor: Int, val title: String, val rows: List<Row>) {
    /** kind: 0 action, 1 group, 2 back, 3 exit, 4 switch on, 5 switch off, 6 waiting for the second press */
    class Row(val kind: Int, val label: String, val value: String)

    companion object {
        fun decode(b: ByteArray): MenuList? {
            if (b.size < 6 || b[0] != 'M'.code.toByte() || b[1].toInt() != 1) return null
            var p = 2
            fun u8() = b[p++].toInt() and 0xFF
            fun str(): String { val l = u8(); val s = String(b, p, min(l, b.size - p), Charsets.ISO_8859_1); p += l; return s }
            val g = u8(); val cur = u8(); val n = u8(); val title = str()
            val rows = ArrayList<Row>(n)
            repeat(n) { if (p < b.size) rows.add(Row(u8(), str(), str())) }
            return MenuList(if (g == 255) -1 else g, cur, title, rows)
        }

        /** "CALIBRATE GAUGE" -> "Calibrate gauge", keeping AUX1, Wi-Fi and the like */
        fun nice(s: String): String {
            if (s.isEmpty() || s.any { it.isLowerCase() }) return s
            val words = s.lowercase().split(" ").map { w ->
                when (w) { "aux1" -> "AUX1"; "aux2" -> "AUX2"; "wifi" -> "Wi-Fi"; "bluetooth" -> "Bluetooth"; "demo" -> "Demo"; else -> w }
            }.joinToString(" ")
            return words.replaceFirstChar { it.uppercase() }
        }
    }
}

/**
 * The DinMeter menu as an iOS-style settings list. The DinMeter keeps the cursor and does the work:
 * a tap moves its cursor to the row with knob steps and presses, the back arrow sends the back key, Done leaves the menu.
 * The row under the DinMeter cursor is highlighted, so the knob and the list always agree.
 */
class MenuView(ctx: Context, private val send: (Int) -> Unit) : View(ctx) {
    var menu: MenuList? = null
        set(v) {
            val moved = v != null && (field?.cursor != v.cursor || field?.group != v.group)
            val levelChanged = field?.group != v?.group
            field = v
            if (levelChanged) scrollY0 = 0f
            if (moved) ensureVisible()
            invalidate()
        }

    private val dp = resources.displayMetrics.density
    private val p = Paint(Paint.ANTI_ALIAS_FLAG)
    private val bold = Typeface.create("sans-serif", Typeface.BOLD)
    private val regular = Typeface.create("sans-serif", Typeface.NORMAL)
    private val medium = Typeface.create("sans-serif-medium", Typeface.NORMAL)
    private val r = RectF()
    private val BLUE = 0xFF0A84FF.toInt(); private val CARD = 0xFF1C1C1E.toInt(); private val SEP = 0xFF38383A.toInt()
    private val GREY = 0xFF8E8E93.toInt(); private val SEL = 0xFF2C2C2E.toInt(); private val GREEN = 0xFF30D158.toInt()
    private val RED = 0xFFFF453A.toInt()

    private val navH get() = 44 * dp
    private val titleH get() = 52 * dp
    private val rowH get() = 52 * dp
    private val side get() = 16 * dp
    private var scrollY0 = 0f

    /** the rows shown: back and exit become the nav buttons */
    private fun shown(m: MenuList) = m.rows.withIndex().filter { it.value.kind != 2 && it.value.kind != 3 }

    private fun listTop() = navH + titleH + 8 * dp

    private fun ensureVisible() {
        val m = menu ?: return
        val idx = shown(m).indexOfFirst { it.index == m.cursor }
        if (idx < 0 || height == 0) return
        val top = listTop() + idx * rowH - scrollY0
        if (top < listTop()) scrollY0 -= listTop() - top
        if (top + rowH > height - 8 * dp) scrollY0 += top + rowH - (height - 8 * dp)
        clampScroll()
    }

    private fun clampScroll() {
        val m = menu ?: return
        val content = shown(m).size * rowH + 24 * dp
        val room = height - listTop()
        scrollY0 = scrollY0.coerceIn(0f, max(0f, content - room))
    }

    override fun onDraw(c: Canvas) {
        c.drawColor(Color.BLACK)
        val m = menu ?: return
        val w = width.toFloat()
        // nav bar: back on the left inside a group, Done on the right
        p.typeface = regular; p.textSize = 17 * dp; p.color = BLUE
        if (m.group >= 0) {
            p.typeface = regular; p.textSize = 30 * dp
            c.drawText("‹", side - 4 * dp, navH * 0.78f, p)
            p.textSize = 17 * dp
            c.drawText("Menu", side + 14 * dp, navH * 0.66f, p)
        }
        p.typeface = medium; p.textSize = 17 * dp
        c.drawText("Done", w - side - p.measureText("Done"), navH * 0.66f, p)
        // large title
        p.typeface = bold; p.textSize = 32 * dp; p.color = Color.WHITE
        c.drawText(if (m.group < 0) "Settings" else MenuList.nice(m.title), side, navH + titleH * 0.75f, p)

        // rows in one rounded card
        val rows = shown(m)
        val top0 = listTop() - scrollY0
        c.save(); c.clipRect(0f, listTop() - 2 * dp, w, height.toFloat())
        r.set(side, top0, w - side, top0 + rows.size * rowH)
        p.color = CARD; c.drawRoundRect(r, 12 * dp, 12 * dp, p)
        rows.forEachIndexed { k, (pos, row) ->
            val y = top0 + k * rowH
            if (pos == m.cursor) {   // the DinMeter cursor
                c.save(); c.clipRect(side, y, w - side, y + rowH)
                r.set(side, top0, w - side, top0 + rows.size * rowH)
                p.color = SEL; c.drawRoundRect(r, 12 * dp, 12 * dp, p)
                c.restore()
                p.color = BLUE; c.drawRect(side, y + 10 * dp, side + 3 * dp, y + rowH - 10 * dp, p)
            }
            if (k > 0) { p.color = SEP; c.drawRect(side + 16 * dp, y, w - side, y + 0.7f * dp, p) }
            val ty = y + rowH / 2 + 6 * dp
            val labelX = side + 16 * dp
            p.typeface = regular; p.textSize = 17 * dp
            p.color = if (row.kind == 6) RED else Color.WHITE
            val label = if (row.kind == 6) "Press again to confirm" else MenuList.nice(row.label)
            c.drawText(label, labelX, ty, p)
            val right = w - side - 16 * dp
            when (row.kind) {
                1 -> { p.color = 0xFF5A5A5E.toInt(); p.textSize = 22 * dp; c.drawText("›", right - p.measureText("›"), ty + 1 * dp, p) }
                4, 5 -> toggle(c, right, y + rowH / 2, row.kind == 4)
                else -> if (row.value.isNotEmpty()) {
                    p.color = GREY; p.textSize = 17 * dp
                    val v = MenuList.nice(row.value)
                    c.drawText(v, right - p.measureText(v), ty, p)
                }
            }
        }
        c.restore()
    }

    private fun toggle(c: Canvas, right: Float, cy: Float, on: Boolean) {
        val tw = 51 * dp; val th = 31 * dp
        r.set(right - tw, cy - th / 2, right, cy + th / 2)
        p.color = if (on) GREEN else 0xFF39393D.toInt(); c.drawRoundRect(r, th / 2, th / 2, p)
        p.color = Color.WHITE
        c.drawCircle(if (on) right - th / 2 else right - tw + th / 2, cy, th / 2 - 2 * dp, p)
    }

    // ---------- touch: tap a row, drag to scroll, the nav buttons ----------
    private var downY = 0f; private var downX = 0f; private var lastY = 0f; private var dragging = false
    override fun onTouchEvent(e: MotionEvent): Boolean {
        when (e.actionMasked) {
            MotionEvent.ACTION_DOWN -> { downX = e.x; downY = e.y; lastY = e.y; dragging = false; return true }
            MotionEvent.ACTION_MOVE -> {
                if (abs(e.y - downY) > 8 * dp) dragging = true
                if (dragging) { scrollY0 -= e.y - lastY; clampScroll(); invalidate() }
                lastY = e.y
            }
            MotionEvent.ACTION_UP -> if (!dragging) tap(e.x, e.y)
        }
        return true
    }

    private fun tap(x: Float, y: Float) {
        val m = menu ?: return
        if (y < navH) {
            if (x > width - 90 * dp) { send(Ble.HOLD); haptic() }                       // Done: leave the menu
            else if (x < 110 * dp && m.group >= 0) { send(Ble.BACK); haptic() }         // back: one level up
            return
        }
        val rows = shown(m)
        val k = ((y - listTop() + scrollY0) / rowH).toInt()
        if (y < listTop() || k !in rows.indices) return
        val target = rows[k].index
        val n = m.rows.size
        var d = ((target - m.cursor) % n + n) % n
        if (d > n / 2) d -= n                                                          // the short way round
        repeat(abs(d)) { send(if (d > 0) Ble.RIGHT else Ble.LEFT) }
        send(Ble.PRESS)
        haptic()
    }

    private fun haptic() = performHapticFeedback(HapticFeedbackConstants.CONFIRM)

    override fun onSizeChanged(w: Int, h: Int, ow: Int, oh: Int) { super.onSizeChanged(w, h, ow, oh); clampScroll() }
}
