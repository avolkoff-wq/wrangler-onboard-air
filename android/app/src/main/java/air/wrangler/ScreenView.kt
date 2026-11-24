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
 * Draws the DinMeter screen from its display list, scaled to fit, same layout and colours:
 *  - text is fitted into the box the firmware measured (width, font height), so it lands where it does on the device;
 *  - the big numbers (Font7 on the DinMeter) are drawn as 7-segment digits;
 *  - the preset icon is the DinMeter's own vector strokes (sent once, cached by key) and the same SUV bitmap.
 * A horizontal swipe across the screen turns the knob, one step per 40 dp.
 */
class ScreenView(ctx: Context, private val onTurn: (Int) -> Unit) : View(ctx) {
    var frame: Frame? = null
        set(v) { field = v; invalidate() }
    var offline = true
        set(v) { if (field != v) { field = v; invalidate() } }
    private val icons = HashMap<Int, List<Frame.IconItem>>()
    fun putIcon(key: Int, strokes: List<Frame.IconItem>) { icons[key] = strokes; invalidate() }

    private val p = Paint(Paint.ANTI_ALIAS_FLAG)
    private val regular = Typeface.create("sans-serif", Typeface.NORMAL)
    private val bold = Typeface.create("sans-serif", Typeface.BOLD)
    private val mono = Typeface.create("monospace", Typeface.BOLD)
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
        p.color = 0xFF151515.toInt(); r.set(ox - 6, oy - 6, ox + f.w * s + 6, oy + f.h * s + 6)
        c.drawRoundRect(r, 18f, 18f, p)
        c.save(); c.clipRect(ox, oy, ox + f.w * s, oy + f.h * s); c.translate(ox, oy); c.scale(s, s)
        p.color = f.bg; p.style = Paint.Style.FILL
        c.drawRect(0f, 0f, f.w.toFloat(), f.h.toFloat(), p)
        for (it in f.items) draw(c, it)
        c.restore()
        if (offline) { p.color = 0xAA000000.toInt(); c.drawRect(ox, oy, ox + f.w * s, oy + f.h * s, p) }
    }

    private fun draw(c: Canvas, it: Frame.Item) {
        p.color = it.color; p.style = Paint.Style.FILL; p.strokeCap = Paint.Cap.BUTT
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

    private fun text(c: Canvas, it: Frame.Item) {
        if (it.text.isEmpty() || it.h == 0) return
        p.typeface = when (it.aux) { 0, 1 -> mono; 2 -> regular; else -> bold }
        p.textScaleX = 1f; p.textSize = 100f
        val fm = p.fontMetrics
        p.textSize = it.h * 100f / (fm.descent - fm.ascent)
        val m = p.fontMetrics
        val tw = p.measureText(it.text)
        if (tw > 0 && it.w > 0) p.textScaleX = (it.w / tw).coerceIn(0.7f, 1.4f)
        val b = box(it)
        val mw = p.measureText(it.text)
        val left = when (it.datum and 3) { 1 -> b.centerX() - mw / 2; 2 -> b.right - mw; else -> b.left }   // keep the anchor
        c.drawText(it.text, left, b.top - m.ascent, p)
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
