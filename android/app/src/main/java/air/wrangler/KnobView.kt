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
    // owner: military style, the button is START / MENU. An olive-drab panel with four screws, engraved preset marks
    // with stencil codes, a heavy steel ring with a diamond knurl, a matte olive face with a raised pointer fin.
    private val CODES = arrayOf("CST", "RCV", "SND", "RCK", "TRL", "GRV", "RGH", "HWY")   // dial order, clockwise
    override fun onDraw(c: Canvas) {
        val R = rad; val x = cx; val y = cy
        val ring = R * 0.74f; val ringIn = R * 0.6f; val face = R * 0.58f
        // panel plate
        val pl = R * 1.0f
        r.set(x - pl, y - pl, x + pl, y + pl)
        p.color = Color.BLACK
        p.shader = LinearGradient(x, y - pl, x, y + pl, 0xFF4A4F3A.toInt(), 0xFF2C2F23.toInt(), Shader.TileMode.CLAMP)
        c.drawRoundRect(r, R * 0.1f, R * 0.1f, p); p.shader = null
        p.style = Paint.Style.STROKE; p.strokeWidth = 1.5f * dp; p.color = 0xFF1C1E16.toInt(); c.drawRoundRect(r, R * 0.1f, R * 0.1f, p)
        p.style = Paint.Style.FILL
        for ((sx, sy) in listOf(-1f to -1f, 1f to -1f, -1f to 1f, 1f to 1f)) {   // screws with a slot
            val qx = x + sx * pl * 0.86f; val qy = y + sy * pl * 0.86f; val qr = R * 0.045f
            p.color = Color.BLACK
            p.shader = RadialGradient(qx - qr * 0.3f, qy - qr * 0.3f, qr * 1.3f, 0xFF9A9D90.toInt(), 0xFF4C4E45.toInt(), Shader.TileMode.CLAMP)
            c.drawCircle(qx, qy, qr, p); p.shader = null
            p.color = 0xFF2A2B26.toInt(); c.save(); c.rotate(35f + sx * 20f, qx, qy); c.drawRect(qx - qr * 0.8f, qy - qr * 0.12f, qx + qr * 0.8f, qy + qr * 0.12f, p); c.restore()
        }
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
        // steel ring with a diamond knurl, turning with the knob
        p.shader = android.graphics.SweepGradient(x, y, intArrayOf(0xFF5A5C58.toInt(), 0xFF2A2B29.toInt(), 0xFF6C6E69.toInt(), 0xFF222321.toInt(), 0xFF5A5C58.toInt()), null)
        c.drawCircle(x, y, ring, p); p.shader = null
        c.save()
        val clip = android.graphics.Path().apply { addCircle(x, y, ring, android.graphics.Path.Direction.CW); addCircle(x, y, ringIn, android.graphics.Path.Direction.CCW) }
        c.clipPath(clip)
        p.style = Paint.Style.STROKE; p.strokeWidth = 1.1f * dp
        for (i in 0 until 60) {   // two families of slanted lines make the diamonds
            for (dir in floatArrayOf(-1f, 1f)) {
                val a0 = Math.toRadians((angle + i * 6).toDouble()); val a1 = Math.toRadians((angle + i * 6 + dir * 9).toDouble())
                p.color = if (dir > 0) 0x55000000 else 0x30FFFFFF
                c.drawLine(x + (ringIn * cos(a0)).toFloat(), y + (ringIn * sin(a0)).toFloat(), x + (ring * cos(a1)).toFloat(), y + (ring * sin(a1)).toFloat(), p)
            }
        }
        c.restore()
        p.strokeWidth = 1.2f * dp; p.color = 0xFF1A1B19.toInt(); c.drawCircle(x, y, ring, p); c.drawCircle(x, y, ringIn, p)
        p.style = Paint.Style.FILL
        // matte olive face, slightly domed; pressed: sinks a little
        val pressed = mode == 2
        val fr = face * (if (pressed) 0.97f else 1f)
        p.color = Color.BLACK
        p.shader = RadialGradient(x - fr * 0.3f, y - fr * 0.35f, fr * 1.35f, 0xFF5E6446.toInt(), 0xFF2A2D1F.toInt(), Shader.TileMode.CLAMP)
        c.drawCircle(x, y, fr, p); p.shader = null
        // raised pointer fin from the centre to the rim, with a painted white line; amber while armed or running
        c.save(); c.rotate(angle, x, y)
        path.reset(); path.moveTo(x - fr * 0.11f, y - fr * 0.25f); path.lineTo(x - fr * 0.05f, y - fr * 0.97f); path.lineTo(x + fr * 0.05f, y - fr * 0.97f); path.lineTo(x + fr * 0.11f, y - fr * 0.25f); path.close()
        p.color = Color.BLACK
        p.shader = LinearGradient(x - fr * 0.11f, 0f, x + fr * 0.11f, 0f, 0xFF6E7452.toInt(), 0xFF262918.toInt(), Shader.TileMode.CLAMP)
        c.drawPath(path, p); p.shader = null
        p.color = if (!enabledLink) 0xFF808070.toInt() else accent   // white idle, amber armed / running, red error
        c.drawRect(x - fr * 0.018f, y - fr * 0.93f, x + fr * 0.018f, y - fr * 0.5f, p)
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
        val l1 = label; val l2 = "MENU"   // the knob says what a press does now (START, STOP, OK, NEXT, SELECT)
        c.drawText(l1, x - p.measureText(l1) / 2, y + fr * 0.33f, p)
        p.textSize = fr * 0.11f; p.strokeWidth = p.textSize * 0.03f
        val held = pressed && SystemClock.uptimeMillis() - downAt > 300 && !holdSent
        p.color = if (held) 0xFFFFB020.toInt() else if (enabledLink) 0xFFBFBDAA.toInt() else 0xFF808070.toInt()
        c.drawText("HOLD: $l2", x - p.measureText("HOLD: $l2") / 2, y + fr * 0.52f, p)
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
