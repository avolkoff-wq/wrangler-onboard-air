package air.wrangler

import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.LinearGradient
import android.graphics.Paint
import android.graphics.RadialGradient
import android.graphics.RectF
import android.graphics.Shader
import android.graphics.Typeface
import android.os.SystemClock
import android.view.HapticFeedbackConstants
import android.view.MotionEvent
import android.view.View
import kotlin.math.atan2
import kotlin.math.cos
import kotlin.math.hypot
import kotlin.math.min
import kotlin.math.sin

/**
 * The DinMeter knob on the phone. Drag the ring around: one step per 18 degrees (clockwise = right), a haptic tick each.
 * The centre is the knob button: tap = press, 0.5-2 s = medium press, held 2 s = hold (menu / back), with a ring filling.
 */
class KnobView(ctx: Context, private val onStep: (Int) -> Unit, private val onButton: (Int) -> Unit) : View(ctx) {
    var label = "PRESS"
        set(v) { if (field != v) { field = v; invalidate() } }
    /** the second line: what a hold does on this screen ("HOLD: MENU", "HOLD: BACK", "" = nothing) */
    var sub = "HOLD: MENU"
        set(v) { if (field != v) { field = v; invalidate() } }
    var accent = 0xFF30D158.toInt()
        set(v) { if (field != v) { field = v; invalidate() } }
    var enabledLink = false
        set(v) { if (field != v) { field = v; invalidate() } }
    /** the preset shown on the main screen (-1 elsewhere): the dial marks it and the pointer settles on it */
    var preset = -1
        set(v) { if (field != v) { field = v; invalidate() } }
    /** armed or running: the pointer takes the accent colour */
    var busy = false
        set(v) { if (field != v) { field = v; invalidate() } }

    private val dp = resources.displayMetrics.density
    private val p = Paint(Paint.ANTI_ALIAS_FLAG)
    private val r = RectF()
    private val medium = Typeface.create("sans-serif-medium", Typeface.NORMAL)
    private var angle = 0f            // drawn rotation of the ring
    private var lastA = 0f; private var acc = 0f
    private var mode = 0              // 0 none, 1 turning, 2 button
    private var downAt = 0L; private var holdSent = false
    private val STEP = 18f

    private val cx get() = width / 2f
    private val cy get() = height / 2f
    private val rad get() = min(width, height) / 2f - 10 * dp
    private val inner get() = rad * 0.58f   // the face is the button

    // drawn like the Wrangler JL climate-control knob (owner): a scale arc on the dash, a black rubber grip with fine ribs,
    // a thin chrome bezel, a matte black face with a pointer that turns; the face is the button
    private val jeepFont = Typeface.create(Typeface.create("sans-serif-condensed", Typeface.BOLD), 800, false)
    // owner: military style, the button is START / MENU. Engraved preset marks with stencil codes, a heavy steel ring
    // with straight knurling, a matte olive face with a raised pointer fin. No panel plate behind it (owner).
    private val CODES = arrayOf("CST", "RCV", "SND", "RCK", "TRL", "GRV", "RGH", "HWY")   // dial order, clockwise
    override fun onDraw(c: Canvas) {
        val R = rad; val x = cx; val y = cy
        val ring = R * 0.74f; val ringIn = R * 0.6f; val face = R * 0.58f
        // no panel plate (owner: remove the square plate): the knob and its marks sit on the black background
        // engraved preset marks and stencil codes
        val det = if (preset >= 0) (7 - preset) % 8 else -1
        p.typeface = jeepFont; p.textSize = R * 0.085f; p.letterSpacing = 0.1f
        for (i in 0..7) {
            val a = Math.toRadians((135 + i * 270.0 / 7)); val ca = cos(a).toFloat(); val sa = sin(a).toFloat()
            val on = i == det
            p.style = Paint.Style.STROKE; p.strokeWidth = (if (on) 3f else 2f) * dp; p.strokeCap = Paint.Cap.BUTT
            val dim = if (det < 0) 0x59 else 0xFF   // off the main screen the presets are not live
            p.color = if (on) accentLit() else (dim shl 24) or 0xD8D6C4
            c.drawLine(x + ring * 1.03f * ca, y + ring * 1.03f * sa, x + R * 0.79f * ca, y + R * 0.79f * sa, p)
            p.style = Paint.Style.FILL
            val tx = x + R * 0.905f * ca; val ty = y + R * 0.905f * sa + p.textSize * 0.36f
            val tw = p.measureText(CODES[i])
            p.color = 0x99000000.toInt(); c.drawText(CODES[i], tx - tw / 2, ty - 0.7f * dp, p)          // engraved: dark above, light below
            p.color = 0x33FFFFFF; c.drawText(CODES[i], tx - tw / 2, ty + 0.7f * dp, p)
            p.color = if (on) accentLit() else (dim shl 24) or 0xC9C7B4
            c.drawText(CODES[i], tx - tw / 2, ty, p)
        }
        p.letterSpacing = 0f
        // the pointer settles on the preset when the finger is off the knob
        if (det >= 0 && mode != 1) {
            val target = 135f + det * 270f / 7f + 90f
            var d = (target - angle) % 360f; if (d > 180) d -= 360; if (d < -180) d += 360
            if (kotlin.math.abs(d) > 0.3f) { angle += d * 0.25f; postInvalidateOnAnimation() } else angle += d
        }
        // shadow of the knob on the plate
        p.color = Color.BLACK
        p.shader = RadialGradient(x + R * 0.03f, y + R * 0.06f, ring * 1.15f, intArrayOf(0xAA000000.toInt(), 0x00000000), floatArrayOf(0.82f, 1f), Shader.TileMode.CLAMP)
        c.drawCircle(x + R * 0.03f, y + R * 0.06f, ring * 1.15f, p); p.shader = null
        // steel ring with straight knurling (advisor: the cross-hatch read as mesh): 90 radial ridges, each a light and a
        // dark stroke half a degree apart, over a sweep with two highlights; bevel rings inside and out. Turns with the knob.
        p.color = Color.BLACK
        p.shader = android.graphics.SweepGradient(x, y, intArrayOf(0xFF4A4C48.toInt(), 0xFF8A8C86.toInt(), 0xFF3A3B38.toInt(), 0xFF262724.toInt(), 0xFF3A3B38.toInt(), 0xFF8A8C86.toInt(), 0xFF2E2F2C.toInt(), 0xFF4A4C48.toInt()),
            floatArrayOf(0f, 0.125f, 0.25f, 0.375f, 0.5f, 0.625f, 0.75f, 1f))
        c.save(); c.rotate(-90f, x, y); c.drawCircle(x, y, ring, p); c.restore(); p.shader = null
        p.style = Paint.Style.STROKE; p.strokeWidth = 1f * dp
        for (i in 0 until 90) {
            val a0 = Math.toRadians((angle + i * 4).toDouble()); val a1 = Math.toRadians((angle + i * 4 + 0.9).toDouble())
            val r0 = ringIn + 1.5f * dp; val r1 = ring - 1.5f * dp
            p.color = 0x66FFFFFF; c.drawLine(x + (r0 * cos(a0)).toFloat(), y + (r0 * sin(a0)).toFloat(), x + (r1 * cos(a0)).toFloat(), y + (r1 * sin(a0)).toFloat(), p)
            p.color = 0x99000000.toInt(); c.drawLine(x + (r0 * cos(a1)).toFloat(), y + (r0 * sin(a1)).toFloat(), x + (r1 * cos(a1)).toFloat(), y + (r1 * sin(a1)).toFloat(), p)
        }
        p.strokeWidth = 1.2f * dp; p.color = 0xFF151614.toInt(); c.drawCircle(x, y, ring, p); c.drawCircle(x, y, ringIn, p)
        p.strokeWidth = 1f * dp; p.color = 0x33FFFFFF; r.set(x - ring + dp, y - ring + dp, x + ring - dp, y + ring - dp); c.drawArc(r, 200f, 140f, false, p)
        p.style = Paint.Style.FILL
        // matte olive face, slightly domed; pressed: sinks a little
        val pressed = mode == 2
        val fr = face * (if (pressed) 0.97f else 1f)
        p.color = Color.BLACK
        p.shader = RadialGradient(x - fr * 0.3f, y - fr * 0.35f, fr * 1.35f, 0xFF5E6446.toInt(), 0xFF2A2D1F.toInt(), Shader.TileMode.CLAMP)
        c.drawCircle(x, y, fr, p); p.shader = null
        // raised pointer fin from the centre to the rim, with a painted white line; amber while armed or running
        c.save(); c.rotate(angle, x, y)
        // the fin: from the hub to the rim, wider at the hub, with a soft shadow
        path.reset(); path.moveTo(x - fr * 0.085f + 2 * dp, y - fr * 0.06f + 2 * dp); path.lineTo(x - fr * 0.03f + 2 * dp, y - fr * 0.985f + 2 * dp); path.lineTo(x + fr * 0.03f + 2 * dp, y - fr * 0.985f + 2 * dp); path.lineTo(x + fr * 0.085f + 2 * dp, y - fr * 0.06f + 2 * dp); path.close()
        p.color = 0x55000000; p.maskFilter = android.graphics.BlurMaskFilter(4 * dp, android.graphics.BlurMaskFilter.Blur.NORMAL); c.drawPath(path, p); p.maskFilter = null
        path.reset(); path.moveTo(x - fr * 0.085f, y - fr * 0.06f); path.lineTo(x - fr * 0.03f, y - fr * 0.985f); path.lineTo(x + fr * 0.03f, y - fr * 0.985f); path.lineTo(x + fr * 0.085f, y - fr * 0.06f); path.close()
        p.color = Color.BLACK
        p.shader = LinearGradient(x - fr * 0.085f, 0f, x + fr * 0.085f, 0f, 0xFF6E7452.toInt(), 0xFF262918.toInt(), Shader.TileMode.CLAMP)
        c.drawPath(path, p); p.shader = null
        p.color = if (!enabledLink) 0xFF808070.toInt() else accent   // white idle, amber armed / running, red error
        c.drawRect(x - fr * 0.016f, y - fr * 0.95f, x + fr * 0.016f, y - fr * 0.5f, p)
        c.restore()
        // hold progress round the face
        if (pressed && !holdSent) {
            val held = SystemClock.uptimeMillis() - downAt
            if (held > 300) {
                val f = ((held - 300) / 1700f).coerceIn(0f, 1f)
                p.style = Paint.Style.STROKE; p.strokeWidth = 4 * dp; p.strokeCap = Paint.Cap.ROUND; p.color = 0xFFFFB020.toInt()
                r.set(x - (ringIn + face) / 2, y - (ringIn + face) / 2, x + (ringIn + face) / 2, y + (ringIn + face) / 2)
                c.drawArc(r, -90f, 360f * f, false, p)
                p.strokeCap = Paint.Cap.BUTT; p.style = Paint.Style.FILL
            }
            postInvalidateOnAnimation()
        }
        // stencil label: START / MENU (two lines), lower half of the face, under the fin's pivot
        p.typeface = jeepFont; p.letterSpacing = 0.16f; p.textSize = fr * 0.16f
        p.color = if (enabledLink) 0xFFE8E6D8.toInt() else 0xFF808070.toInt()
        p.style = Paint.Style.FILL_AND_STROKE; p.strokeWidth = p.textSize * 0.03f
        val l1 = label                    // the knob says what a press does now (START, STOP, OK, NEXT, SELECT)
        c.drawText(l1, x - p.measureText(l1) / 2, y + fr * 0.33f, p)
        p.textSize = fr * 0.11f; p.strokeWidth = p.textSize * 0.03f
        val held = pressed && SystemClock.uptimeMillis() - downAt > 300 && !holdSent
        p.color = if (held) 0xFFFFB020.toInt() else if (enabledLink) 0xFFBFBDAA.toInt() else 0xFF808070.toInt()
        if (p.measureText(sub) > fr * 1.5f) p.textSize *= fr * 1.5f / p.measureText(sub)
        c.drawText(sub, x - p.measureText(sub) / 2, y + fr * 0.52f, p)
        p.style = Paint.Style.FILL; p.letterSpacing = 0f
        // centre pivot
        p.color = 0xFF1E2016.toInt(); c.drawCircle(x, y, fr * 0.1f, p)
        p.color = 0xFF7A7E62.toInt(); c.drawCircle(x, y, fr * 0.05f, p)
    }
    private val path = android.graphics.Path()
    /** the lit colour: amber by default, whatever the activity set (white idle, amber armed / running, red error) */
    private fun accentLit() = if (accent == 0xFFF5F5F7.toInt()) 0xFFFFB020.toInt() else accent
    private fun mixC(a: Int, b: Int, k: Float): Int {
        fun ch(sh: Int) = (((a shr sh) and 0xFF) * (1 - k) + ((b shr sh) and 0xFF) * k).toInt().coerceIn(0, 255)
        return (0xFF shl 24) or (ch(16) shl 16) or (ch(8) shl 8) or ch(0)
    }

    private fun ang(e: MotionEvent) = Math.toDegrees(atan2((e.x - cx).toDouble(), -(e.y - cy).toDouble())).toFloat()

    override fun onTouchEvent(e: MotionEvent): Boolean {
        val d = hypot(e.x - cx, e.y - cy)
        when (e.actionMasked) {
            MotionEvent.ACTION_DOWN -> {
                if (d <= inner) { mode = 2; downAt = SystemClock.uptimeMillis(); holdSent = false; postDelayed(holdTick, 50); invalidate() }
                else if (d <= rad * 1.15f) { mode = 1; lastA = ang(e); acc = 0f }
                else return false
                parent.requestDisallowInterceptTouchEvent(true)
                return true
            }
            MotionEvent.ACTION_MOVE -> if (mode == 1) {
                val a = ang(e); var da = a - lastA
                if (da > 180) da -= 360; if (da < -180) da += 360
                lastA = a; acc += da; angle += da
                while (acc >= STEP) { acc -= STEP; onStep(1); performHapticFeedback(HapticFeedbackConstants.CLOCK_TICK) }
                while (acc <= -STEP) { acc += STEP; onStep(-1); performHapticFeedback(HapticFeedbackConstants.CLOCK_TICK) }
                invalidate()
            }
            MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> {
                if (mode == 2 && e.actionMasked == MotionEvent.ACTION_UP && !holdSent) {
                    val held = SystemClock.uptimeMillis() - downAt
                    onButton(if (held < 500) Ble.PRESS else Ble.MID)
                    performHapticFeedback(HapticFeedbackConstants.CONFIRM)
                }
                mode = 0; removeCallbacks(holdTick); invalidate()
            }
        }
        return true
    }

    private val holdTick = object : Runnable {
        override fun run() {
            if (mode != 2 || holdSent) return
            if (SystemClock.uptimeMillis() - downAt >= 2000) {
                holdSent = true; onButton(Ble.HOLD); performHapticFeedback(HapticFeedbackConstants.LONG_PRESS); invalidate(); return
            }
            postDelayed(this, 50)
        }
    }
}
