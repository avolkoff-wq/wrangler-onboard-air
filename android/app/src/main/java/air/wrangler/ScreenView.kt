package air.wrangler

import android.content.Context
import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.Path
import android.graphics.RectF
import android.graphics.Typeface
import android.view.MotionEvent
import android.view.View
import kotlin.math.abs
import kotlin.math.min

/**
 * The DinMeter's service screens (editors, calibration, LEARN, sensor, hours, Bluetooth, Wi-Fi ...) in the app's own layout
 * (owner, 24.11: "align the fonts of every sub-item"): the display list of the DinMeter is read as content, not as a picture.
 *  - a screen with a selection bar is a list: the same rows, switches and highlight as the menu (MenuView), a tap selects;
 *  - any other screen is a sheet: large title, lines in three fixed sizes (value / text / note), the hint at the bottom.
 * One type, one set of sizes and one spacing for every screen, whatever fonts the 240 x 135 display had to use.
 * A screen with graphics this view does not know is still drawn as the scaled picture of the DinMeter screen (mirror()).
 * A horizontal swipe turns the knob, one step per 40 dp.
 */
class ScreenView(ctx: Context, private val onTurn: (Int) -> Unit, private val send: (Int) -> Unit) : View(ctx) {
    var frame: Frame? = null
        set(v) { field = v; sheet = v?.let { parse(it) }; invalidate() }
    var screenId = -1
        set(v) { if (field != v) { field = v; sheet = frame?.let { parse(it) }; invalidate() } }
    var offline = true
        set(v) { if (field != v) { field = v; invalidate() } }
    private val icons = HashMap<Int, List<Frame.IconItem>>()
    fun putIcon(key: Int, strokes: List<Frame.IconItem>) { icons[key] = strokes; invalidate() }

    private val p = Paint(Paint.ANTI_ALIAS_FLAG)
    private val regular = Typeface.create("sans-serif", Typeface.NORMAL)
    private val bold = Typeface.create("sans-serif", Typeface.BOLD)
    private val mono = Typeface.create("monospace", Typeface.BOLD)
    private val condensed = Typeface.create("sans-serif-condensed", Typeface.NORMAL)
    private val condensedBold = Typeface.create(Typeface.create("sans-serif-condensed", Typeface.BOLD), 800, false)
    private val r = RectF()
    private val path = Path()
    private val suv: Bitmap by lazy {
        val bm = Bitmap.createBitmap(SuvBitmap.W, SuvBitmap.H, Bitmap.Config.ALPHA_8)
        val px = ByteArray(SuvBitmap.W * SuvBitmap.H)
        for (y in 0 until SuvBitmap.H) for (x in 0 until SuvBitmap.W) {
            val bit = (SuvBitmap.BITS[(y * SuvBitmap.W + x) / 8].toInt() shr (7 - x % 8)) and 1
            px[y * SuvBitmap.W + x] = if (bit == 1) 0xFF.toByte() else 0
        }
        bm.copyPixelsFromBuffer(java.nio.ByteBuffer.wrap(px)); bm
    }

    override fun onDraw(c: Canvas) {
        c.drawColor(Color.BLACK)
        val f = frame
        val sh = sheet
        if (f != null && sh != null) {
            drawSheet(c, sh)
            if (offline) { p.color = 0xAA000000.toInt(); p.style = Paint.Style.FILL; c.drawRect(0f, 0f, width.toFloat(), height.toFloat(), p) }
            return
        }
        mirror(c, f)
    }

    // ================= the service screens in the app's layout =================
    private class Row(val label: String, val value: String, val kind: Int, val color: Int, val valueColor: Int, val selected: Boolean)   // kind as in MenuList.Row
    private class Line(val text: String, val cls: Int, val color: Int, val cursor: Int = -1)   // cls: 2 value, 1 text, 0 note
    private class Sheet(val title: String, val rows: List<Row>?, val lines: List<Line>, val hint: String, val hintColor: Int, val top: String, val tag: String)
    private var sheet: Sheet? = null

    private val dp = resources.displayMetrics.density
    private val jeep = Typeface.create(Typeface.create("sans-serif-condensed", Typeface.BOLD), 800, false)
    private val AMBER = 0xFFFFB020.toInt(); private val CARD = 0xFF1C1C1E.toInt(); private val SEP = 0xFF38383A.toInt()
    private val GREY = 0xFF8E8E93.toInt(); private val SEL = 0xFF2C2C2E.toInt(); private val GREEN = 0xFF30D158.toInt()
    private val RED = 0xFFFF453A.toInt(); private val WHITE = 0xFFF5F5F7.toInt(); private val BLUE = 0xFF64D2FF.toInt()
    private val navH get() = 44 * dp
    private val titleH get() = 52 * dp
    private val rowH get() = 52 * dp
    private val side get() = 16 * dp
    private fun listTop() = navH + titleH + 8 * dp

    private val TITLES = mapOf(2 to "SETTING", 3 to "CALIBRATE GAUGE", 4 to "WI-FI UPDATE", 5 to "VERSION", 6 to "SENSOR LIVE",
        8 to "LEARN TEST", 9 to "LEARN TEST", 10 to "LEARN TEST", 11 to "COMPRESSOR HOURS", 12 to "COMPRESSOR HOURS",
        13 to "OUTPUT TEST", 14 to "WI-FI PASSWORD", 15 to "BLUETOOTH")

    /** the device palette as the app's: white, grey notes, amber warnings, green / red results */
    private fun tone(col: Int): Int {
        val r0 = Color.red(col); val g = Color.green(col); val b = Color.blue(col)
        return when {
            r0 > 200 && g < 90 && b < 90 -> RED
            r0 < 90 && g > 180 && b < 90 -> GREEN
            r0 > 200 && g in 120..235 && b < 90 -> AMBER                       // orange, yellow
            r0 in 100..170 && g in 180..230 && b > 215 -> BLUE                 // sky blue: the valve
            abs(r0 - g) < 24 && abs(g - b) < 24 && r0 < 225 -> GREY            // greys
            else -> WHITE                                                      // white, cyan titles, lime values
        }
    }
    private fun isCyan(col: Int) = Color.red(col) < 60 && Color.green(col) > 200 && Color.blue(col) > 200
    private fun isSelBar(col: Int) = Color.red(col) < 40 && Color.green(col) in 80..140 && Color.blue(col) < 40
    private fun isMagenta(col: Int) = Color.red(col) > 200 && Color.green(col) < 60 && Color.blue(col) > 200

    /** the display list as content; null = a screen this layout does not know (drawn as the mirror) */
    private fun parse(f: Frame): Sheet? {
        if (screenId !in TITLES || f.w == 0 || f.h == 0) return null
        var tag = ""
        var sel: Frame.Item? = null
        var cursorRect: Frame.Item? = null
        val texts = ArrayList<Frame.Item>()
        for (it in f.items) when (it.kind) {
            Frame.TEXT -> {
                if (it.text.isBlank()) continue
                if (it.aux <= 1 && it.y <= 2 && it.text in setOf("BT", "DEMO", "FAKE")) { if (it.text != "BT") tag = it.text; continue }
                texts.add(it)
            }
            Frame.FILL_RRECT -> if (isSelBar(it.color)) sel = it else if (it.y > 0) return null
            Frame.FILL_RECT -> when {
                it.color == Color.BLACK || isMagenta(it.color) -> {}
                it.h <= 3 && it.w <= 24 -> cursorRect = it                      // the character cursor of WIFI PASSWORD
                else -> return null
            }
            else -> return null
        }
        fun centred(it: Frame.Item) = it.datum and 3 == 1
        // the hint: the bold caps line at the very bottom, or the small wrapped fonts
        val hints = texts.filter { centred(it) && it.datum and 12 == 0 && it.text.contains(':') &&
                ((it.y >= f.h - 19 && it.text == it.text.uppercase()) || (it.aux <= 1 && it.y >= f.h - 50)) }
        val hint = hints.joinToString("  ") { it.text.uppercase().trim() }
        val hintColor = hints.firstOrNull()?.let { tone(it.color) } ?: GREY
        val rest = texts.filter { it !in hints }.sortedBy { it.y }
        val top = rest.filter { it.datum == 0 && it.x <= 4 && it.y < 40 && it.aux == 2 }
        val body = rest.filter { it !in top }
        val left = body.filter { it.datum == 4 }                                // middle_left: list rows
        if (left.isNotEmpty()) {
            val right = body.filter { it.datum == 6 }
            val rows = left.map { t ->
                var s = t.text.trim(); var kind = 0
                if (s.startsWith("[x] ")) { kind = 4; s = s.substring(4) } else if (s.startsWith("[ ] ")) { kind = 5; s = s.substring(4) }
                if (s.startsWith("< ")) s = s.substring(2)
                if (s == "BACK") kind = 2
                if (s == "PRESS AGAIN") kind = 6
                val cut = s.indexOf("  ")
                var value = if (cut > 0) s.substring(cut).trim() else ""
                if (cut > 0) s = s.substring(0, cut)
                var vc = GREY
                right.firstOrNull { abs(it.y - t.y) <= 3 }?.let { value = it.text.trim(); vc = tone(it.color) }
                Row(s, value, kind, tone(t.color), vc, sel != null && abs(sel.y + sel.h / 2 - t.y) <= 4)
            }
            val notes = body.filter { it !in left && it !in right }.map { Line(it.text.trim(), if (tone(it.color) == GREY) 0 else 1, tone(it.color)) }
            return Sheet(TITLES[screenId] ?: "", rows, notes, hint, hintColor, "", tag)
        }
        var title = TITLES[screenId] ?: ""
        val lines = ArrayList<Line>()
        for ((k, it) in body.withIndex()) {
            if (k == 0 && isCyan(it.color) && it.aux == 3) { title = it.text.trim(); continue }
            val cls = if (it.aux >= 4) 2 else if (it.aux == 3) 1 else 0
            var cur = -1
            val cr = cursorRect
            if (cr != null && cls == 2 && cr.w > 0 && cr.y > it.y) cur = Math.round((cr.x - (it.x - it.w / 2f)) / cr.w)
            lines.add(Line(it.text.trim(), cls, if (cls == 0 && tone(it.color) == WHITE) GREY else tone(it.color), cur))
        }
        return Sheet(title, null, lines, hint, hintColor, top.joinToString(" ") { it.text.trim() }, tag)
    }

    /** caps in the Jeep type at one size; a line too wide is squeezed a little, then set smaller */
    private fun fit(t: String, size: Float, maxW: Float): Float {
        p.typeface = jeep; p.textSize = size; p.textScaleX = 1f
        val w = p.measureText(t)
        if (w > maxW && w > 0) { val k = maxW / w; if (k >= 0.85f) p.textScaleX = k else { p.textScaleX = 0.85f; p.textSize = size * k / 0.85f } }
        return p.measureText(t)
    }

    private fun drawSheet(c: Canvas, sh: Sheet) {
        val w = width.toFloat(); val h = height.toFloat()
        p.style = Paint.Style.FILL; p.textAlign = Paint.Align.LEFT; p.typeface = jeep
        // nav bar: one level back (the DinMeter's own way back from this screen)
        p.color = AMBER; p.letterSpacing = 0f; p.textSize = 30 * dp; p.textScaleX = 1f
        c.drawText("\u2039", side - 4 * dp, navH * 0.78f, p)
        p.textSize = 17 * dp; p.letterSpacing = 0.08f
        c.drawText("BACK", side + 14 * dp, navH * 0.66f, p)
        if (sh.tag.isNotEmpty()) {                                              // DEMO / FAKE: never mistaken for real tyres
            p.textSize = 12 * dp; p.letterSpacing = 0.12f
            val tw = p.measureText(sh.tag)
            r.set(w - side - tw - 20 * dp, navH * 0.5f - 12 * dp, w - side, navH * 0.5f + 12 * dp)
            p.color = 0x33FF2D92; c.drawRoundRect(r, 12 * dp, 12 * dp, p)
            p.color = 0xFFFF6EB4.toInt(); c.drawText(sh.tag, r.left + 10 * dp, r.centerY() + 4.3f * dp, p)
        }
        // large title, the same as the menu's
        p.color = Color.WHITE; p.letterSpacing = 0.08f
        fit(sh.title.uppercase(), 28 * dp, w - 2 * side)
        c.drawText(sh.title.uppercase(), side, navH + titleH * 0.75f, p)
        p.textScaleX = 1f; p.letterSpacing = 0.06f
        if (sh.rows != null) drawList(c, sh, w) else drawLinesCard(c, sh, w, h)
        p.letterSpacing = 0f; p.textScaleX = 1f
    }

    private fun shown(sh: Sheet) = sh.rows!!.withIndex().filter { it.value.kind != 2 }

    private fun drawList(c: Canvas, sh: Sheet, w: Float) {
        val rows = shown(sh)
        val top0 = listTop()
        r.set(side, top0, w - side, top0 + rows.size * rowH)
        p.color = CARD; c.drawRoundRect(r, 16 * dp, 16 * dp, p)
        rows.forEachIndexed { k, (_, row) ->
            val y = top0 + k * rowH
            if (row.selected) {
                c.save(); c.clipRect(side, y, w - side, y + rowH)
                r.set(side, top0, w - side, top0 + rows.size * rowH)
                p.color = SEL; c.drawRoundRect(r, 16 * dp, 16 * dp, p)
                c.restore()
                p.color = AMBER; c.drawRect(side, y + 10 * dp, side + 3 * dp, y + rowH - 10 * dp, p)
            }
            if (k > 0) { p.color = SEP; c.drawRect(side + 16 * dp, y, w - side, y + 0.7f * dp, p) }
            val ty = y + rowH / 2 + 6 * dp
            val right = w - side - 16 * dp
            val roomRight = if (row.kind == 4 || row.kind == 5) right - 51 * dp - 12 * dp else right
            p.letterSpacing = 0.06f
            p.color = if (row.kind == 6) RED else row.color
            val label = if (row.kind == 6) "PRESS AGAIN TO CONFIRM" else row.label.uppercase()
            val lw = fit(label, 17 * dp, roomRight - side - 16 * dp)
            c.drawText(label, side + 16 * dp, ty, p)
            p.textScaleX = 1f
            if (row.value.isNotEmpty()) {
                val v = row.value.uppercase()
                p.color = row.valueColor
                if (row.kind == 4 || row.kind == 5) {                            // a switch with a note: the note follows the label, smaller
                    fit(v, 13 * dp, roomRight - (side + 16 * dp + lw + 12 * dp))
                    c.drawText(v, side + 16 * dp + lw + 12 * dp, ty - 0.5f * dp, p)
                } else {
                    val vw = fit(v, 17 * dp, right - (side + 16 * dp + lw + 12 * dp))
                    c.drawText(v, right - vw, ty, p)
                }
                p.textScaleX = 1f
            }
            if (row.kind == 4 || row.kind == 5) toggle(c, right, y + rowH / 2, row.kind == 4)
        }
        // what the DinMeter shows under its list: live readings, as the footer of the card
        var y = top0 + rows.size * rowH + 30 * dp
        for (l in sh.lines) {
            p.color = l.color
            fit(l.text.uppercase(), 15 * dp, w - 2 * side - 32 * dp)
            c.drawText(l.text.uppercase(), side + 16 * dp, y, p)
            p.textScaleX = 1f
            y += 24 * dp
        }
    }

    private fun toggle(c: Canvas, right: Float, cy: Float, on: Boolean) {
        val tw = 51 * dp; val th = 31 * dp
        r.set(right - tw, cy - th / 2, right, cy + th / 2)
        p.color = if (on) GREEN else 0xFF39393D.toInt(); c.drawRoundRect(r, th / 2, th / 2, p)
        p.color = Color.WHITE
        c.drawCircle(if (on) right - th / 2 else right - tw + th / 2, cy, th / 2 - 2 * dp, p)
    }

    private val SIZE = floatArrayOf(15f, 19f, 56f)       // note, text, value (dp): the only sizes a sheet uses
    private fun drawLinesCard(c: Canvas, sh: Sheet, w: Float, h: Float) {
        val top0 = listTop(); val bottom = h - 8 * dp
        r.set(side, top0, w - side, bottom)
        p.color = CARD; c.drawRoundRect(r, 16 * dp, 16 * dp, p)
        val inner = w - 2 * side - 32 * dp
        var areaTop = top0 + 20 * dp; var areaBottom = bottom - 20 * dp
        if (sh.top.isNotEmpty()) {                                              // the running time of a calibration
            p.color = GREY; p.letterSpacing = 0.1f
            fit(sh.top.uppercase(), 13 * dp, inner)
            c.drawText(sh.top.uppercase(), side + 16 * dp, top0 + 26 * dp, p)
            areaTop = top0 + 38 * dp
        }
        if (sh.hint.isNotEmpty()) {
            val t = sh.hint.replace("  ", "   \u00B7   ")
            p.color = if (sh.hintColor == RED) RED else GREY; p.letterSpacing = 0.1f
            val tw = fit(t, 13 * dp, inner)
            c.drawText(t, w / 2 - tw / 2, bottom - 18 * dp, p)
            p.color = SEP; c.drawRect(side + 16 * dp, bottom - 44 * dp, w - side - 16 * dp, bottom - 44 * dp + 0.7f * dp, p)
            areaBottom = bottom - 58 * dp
        }
        p.textScaleX = 1f; p.letterSpacing = 0.06f
        val ls = sh.lines
        if (ls.isEmpty()) return
        // the block of lines: cap heights and fixed gaps, centred in the card; made smaller as a whole if it is too tall
        fun cap(l: Line) = SIZE[l.cls] * dp * 0.72f
        fun gap(a: Line, b: Line) = (if (a.cls == 2 || b.cls == 2) 28f else if (a.cls == 0 && b.cls == 0) 13f else 17f) * dp
        var total = 0f
        for (k in ls.indices) { total += cap(ls[k]); if (k > 0) total += gap(ls[k - 1], ls[k]) }
        val z = min(1f, (areaBottom - areaTop) / total)
        var y = (areaTop + areaBottom) / 2 - total * z / 2
        for (k in ls.indices) {
            val l = ls[k]
            if (k > 0) y += gap(ls[k - 1], l) * z
            y += cap(l) * z
            val t = l.text.uppercase()
            p.color = l.color
            p.letterSpacing = if (l.cls == 2) 0.02f else 0.06f
            if (l.cursor >= 0 && t.isNotEmpty()) {                               // an editor of characters: fixed cells, the cursor under one
                p.textSize = SIZE[2] * dp * z; p.textScaleX = 1f; p.letterSpacing = 0f
                val cell = min(p.textSize * 0.72f, inner / t.length)
                if (cell < p.textSize * 0.72f) p.textSize = cell / 0.72f
                val x0 = w / 2 - cell * t.length / 2
                for ((i, ch) in t.withIndex()) {
                    val cw = p.measureText(ch.toString())
                    p.color = l.color; c.drawText(ch.toString(), x0 + i * cell + (cell - cw) / 2, y, p)
                    if (i == l.cursor) { p.color = AMBER; c.drawRect(x0 + i * cell + 3 * dp, y + 8 * dp, x0 + (i + 1) * cell - 3 * dp, y + 11 * dp, p) }
                }
                y += 6 * dp
            } else {
                val tw = fit(t, SIZE[l.cls] * dp * z, inner)
                c.drawText(t, w / 2 - tw / 2, y, p)
            }
            p.textScaleX = 1f
        }
    }

    // ---------- touch: the back button, a tap on a row, a horizontal swipe = the knob ----------
    private var downX = 0f; private var downY = 0f; private var turned = 0; private var moved = false
    override fun onTouchEvent(e: MotionEvent): Boolean {
        when (e.actionMasked) {
            MotionEvent.ACTION_DOWN -> { downX = e.x; downY = e.y; turned = 0; moved = false }
            MotionEvent.ACTION_MOVE -> {
                if (abs(e.x - downX) > 12 * dp || abs(e.y - downY) > 12 * dp) moved = true
                if (sheet?.rows == null && abs(e.x - downX) > abs(e.y - downY)) {
                    val steps = ((e.x - downX) / (40 * dp)).toInt()
                    if (steps != turned) { onTurn(if (steps > turned) 1 else -1); turned += if (steps > turned) 1 else -1 }
                }
            }
            MotionEvent.ACTION_UP -> if (!moved) tap(e.x, e.y)
        }
        return true
    }
    private fun tap(x: Float, y: Float) {
        val sh = sheet ?: return
        if (y < navH + 6 * dp) { if (x < 130 * dp) { send(Ble.BACK); haptic() }; return }
        val all = sh.rows ?: return
        val rows = shown(sh)
        val k = ((y - listTop()) / rowH).toInt()
        if (y < listTop() || k !in rows.indices) return
        val cur = all.indexOfFirst { it.selected }
        if (cur < 0) return
        val d = rows[k].index - cur
        repeat(abs(d)) { send(if (d > 0) Ble.RIGHT else Ble.LEFT) }
        send(Ble.PRESS); haptic()
    }
    private fun haptic() = performHapticFeedback(android.view.HapticFeedbackConstants.CONFIRM)

    // ================= the scaled picture of the DinMeter screen (screens the layout above does not know) =================
    private fun mirror(c: Canvas, f: Frame?) {
        if (f == null || f.w == 0 || f.h == 0) {
            p.color = Color.GRAY; p.textSize = 16 * resources.displayMetrics.density; p.typeface = regular
            p.textAlign = Paint.Align.CENTER
            c.drawText("Waiting for the DinMeter screen", width / 2f, height / 2f, p)
            p.textAlign = Paint.Align.LEFT
            return
        }
        val s = min(width.toFloat() / f.w, height.toFloat() / f.h)
        val ox = (width - f.w * s) / 2; val oy = (height - f.h * s) / 2
        // rounded "glass" behind the screen, like the device
        r.set(ox - 8 * dp, oy - 8 * dp, ox + f.w * s + 8 * dp, oy + f.h * s + 8 * dp)
        p.color = 0xFF101012.toInt(); c.drawRoundRect(r, 24 * dp, 24 * dp, p)                       // the same card as the scene
        p.style = Paint.Style.STROKE; p.strokeWidth = dp; p.color = 0x14FFFFFF; c.drawRoundRect(r, 24 * dp, 24 * dp, p); p.style = Paint.Style.FILL
        c.save(); c.clipRect(ox, oy, ox + f.w * s, oy + f.h * s); c.translate(ox, oy); c.scale(s, s)
        p.color = if (f.bg == Color.BLACK) 0xFF101012.toInt() else f.bg; p.style = Paint.Style.FILL
        c.drawRect(0f, 0f, f.w.toFloat(), f.h.toFloat(), p)
        for (it in f.items) draw(c, it)
        c.restore()
        if (offline) { p.color = 0xAA000000.toInt(); c.drawRect(ox, oy, ox + f.w * s, oy + f.h * s, p) }
    }

    private fun draw(c: Canvas, it: Frame.Item) {
        p.color = calm(it.color); p.style = Paint.Style.FILL; p.strokeCap = Paint.Cap.BUTT
        val x = it.x.toFloat(); val y = it.y.toFloat(); val w = it.w.toFloat(); val h = it.h.toFloat()
        when (it.kind) {
            Frame.FILL_RECT -> c.drawRect(x, y, x + w, y + h, p)
            Frame.FILL_RRECT -> { r.set(x, y, x + w, y + h); c.drawRoundRect(r, it.aux.toFloat(), it.aux.toFloat(), p) }
            Frame.DRAW_RECT -> { p.style = Paint.Style.STROKE; p.strokeWidth = 1f; c.drawRect(x + .5f, y + .5f, x + w - .5f, y + h - .5f, p) }
            Frame.FILL_CIRCLE -> c.drawCircle(x, y, w, p)
            Frame.ARROW -> {
                path.reset()
                if (it.aux == 0) { path.moveTo(x, y + h); path.lineTo(x + w, y + h); path.lineTo(x + w / 2, y) }
                else { path.moveTo(x, y); path.lineTo(x + w, y); path.lineTo(x + w / 2, y + h) }
                path.close(); c.drawPath(path, p)
            }
            Frame.ICON -> icons[it.raw]?.let { IconPainter.draw(c, x, y, it) }
            Frame.TEXT -> if (it.aux == 6) seg7(c, it) else text(c, it)
        }
    }

    // ---------- text ----------
    private fun box(it: Frame.Item, ascentOffset: Float = 0f): RectF {
        val w = it.w.toFloat(); val h = it.h.toFloat()
        val left = when (it.datum and 3) { 1 -> it.x - w / 2; 2 -> it.x - w; else -> it.x.toFloat() }
        val top = when {
            it.datum and 16 != 0 -> it.y - h * 0.8f - ascentOffset   // baseline datum
            it.datum and 12 == 4 -> it.y - h / 2
            it.datum and 12 == 8 -> it.y - h
            else -> it.y.toFloat()
        }
        return RectF(left, top, left + w, top + h)
    }

    /** the device palette mapped to the app's: white titles and values, grey notes, amber for the selection and warnings */
    private fun calm(col: Int): Int {
        val r0 = Color.red(col); val g = Color.green(col); val b = Color.blue(col)
        return when {
            r0 < 60 && g > 200 && b > 200 -> 0xFFF5F5F7.toInt()             // cyan titles
            r0 in 120..200 && g > 220 && b < 120 -> 0xFFF5F5F7.toInt()      // lime values
            r0 > 200 && g in 120..230 && b < 90 -> 0xFFFFB020.toInt()       // orange / yellow
            r0 < 40 && g in 80..140 && b < 40 -> 0xFF4A3A10.toInt()         // dark green selection bar -> amber tint (text over it stays white)
            else -> col
        }
    }
    private fun text(c: Canvas, it: Frame.Item) {
        if (it.text.isEmpty() || it.h == 0) return
        // one style with the native screens (owner): every DinMeter text in the condensed Jeep type
        p.typeface = when (it.aux) { 2 -> condensed; else -> condensedBold }
        p.textScaleX = 1f; p.textSize = 100f
        val fm = p.fontMetrics
        p.textSize = it.h * 100f / (fm.descent - fm.ascent)
        val m = p.fontMetrics
        // caps in the condensed type, never stretched wider than drawn (the device font is wider than ours)
        val t = it.text.uppercase()
        p.letterSpacing = 0.04f
        val tw = p.measureText(t)
        if (tw > 0 && it.w > 0) p.textScaleX = (it.w / tw).coerceIn(0.7f, 1.0f)
        val b = box(it)
        val mw = p.measureText(t)
        val left = when (it.datum and 3) { 1 -> b.centerX() - mw / 2; 2 -> b.right - mw; else -> b.left }   // keep the anchor
        c.drawText(t, left, b.top - m.ascent, p)
        p.letterSpacing = 0f
        p.textScaleX = 1f
    }

    // ---------- 7-segment digits (Font7 on the DinMeter) ----------
    private val segs = mapOf(   // a b c d e f g
        '0' to "abcdef", '1' to "bc", '2' to "abdeg", '3' to "abcdg", '4' to "bcfg", '5' to "acdfg",
        '6' to "acdefg", '7' to "abc", '8' to "abcdefg", '9' to "abcdfg", '-' to "g")

    private fun seg7(c: Canvas, it: Frame.Item) {
        val b = box(it)
        val units = it.text.sumOf { ch -> if (ch == '.' || ch == ':') 0.45 else 1.0 }.toFloat()
        if (units <= 0f) return
        val cw = b.width() / units
        val dh = b.height() * 0.92f; val top = b.top + b.height() * 0.04f
        val t = min(cw, dh / 2) * 0.17f                     // stroke
        var x = b.left
        p.style = Paint.Style.FILL
        for (ch in it.text) {
            if (ch == '.' || ch == ':') {
                val w = cw * 0.45f
                if (ch == '.') c.drawRect(x + w / 2 - t / 2 - t * .1f, top + dh - t, x + w / 2 + t / 2, top + dh, p)
                else { c.drawRect(x + w / 2 - t / 2, top + dh * 0.3f, x + w / 2 + t / 2, top + dh * 0.3f + t, p)
                       c.drawRect(x + w / 2 - t / 2, top + dh * 0.7f - t, x + w / 2 + t / 2, top + dh * 0.7f, p) }
                x += w; continue
            }
            val on = segs[ch] ?: ""
            val l = x + cw * 0.1f; val rr = x + cw * 0.9f; val mid = top + dh / 2
            val g = t * 0.25f
            fun h(y: Float) { path.reset(); path.moveTo(l + g, y); path.lineTo(l + t / 2 + g, y - t / 2); path.lineTo(rr - t / 2 - g, y - t / 2)
                              path.lineTo(rr - g, y); path.lineTo(rr - t / 2 - g, y + t / 2); path.lineTo(l + t / 2 + g, y + t / 2); path.close(); c.drawPath(path, p) }
            fun v(xx: Float, y0: Float, y1: Float) { path.reset(); path.moveTo(xx, y0 + g); path.lineTo(xx + t / 2, y0 + t / 2 + g); path.lineTo(xx + t / 2, y1 - t / 2 - g)
                              path.lineTo(xx, y1 - g); path.lineTo(xx - t / 2, y1 - t / 2 - g); path.lineTo(xx - t / 2, y0 + t / 2 + g); path.close(); c.drawPath(path, p) }
            if ('a' in on) h(top + t / 2)
            if ('g' in on) h(mid)
            if ('d' in on) h(top + dh - t / 2)
            if ('f' in on) v(l + t / 2, top + t / 2, mid)
            if ('b' in on) v(rr - t / 2, top + t / 2, mid)
            if ('e' in on) v(l + t / 2, mid, top + dh - t / 2)
            if ('c' in on) v(rr - t / 2, mid, top + dh - t / 2)
            x += cw
        }
    }

    // ---------- preset icon: the DinMeter's strokes, 1/8 px relative to the box ----------
    private fun icon(c: Canvas, bx: Float, by: Float, it: Frame.Item, bw: Float, bh: Float) {
        val list = icons[it.raw] ?: return
        IconPainter.draw(c, bx, by, list)
    }
}

/** The DinMeter's preset icon from its strokes (1/8 px relative to the icon box) and the SUV bitmap. */
object IconPainter {
    private val p = Paint(Paint.ANTI_ALIAS_FLAG)
    private val path = Path()
    private val r = RectF()
    // the 128 x 51 mask, smoothed: upscaled 8x with a blur-free bilinear pass, then the edge sharpened again by a steep
    // alpha curve, so the outline is a clean curve instead of 7-px stairs when drawn big
    private const val UP = 8
    private val suv: Bitmap by lazy {
        val w = SuvBitmap.W; val h = SuvBitmap.H
        val px = IntArray(w * h)
        for (y in 0 until h) for (x in 0 until w) {
            val bit = (SuvBitmap.BITS[(y * w + x) / 8].toInt() shr (7 - x % 8)) and 1
            px[y * w + x] = if (bit == 1) -1 else 0
        }
        val small = Bitmap.createBitmap(px, w, h, Bitmap.Config.ARGB_8888)
        val big = Bitmap.createScaledBitmap(small, w * UP, h * UP, true)
        val bw = big.width; val bh = big.height
        val all = IntArray(bw * bh); big.getPixels(all, 0, bw, 0, 0, bw, bh)
        for (i in all.indices) {
            val a = (all[i] ushr 24) and 0xFF
            val s = ((a - 128) * 5 + 128).coerceIn(0, 255)
            all[i] = (s shl 24) or 0xFFFFFF
        }
        Bitmap.createBitmap(all, bw, bh, Bitmap.Config.ARGB_8888).extractAlpha()
    }
    fun draw(c: Canvas, bx: Float, by: Float, list: List<Frame.IconItem>) {
        c.save(); c.translate(bx, by)
        for (s in list) {
            p.color = s.color; p.style = Paint.Style.FILL; p.strokeCap = Paint.Cap.ROUND
            val a = s.a / 8f; val b = s.b / 8f; val cc = s.c / 8f; val d = s.d / 8f
            when (s.k) {
                1 -> { p.style = Paint.Style.STROKE; p.strokeWidth = s.e.toFloat(); c.drawLine(a, b, cc, d, p) }
                2 -> { path.reset(); path.moveTo(a, b); path.lineTo(cc, d); path.lineTo(s.f / 8f, s.g / 8f); path.close(); c.drawPath(path, p) }
                3 -> c.drawCircle(a, b, cc, p)
                4 -> { p.style = Paint.Style.STROKE; p.strokeWidth = s.e.toFloat(); c.drawCircle(a, b, cc - (s.e - 1) / 2f, p) }
                5 -> c.drawRect(a, b, cc + 1, d + 1, p)
                6 -> { r.set(a, b, cc + 1, d + 1); c.drawRoundRect(r, s.f / 8f, s.f / 8f, p) }
                7 -> {   // SUV bitmap: pivot = ground under the rear wheel at (a, b), c = px per unit x 256, d = angle x 100
                    val z = SuvBitmap.LEN_UNITS * (s.c / 256f) / SuvBitmap.W / UP
                    c.save(); c.translate(s.a / 8f, s.b / 8f); c.rotate(s.d / 100f); c.scale(z, z)
                    p.isFilterBitmap = true
                    c.drawBitmap(suv, -SuvBitmap.PIVOT_X * UP, -SuvBitmap.PIVOT_Y * UP, p)
                    c.restore()
                }
            }
        }
        p.strokeCap = Paint.Cap.BUTT
        c.restore()
    }
}
