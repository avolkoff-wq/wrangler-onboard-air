package air.wrangler

import android.content.Context
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
 * Draws the DinMeter screen from its display list, scaled to fit, same layout and colours.
 * Text is fitted into the box the firmware measured (width and font height), so it lands where it does on the device.
 * A horizontal swipe turns the knob: one step per 40 dp.
 */
class ScreenView(ctx: Context, private val onTurn: (Int) -> Unit) : View(ctx) {
    var frame: Frame? = null
        set(v) { field = v; invalidate() }
    var offline = true
        set(v) { field = v; invalidate() }

    private val p = Paint(Paint.ANTI_ALIAS_FLAG)
    private val regular = Typeface.create("sans-serif", Typeface.NORMAL)
    private val bold = Typeface.create("sans-serif", Typeface.BOLD)
    private val digits = Typeface.create("sans-serif-condensed", Typeface.BOLD)
    private val mono = Typeface.MONOSPACE
    private val r = RectF()
    private val path = Path()

    override fun onDraw(c: Canvas) {
        c.drawColor(Color.BLACK)
        val f = frame
        if (f == null || f.w == 0 || f.h == 0) {
            p.color = Color.GRAY; p.textSize = 16 * resources.displayMetrics.density; p.typeface = regular
            p.textAlign = Paint.Align.CENTER
            c.drawText("No screen yet", width / 2f, height / 2f, p)
            p.textAlign = Paint.Align.LEFT
            return
        }
        val s = min(width.toFloat() / f.w, height.toFloat() / f.h)
        val ox = (width - f.w * s) / 2; val oy = (height - f.h * s) / 2
        c.save(); c.translate(ox, oy); c.scale(s, s)
        p.color = f.bg; p.style = Paint.Style.FILL
        c.drawRect(0f, 0f, f.w.toFloat(), f.h.toFloat(), p)
        for (it in f.items) draw(c, it)
        c.restore()
        if (offline) { p.color = 0x99000000.toInt(); c.drawRect(ox, oy, ox + f.w * s, oy + f.h * s, p) }
        p.color = 0xFF333333.toInt(); p.style = Paint.Style.STROKE; p.strokeWidth = 2f
        c.drawRect(ox, oy, ox + f.w * s, oy + f.h * s, p); p.style = Paint.Style.FILL
    }

    private fun draw(c: Canvas, it: Frame.Item) {
        p.color = it.color; p.style = Paint.Style.FILL
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
            Frame.ICON -> icon(c, x, y, w, h, it.color)
            Frame.TEXT -> text(c, it)
        }
    }

    private fun text(c: Canvas, it: Frame.Item) {
        if (it.text.isEmpty() || it.h == 0) return
        p.typeface = when (it.aux) { 0, 1 -> mono; 2 -> regular; 6 -> digits; else -> bold }
        p.textScaleX = 1f
        // font height on the device = ascent + descent here
        p.textSize = 100f
        val fm = p.fontMetrics
        p.textSize = it.h * 100f / (fm.descent - fm.ascent)
        val m = p.fontMetrics
        val tw = p.measureText(it.text)
        if (tw > 0 && it.w > 0) p.textScaleX = (it.w / tw).coerceIn(0.6f, 1.6f)
        val w = it.w.toFloat(); val h = it.h.toFloat()
        val left = when (it.datum and 3) { 1 -> it.x - w / 2; 2 -> it.x - w; else -> it.x.toFloat() }
        val top = when {
            it.datum and 16 != 0 -> it.y + m.ascent          // baseline datum
            it.datum and 12 == 4 -> it.y - h / 2
            it.datum and 12 == 8 -> it.y - h
            else -> it.y.toFloat()
        }
        c.drawText(it.text, left, top - m.ascent, p)
        p.textScaleX = 1f
    }

    // preset icon: a simple SUV on a ground line (the scenery stays on the DinMeter)
    private fun icon(c: Canvas, x: Float, y: Float, w: Float, h: Float, col: Int) {
        val g = y + h * 0.85f
        p.color = 0xFF777777.toInt(); c.drawRect(x, g, x + w, g + 2, p)
        val bx = x + w * 0.15f; val bw = w * 0.7f
        p.color = if (col == Color.WHITE) 0xFFFFA500.toInt() else col
        path.reset()
        path.moveTo(bx, g - h * 0.18f); path.lineTo(bx + bw * 0.1f, g - h * 0.5f); path.lineTo(bx + bw * 0.75f, g - h * 0.5f)
        path.lineTo(bx + bw * 0.85f, g - h * 0.3f); path.lineTo(bx + bw, g - h * 0.28f); path.lineTo(bx + bw, g - h * 0.12f)
        path.lineTo(bx, g - h * 0.12f); path.close()
        c.drawPath(path, p)
        p.color = 0xFF222222.toInt()
        c.drawRect(bx + bw * 0.18f, g - h * 0.45f, bx + bw * 0.42f, g - h * 0.3f, p)
        c.drawRect(bx + bw * 0.46f, g - h * 0.45f, bx + bw * 0.7f, g - h * 0.3f, p)
        p.color = Color.WHITE
        c.drawCircle(bx + bw * 0.22f, g - h * 0.1f, h * 0.11f, p); c.drawCircle(bx + bw * 0.8f, g - h * 0.1f, h * 0.11f, p)
        p.color = Color.BLACK
        c.drawCircle(bx + bw * 0.22f, g - h * 0.1f, h * 0.05f, p); c.drawCircle(bx + bw * 0.8f, g - h * 0.1f, h * 0.05f, p)
    }

    // swipe = knob
    private var downX = 0f
    private var stepped = 0
    override fun onTouchEvent(e: MotionEvent): Boolean {
        val step = 40 * resources.displayMetrics.density
        when (e.actionMasked) {
            MotionEvent.ACTION_DOWN -> { downX = e.x; stepped = 0; return true }
            MotionEvent.ACTION_MOVE -> {
                val want = ((e.x - downX) / step).toInt()
                while (stepped < want) { onTurn(1); stepped++ }
                while (stepped > want) { onTurn(-1); stepped-- }
                return true
            }
            MotionEvent.ACTION_UP -> { if (abs(e.x - downX) < step / 4) performClick(); return true }
        }
        return super.onTouchEvent(e)
    }
    override fun performClick(): Boolean { super.performClick(); return true }
}
